# esp32s2_net — ESP32-S2 PSRAM 与 Wi-Fi 共存问题解决记录

## 项目概况

| 项目 | 说明 |
| --- | --- |
| 芯片 | ESP32-S2（开发板：ESP32-S2-Kaluga-1，WROVER N4R2 模组，4MB Flash + 2MB PSRAM） |
| 系统 | Zephyr **v3.7.2 LTS**（树位于 `/home/mojo/zephyrproject/zephyr`） |
| 功能 | LVGL 显示（ST7789V） + Wi-Fi 连接 + 外部 PSRAM 扩展内存 |
| 自建 board | `esp32s2_net`（本工程 `boards/` 目录，板级 dts 用 solo include，overlay 里 include wrover dtsi 补 PSRAM） |

**结论：PSRAM 与 Wi-Fi 可以共存。** 最终在硬件上验证通过：PSRAM 正常初始化（2MB @ 40MHz），`wifi scan` 扫描到 9 个热点，连接 `QHDTUC2.4` 成功，DHCP 拿到 `192.168.1.149`。

---

## 问题 1（核心）：开启 PSRAM 后 Wi-Fi 无法扫描到热点

### 现象

- 不开 PSRAM：Wi-Fi 扫描、连接、拿 IP 全部正常；
- 开启 PSRAM（`CONFIG_ESP_SPIRAM=y`）：`wifi scan` 返回空列表（`No Wi-Fi AP found`），连接也失败。

### 原因（四层递进）

**第 1 层：ESP32-S2 的硬件限制 —— PSRAM 不能做 DMA。**

Wi-Fi MAC 的 DMA 引擎只能访问内部 SRAM，地址范围定义在：

> `modules/hal/espressif/components/soc/esp32s2/include/soc/soc.h:191`
> ```c
> #define SOC_DMA_LOW  0x3FFB0000   // 内部 SRAM 起点
> #define SOC_DMA_HIGH 0x40000000   // 内部 SRAM 终点
> ```

PSRAM 位于 `0x3F800000`，只有 CPU 能通过 cache 访问。任何落入 PSRAM 的 DMA 缓冲（Wi-Fi 收发包缓冲、GDMA 的 SPI 缓冲）都会失效。ESP-IDF 官方文档 "External RAM" 页面也明确 ESP32-S2 的外部 RAM 不具备 DMA 能力。

**第 2 层：Zephyr 的 esp 堆实现无视内存类型（caps）。**

> `modules/hal/espressif/zephyr/port/heap/heap_caps_zephyr.c:83`

```c
void *heap_caps_malloc(size_t size, uint32_t caps)  // caps 参数被直接忽略！
{
    return heap_caps_malloc_base(size, caps);        // 内部就是 k_malloc(size)
}
```

ESP-IDF 里 Wi-Fi 固件申请 DMA 缓冲时会带 `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL`，保证拿到内部 SRAM；Zephyr 的移植把 caps 全丢了。

**第 3 层：开启 PSRAM 后 k_malloc 被 `__wrap_` 重定向，可能静默返回 PSRAM 内存。**

> `modules/hal/espressif/zephyr/port/heap/heap_caps_zephyr.c:334`（`__wrap_k_malloc`）

开启 `CONFIG_ESP_SPIRAM` 后，链接器把 `k_malloc`/`k_calloc` 包装（链接命令里可见 `-Wl,--wrap=k_malloc`），按两条规则路由：

1. `请求 >= CONFIG_ESP_HEAP_MIN_EXTRAM_THRESHOLD` → 优先从 PSRAM 堆分配；
2. 内部堆耗尽时，若 `CONFIG_ESP_HEAP_SEARCH_ALL_REGIONS=y`（默认开启）→ **静默回退到 PSRAM 堆**。

**第 4 层：开启 PSRAM 后 Wi-Fi 固件会多要 32×1600B 的 cache-TX 缓冲，掏空内部堆，触发上述回退。**

> `modules/hal/espressif/zephyr/esp32s2/CMakeLists.txt:163`
> ```cmake
> if (CONFIG_ESP_SPIRAM)
>     zephyr_compile_definitions(CONFIG_SPIRAM)   # 向 hal 注入 CONFIG_SPIRAM 宏
> ```

> `modules/hal/espressif/zephyr/esp32s2/src/wifi/esp_wifi_adapter.c:50`
> ```c
> uint64_t g_wifi_feature_caps =
> #if CONFIG_SPIRAM
>     CONFIG_FEATURE_CACHE_TX_BUF_BIT |   // 开启 PSRAM 就自动置位！
> #endif
> ```

Wi-Fi 固件看到 `CACHE_TX_BUF_BIT` 后，会为"TX 重传缓存"申请 `WIFI_CACHE_TX_BUFFER_NUM`（默认 32）× 1600B ≈ 51KB 的缓冲。本工程内部堆只有 60KB（见"内部堆构成"一节），一下就被掏空。之后固件再申请收包 DMA 缓冲（`wDev_Rxbuf_Init` 的 1604B 分配，见 issue #74246 的调用栈分析）时，触发第 3 层的 PSRAM 回退 → **DMA 缓冲落在 PSRAM → MAC 收不到任何帧 → 扫描自然一无所获**。

### 解决办法

**1) 给 hal_espressif 模块打一个小 patch（本工程 `patches/` 目录已保存，共两处修改）：**

修改 `modules/hal/espressif/zephyr/esp32s2/src/wifi/esp_wifi_adapter.c`：

- 去掉 `g_wifi_feature_caps` 里的 `CONFIG_FEATURE_CACHE_TX_BUF_BIT`（消除 51KB 的无谓内存消耗）；
- `malloc_internal_wrapper` / `calloc_internal_wrapper` / `zalloc_internal_wrapper` 改用 **`k_aligned_alloc()`**。

`k_aligned_alloc()` 没有被 `__wrap_` 包装，永远从内部堆（`_system_heap`）分配。这样 Wi-Fi 固件的 `_malloc_internal` 系列钩子拿到的内存**保证在内部 SRAM**，与 ESP-IDF 的 `MALLOC_CAP_INTERNAL` 语义一致。

**2) prj.conf 配置（本工程已配好）：**

```ini
CONFIG_ESP_SPIRAM=y
CONFIG_ESP_SPIRAM_HEAP_SIZE=1048576          # 1MB 的 PSRAM 附加堆
CONFIG_ESP_HEAP_MIN_EXTRAM_THRESHOLD=32768   # >=32KB 的分配才走 PSRAM
# 不启用 CONFIG_ESP32_WIFI_NET_ALLOC_SPIRAM（网络 .noinit 缓冲落 PSRAM，DMA 必挂）
```

threshold 保持 32768 的原因：屏幕 SPI 驱动的 DMA 临时缓冲最大 4092 字节（`zephyr/drivers/spi/spi_esp32_spim.c:87` 的 `tx_temp = k_malloc(dma_len_tx)`），必须留在内部 RAM。应用想用 PSRAM 时，直接 `k_malloc(>= 32KB)` 即可命中 PSRAM。

### 参考资料

- **GitHub issue #74246**（zephyrproject-rtos/zephyr）"ESP32-S3 WiFi does not work if external SPIRAM is enabled" —— 与本问题完全同源，S3/S2/ESP32 均有复现；其中 vigen1980 的 `k_aligned_alloc` 方案、marekmatej 的去掉 `CACHE_TX_BUF_BIT` 方案，即本工程采用的修法（均在硬件上验证过）。https://github.com/zephyrproject-rtos/zephyr/issues/74246
- `heap_caps_zephyr.c`（caps 被忽略 + `__wrap_k_malloc` 阈值/回退逻辑）：`modules/hal/espressif/zephyr/port/heap/heap_caps_zephyr.c`
- DMA 范围宏：`modules/hal/espressif/components/soc/esp32s2/include/soc/soc.h:191`
- ESP-IDF "External RAM" 文档（PSRAM 的 DMA 能力表）：https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-guides/external-ram.html
- 相关跟进 issue #109602（main 分支上对 PSRAM+Wi-Fi 堆映射/链接脚本的官方修复，v3.7.2 未包含）：https://github.com/zephyrproject-rtos/zephyr/issues/109602

---

## 问题 2：开启 `CONFIG_SPIRAM_FETCH_INSTRUCTIONS` / `CONFIG_SPIRAM_RODATA` 链接失败

### 现象

```
undefined reference to `mmu_config_psram_text_segment'
undefined reference to `mmu_config_psram_rodata_segment'
```

### 原因

这两个选项在 ESP-IDF 里表示"把 flash 中的代码/只读数据搬到 PSRAM"。开启后 `esp_psram.c` 会调用这两个函数：

> `modules/hal/espressif/components/esp_psram/esp_psram.c:146` 和 `:168`

但 Zephyr v3.7.2 的 hal **只在 esp32s3 里实现了它们**：

> `modules/hal/espressif/zephyr/esp32s3/CMakeLists.txt:232` 才编译 `mmu_psram_flash.c`，
> `zephyr/esp32s2/CMakeLists.txt` 里没有这个文件。

### 解决办法

ESP32-S2 上**不要开启**这两个选项（本工程 prj.conf 已用注释说明）。

### 参考资料

- `components/esp_psram/esp_psram.c`（调用点）、`components/esp_psram/mmu_psram_flash.c`（实现，仅 S3 编译）、`zephyr/esp32s3/CMakeLists.txt:232`。

---

## 问题 3：开启 `CONFIG_ESP32_WIFI_IRAM_OPT` 链接溢出

### 现象

```
ld: zephyr.elf section `.dram0.noinit' will not fit in region `dram0_0_seg'
ld: region `dram0_0_seg' overflowed by 6528 bytes
```

### 原因

ESP32-S2 的 IRAM 与 DRAM 是**同一块 320KB SRAM 的两个总线视角**（本工程用户可用约 224KB）。链接脚本里 DRAM 的起始位置要跳过 IRAM 已用掉的部分：

> `zephyr/soc/espressif/esp32s2/default.ld` 的 `.dram0.dummy` 段：
> `ORIGIN(dram0_0_seg) + MAX(_iram_end, user_iram_seg_org) - user_iram_seg_org`

开启 `CONFIG_ESP32_WIFI_IRAM_OPT` 后，Wi-Fi blob（libnet80211/libpp）的 IRAM 优化段被拉入 IRAM（实测增加约 12.5KB），DRAM 数据段整体上移，与 LVGL 的静态缓冲（VDB 约 77KB）抢空间，最终 `.dram0.noinit` 溢出约 6.5KB。IRAM 优化本来是提速手段，但在"LVGL + WiFi + PSRAM"三件套下内部 SRAM 已到极限，只能放弃。

### 解决办法

```ini
CONFIG_ESP32_WIFI_IRAM_OPT=n
CONFIG_ESP32_WIFI_RX_IRAM_OPT=n
```

Wi-Fi 代码留在 flash 中经 icache 执行，功能不受影响。

### 参考资料

- `zephyr/soc/espressif/esp32s2/default.ld`（`dram0.dummy` 的 MAX 逻辑、Wi-Fi IRAM 段映射）
- `zephyr/soc/espressif/esp32s2/memory.h`（`SRAM_CACHE_SIZE`、`DRAM_BUFFERS_START` 等布局常量）

---

## 问题 4：串口控制台日志每条输出两遍（minicom 里看着重复）

### 现象

```text
uart:~$ [00:00:00.845,000] <inf> main: display blanking off
[00:00:00.945,000] <inf> main: Hello Zephyr.
[00:00:00.845,000] <inf> main: display blanking off     <- 重复
[00:00:00.945,000] <inf> main: Hello Zephyr.            <- 重复
uart:~$
```

### 原因

不是 minicom 的问题，固件确实把每条日志写了两遍：**同时启用了两个日志后端**。

1. `CONFIG_LOG_BACKEND_UART=y`（prj.conf 显式开启）—— UART 后端直接打印一份；
2. `CONFIG_SHELL_LOG_BACKEND=y`（`subsys/shell/Kconfig:272`，`LOG` 开启时**默认 y**）—— shell 自己也是一个日志后端，再打印一份。

shell 后端的这一份还带提示符擦除/重画的 VT100 转义序列（原始字节流里能看到 `ESC[8D ESC[J` —— 光标回退 8 列（提示符 "uart:~$ " 恰好 8 个字符）再擦到行尾）。minicom 对这些序列处理不完整，两份就都显示出来了。

### 解决办法

在 prj.conf 里关掉 shell 日志后端，保留 UART 后端：

```ini
CONFIG_SHELL_LOG_BACKEND=n
```

保留 UART 后端而不是 shell 后端的原因：shell 后端要等 shell 初始化完成后才生效，**早期启动日志会丢失**（本工程排查启动问题依赖早期日志，`CONFIG_LOG_MODE_IMMEDIATE` 也是为此）。

### 参考资料

- `zephyr/subsys/shell/Kconfig:272`（`SHELL_LOG_BACKEND`，`default y if LOG`）
- `zephyr/subsys/shell/backends/shell_uart.c`（提示符擦除/重画逻辑）

---

## 附加知识：内部堆的真实构成（排障时的关键认知）

`CONFIG_HEAP_MEM_POOL_SIZE` **并不等于**全部可用堆，ESP32-S2 上有两个独立的堆：

| 堆 | 大小 | 来源 |
| --- | --- | --- |
| `k_malloc` 内部堆（`_system_heap`） | 61440 B（=`CONFIG_HEAP_MEM_POOL_SIZE` 的静态池） | `zephyr/kernel/mempool.c` 的 `K_HEAP_DEFINE(_system_heap, K_HEAP_MEM_POOL_SIZE)` |
| libc `malloc` 堆（`z_malloc_heap`） | 10448 B（=`_heap_sentry - _end`，本构建） | `zephyr/lib/libc/common/source/stdlib/malloc.c:106`，Xtensa 上以 `_heap_sentry` 为上界 |

Wi-Fi 固件的内存全部来自第一个堆（60KB）。修复后固件"必须内部"的分配约 25KB（上游实测值），60KB 有充足余量。可用 `kernel heap` shell 命令查看 `_system_heap` 使用情况。

当前构建内存占用（map 实测）：

```
dram0_0_seg: 213552 / 229376  (93%)
iram 使用:   ~61KB（IRAM/DRAM 共享池，剩约 166KB）
```

---

## hal patch 的工作原理与使用

### 为什么需要补丁

`hal_espressif` 是 west 管理的第三方模块（独立 git 仓库，路径 `/home/mojo/zephyrproject/modules/hal/espressif`，由 `zephyr/west.yml` 指定 revision）。本工程不能直接提交修改到该仓库，而 Zephyr v3.7.2 又没有包含上游修复，所以只能在本工程里保存一个补丁文件，构建前先打到模块上。

### 补丁内容（两个 hunk，只改一个文件）

文件：`zephyr/esp32s2/src/wifi/esp_wifi_adapter.c`（共 36 增 / 6 删）：

| Hunk | 修改点 | 作用 |
| --- | --- | --- |
| 1 | `g_wifi_feature_caps` 去掉 `CONFIG_FEATURE_CACHE_TX_BUF_BIT` | 消除开启 PSRAM 后固件多申请的 32×1600B cache-TX 缓冲（问题 1 第 4 层） |
| 2 | `malloc/calloc/zalloc_internal_wrapper` 改用 `k_aligned_alloc()` | 保证 Wi-Fi 固件的 DMA 缓冲永远落在内部 SRAM（问题 1 第 3 层） |

补丁文件是 **git 统一 diff 格式（unified diff）**——由 `git diff` 生成，`git apply` 依据每个 hunk 的上下文（前后几行 + 行号）定位修改点。它的效果与手工编辑完全等价，但可重复、可追溯、可随 `west update` 反复重打。

### 使用指令

```bash
HAL=/home/mojo/zephyrproject/modules/hal/espressif
PATCH=/home/mojo/Projects/zephyr/esp32s2_net/patches/hal_espressif-0001-esp32s2-wifi-psram-internal-alloc.patch

# 1) 查看模块当前是否有未提交改动
git -C $HAL status --short
#    （打过补丁时会显示  M zephyr/esp32s2/src/wifi/esp_wifi_adapter.c）

# 2) 应用前预检（--check 只检查不修改，成功后无输出、退出码 0）
git -C $HAL apply --check $PATCH

# 3) 应用补丁
git -C $HAL apply $PATCH

# 4) 反向预检：判断"是否已经打过"（退出码 0 = 已打过，可跳过应用）
git -C $HAL apply --check --reverse $PATCH

# 5) 重新构建（补丁改动会被 ccache/CMake 依赖追踪，增量编译即可）
cd /home/mojo/zephyrproject
west build -b esp32s2_net /home/mojo/Projects/zephyr/esp32s2_net \
           -d /home/mojo/Projects/zephyr/esp32s2_net/build
```

**推荐直接用本工程自带的幂等脚本**（内部做了"未打 → 打上；已打 → 跳过；冲突 → 报错"三态判断）：

```bash
bash patches/apply-hal-patch.sh
```

`west update`（或 `west update hal_espressif`）会重置模块到 west.yml 指定的 revision，**补丁会丢失**，之后重新执行上面第 3 步或脚本即可。

### 什么时候不需要这个补丁

- **一直停留在 Zephyr v3.7.2 且从不执行 `west update`**：补丁只是 hal 模块工作区里的一处修改，构建过程不会触碰它，没有任何机制会自动撤销——不需要重打。会使其丢失的操作只有：`west update`（整仓或 `west update hal_espressif`）、重装/迁移 zephyrproject 目录、手动 `git checkout/reset` 该模块。脚本保留作为保险即可。
- **升级到 Zephyr v4.4.x（4.4.0/4.4.1/4.4.2 均已确认）**：上游已重构了整套 Wi-Fi 内存模型（对应 issue #109602 的修复）：
  - `drivers/wifi/esp32/Kconfig.esp32` 新增 `ESP_WIFI_HEAP` 选项：`SYSTEM`（默认，WiFi 走 k_malloc 且自动为内部堆追加 51200B）或 `SPIRAM`（WiFi 走专用 spiram 堆 `smh_malloc`）；
  - hal 的 `heap_caps_zephyr.c` 已删除 `__wrap_k_malloc` 的阈值/回退路由；
  - 适配器里的 `CACHE_TX_BUF_BIT` 已移除。
  
  因此本补丁在 4.4.x 上**既不需要、也无法直接应用**（目标文件已不在旧路径，`git apply` 会直接报冲突）。升级后建议仍实测一遍 `wifi scan`/连接，并留意 S2 的物理内存约束（IRAM_OPT 与 DRAM 共享 320KB 池）在链接阶段的表现。

### 补丁失效（冲突）时怎么办

hal_espressif 升级后如果行号/上下文对不上，`git apply` 会报 `error: patch does not apply`：

1. 用 `git -C $HAL apply --check $PATCH` 查看具体是哪个 hunk 失败；
2. 打开 `esp_wifi_adapter.c`，对照补丁文件里的两个修改点手工重做（内容详见 `patches/` 下的 .patch 文件，就是加注释、删 CACHE_TX_BUF_BIT、三个 wrapper 改 k_aligned_alloc）；
3. 重新生成补丁文件：
   ```bash
   git -C $HAL diff > patches/hal_espressif-0001-esp32s2-wifi-psram-internal-alloc.patch
   ```
4. 重新构建 + 硬件验证（`wifi scan` 能列出热点、能连接拿到 IP 即通过）。

---

## 构建与烧录

```bash
cd /home/mojo/zephyrproject
west build -b esp32s2_net /home/mojo/Projects/zephyr/esp32s2_net \
           -d /home/mojo/Projects/zephyr/esp32s2_net/build
west flash -d /home/mojo/Projects/zephyr/esp32s2_net/build
# 串口控制台：/dev/ttyUSB1 @ 115200（复位：DTR 拉低 150ms 再拉高）
```

shell 验证命令：`wifi scan`、`wifi connect QHDTUC2.4 6 2`、`kernel heap`、`net iface`。

---

## 硬件验证结果（2026-09-15）

```text
I (195) esp_psram: Found 2MB PSRAM device
I (195) esp_psram: Speed: 40MHz
[00:00:02.988] <inf> main: Wi-Fi 连接请求已发出，等待结果...
[00:00:07.658] <inf> net_dhcpv4: Received: 192.168.1.149
[00:00:07.659] <inf> main: wifi connected

uart:~$ wifi scan
Num  | SSID                             (len) | Chan (Band)   | RSSI | Security
1    | QHDTUC2.4                        9     | 6    (2.4GHz) | -52  | WPA2-PSK
2    | A                                1     | 1    (2.4GHz) | -76  | UNKNOWN
...
9    | SUNREAL_AP                       10    | 11   (2.4GHz) | -90  | UNKNOWN
Scan request done
```

---

## 参考链接汇总

| 资料 | 位置 |
| --- | --- |
| Zephyr issue #74246（根因 + 两种修法，本工程修法来源） | https://github.com/zephyrproject-rtos/zephyr/issues/74246 |
| Zephyr issue #74899（S2 扫描问题的另一个历史 bug：wifi clock gate，v3.7 已修复） | https://github.com/zephyrproject-rtos/zephyr/issues/74899 |
| Zephyr issue #109602（PSRAM+Wi-Fi 堆映射官方修复，main 分支） | https://github.com/zephyrproject-rtos/zephyr/issues/109602 |
| ESP-IDF External RAM 文档（PSRAM DMA 能力、cache TX buffer 设计） | https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-guides/external-ram.html |
| heap 包装与阈值：`__wrap_k_malloc` / `heap_caps_malloc` | `modules/hal/espressif/zephyr/port/heap/heap_caps_zephyr.c` |
| Wi-Fi 适配器（feature_caps / internal wrapper，patch 对象） | `modules/hal/espressif/zephyr/esp32s2/src/wifi/esp_wifi_adapter.c` |
| DMA 地址范围宏 | `modules/hal/espressif/components/soc/esp32s2/include/soc/soc.h:191` |
| S2 链接脚本（IRAM/DRAM 共享池） | `zephyr/soc/espressif/esp32s2/default.ld` |
| S2 内存布局常量 | `zephyr/soc/espressif/esp32s2/memory.h` |
| PSRAM Kconfig（阈值默认 8192，范围 1024~131072） | `zephyr/soc/espressif/common/Kconfig.spiram` |
