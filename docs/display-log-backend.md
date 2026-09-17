# 屏幕日志面板实现记录(自定义 Zephyr 日志后端)

把串口日志实时滚动显示在 LCD 底部半透明面板上。实现位于 `src/lv_log.c`(日志后端)+ `src/lv_log.h`(接口),由 `main.c` 的 `lv_log_init()` 一行接入。

## 数据流

```
任意线程 LOG_XXX()
    │ (LOG_MODE_DEFERRED: 消息进入日志队列)
    ▼
logging 线程 → 后端 process()                    ← 自定义后端, 与 UART 后端并行
    │  log_format_func_t_get(0) = 标准文本格式化
    ▼
log_output 格式化(时间戳+级别+模块名+正文)
    ▼
char_out() 字符回调 ──spinlock──▶ PSRAM 行环(8行×112B)
    ▼
LVGL 定时器(300ms, LVGL 上下文)
    ▼
底部面板 lv_label(最近 4 行, 超长截断加 ..)
```

## 关键实现点

### 1. 自定义日志后端的最小骨架

Zephyr 3.7 的后端 API(`include/zephyr/logging/log_backend.h`)只有 7 个回调,做一个"把日志转发到别处"的后端只需要其中 3 个:

```c
static const struct log_backend_api log_backend_lvlog_api = {
	.process = process,      /* 每条日志经这里 */
	.panic = panic,          /* 系统 panic 时的兜底 */
	.init = lv_log_backend_init,
	.is_ready = is_ready,    /* 返回 1 */
	.format_set = format_set,
};
LOG_BACKEND_DEFINE(log_backend_lvlog, log_backend_lvlog_api, false, NULL);
/* autostart=false, 运行时手动 log_backend_activate(&log_backend_lvlog, NULL) */
```

`process()` 里**不要自己解析消息**——直接复用标准格式化:

```c
static void process(const struct log_backend *const backend, union log_msg_generic *msg)
{
	uint32_t flags = log_backend_std_get_flags();          /* 时间戳+级别等标志 */
	log_format_func_t f = log_format_func_t_get(0);        /* 0 = LOG_OUTPUT_TEXT */
	f(&lv_log_output, &msg->log, flags);                   /* 格式化后逐块回调 char_out */
}
```

配套的 `lv_log_output` 用宏生成,`char_out` 是输出回调:

```c
LOG_OUTPUT_DEFINE(lv_log_output, char_out, log_buf, sizeof(log_buf));
```

### 2. ⚠️ char_out 的返回值契约(本次最大的坑)

`log_output_func_t` 要求回调返回**已消费的字节数**,`buffer_write()` 用它推进指针:

```c
static void buffer_write(log_output_func_t outf, uint8_t *buf, size_t len, void *ctx)
{
	while (len != 0) {
		processed = outf(buf, len, ctx);   /* 返回值 = 已处理字节数 */
		len -= processed;
		buf += processed;                  /* 用返回值推进指针 */
	}
}
```

第一版把 `char_out` 写成了 `void`。Xtensa 上 void 函数的 A2 返回寄存器残留垃圾值(实测 `0x80000000`),于是:

1. 第一次调用正常处理;
2. `len -= 0x80000000` → 长度变成一个天文数字;
3. `buf += 0x80000000` → 指针变成 `0x80000000`;
4. 第二次循环 `l8ui a4, a2, 0` 从 0x80000000 取字节 → **FATAL EXCEPTION (load prohibited), VADDR=0x80000000**。

崩溃 dump 里的 `VADDR 0x80000000` 就是这个残留值,寄存器痕迹与反汇编完全吻合。修复:`static int char_out(...) { ...; return length; }`。

> 经验:凡是"回调给库调用"的函数,先查头文件里的函数指针类型,签名(尤其返回值)必须完全一致;C 不会为函数指针类型不匹配报错。

### 3. 线程模型与锁

`LOG_MODE_DEFERRED` 下 `process()` 运行在 **logging 线程**;若切到 `LOG_MODE_IMMEDIATE`,则运行在**日志产生者上下文**(任意线程,甚至 ISR)。所以 `char_out` 内部的行环用 `k_spinlock` 保护(ISR 安全),且不能调用任何会阻塞的 API。

### 4. 缓冲放 PSRAM,不占内部 DRAM

内部 DRAM 已用 98%,所以后端所有缓冲都放 PSRAM:

```c
static char ring[8][113] __attribute__((section(".ext_ram.bss")));   /* 行环 904B */
static char cur_line[113]  __attribute__((section(".ext_ram.bss")));
static char disp_buf[189]  __attribute__((section(".ext_ram.bss")));
```

日志文本只被 CPU 读写、不涉及 DMA → 放 PSRAM 安全(判断方法见 README「内存腾挪实践」)。链接后从 map 验证:三个对象都落在 `0x3f80xxxx` 段。

### 5. 中文显示限制

LVGL 的 montserrat 字体**没有中文字形**,中文日志在面板上显示为空白。两种解法:

1. **日志统一用英文**(当前采用):串口和屏幕都显示英文,一致、零成本;
2. 用中文字体:项目里的 `yahei_14` 字库只有 16 个字(仅覆盖 UI 诗句),需要用 `gen_font.py` 把日志常用字扩充进字库,再把面板字体换成它。

## 验证(2026-09-17)

- 串口日志与屏幕日志并行输出,互不影响(UART 后端 + 显示后端两个独立后端);
- 启动 → WiFi → MQTT 全过程日志在面板滚动,45 秒压测无崩溃;
- 断开/重连 broker、信号变化等事件都会实时出现在面板上。

## 相关参考

- `include/zephyr/logging/log_backend.h` — 后端 API 定义
- `subsys/logging/backends/log_backend_uart.c` — 最简参考实现(process/char_out 模式)
- `subsys/logging/log_output.c` — `buffer_write()` 返回值契约、`out_func` 的 immediate/deferred 分支
- `include/zephyr/logging/log_output.h` — `LOG_OUTPUT_DEFINE`、`log_output_func_t` 类型
