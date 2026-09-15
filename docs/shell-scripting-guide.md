# Shell 脚本自动化指南(嵌入式开发工作向)

面向日常开发工作的 bash 脚本速成指南:把重复的 shell 命令封装成脚本,构建、烧录、查日志、批量操作一次搞定。文中所有例子都在本机验证过,可直接改一改就用。

---

## 1. 什么时候值得写脚本

判断标准很简单:**同一条(或一组)命令,你已经手敲过第三次**。比如:

```bash
cd /home/mojo/zephyrproject
west build -b esp32s2_net /home/mojo/Projects/zephyr/esp32s2_net -d /home/mojo/Projects/zephyr/esp32s2_net/build
west flash -d /home/mojo/Projects/zephyr/esp32s2_net/build
stty -F /dev/ttyUSB1 115200 raw -echo && timeout 30 cat /dev/ttyUSB1
```

三条命令 → 写进一个文件 `flash.sh` → 以后一条 `./flash.sh` 完事。省时间只是其一,更重要的是**不会漏步骤、不会敲错**。

## 2. 最简脚本与三种执行方式

```bash
#!/bin/bash
echo "hello, $USER, 当前目录是 $(pwd)"
```

| 方式 | 命令 | 说明 |
| --- | --- | --- |
| 交给解释器执行 | `bash hello.sh` | 不需要执行权限,`#!/bin/bash` 这行不生效 |
| 直接执行 | `chmod +x hello.sh && ./hello.sh` | 依赖 shebang(`#!` 行)找解释器 |
| 变成"命令" | 把脚本放进 `~/.local/bin/`(已在 PATH) | 任意目录直接敲 `hello.sh` |

语法检查与调试(先记住这两条,写脚本必备):

```bash
bash -n script.sh     # 只检查语法,不执行
bash -x script.sh     # 执行并打印每条命令(排错神器)
```

## 3. 核心语法(每个都配开发场景)

### 3.1 变量、引号、命令替换

```bash
BOARD=esp32s2_net          # 赋值=两侧不能有空格!
echo $BOARD                # 取值加 $
NAME="hello world"         # 值含空格必须加引号
echo "$NAME"               # 双引号:保留为一个整体,内部 $ 变量会展开
echo '$NAME'               # 单引号:纯字面量,不展开(输出 $NAME)
NOW=$(date '+%H:%M:%S')    # $( ) 命令替换:取命令输出
BUILD_DIR="${BUILD_DIR:-build}"   # 变量为空/未定义时用默认值 build
LOG=${FILE%.log}.txt       # 去掉后缀 .log 再加 .txt
```

> 开发中最常用的是最后两行:`${VAR:-默认值}` 让脚本"有默认值、可被覆盖";`$(命令)` 取命令输出。

### 3.2 脚本参数

```bash
#!/bin/bash
BOARD="${1:-esp32s2_net}"        # $1=第一个参数,没传就用默认值
APP="${2:-/home/mojo/Projects/zephyr/esp32s2_net}"
echo "board=$BOARD app=$APP 参数个数=$# 全部参数=$*"
```

用法:`./build.sh esp32s2_net /path/to/app`。`$0` 是脚本自身路径,`$#` 是参数个数,`$@`/`$*` 是所有参数。

### 3.3 退出码与条件判断

**一切命令都有退出码:0=成功,非 0=失败**,上一个命令的退出码存在 `$?` 里。

```bash
west build -b esp32s2_net .      # 构建失败会返回非 0
if [ $? -ne 0 ]; then            # -ne 即 not equal
    echo "构建失败"; exit 1
fi
```

更常见的是**直接用命令当条件**(省去 `$?`):

```bash
# && : 前一条成功才执行后一条   || : 前一条失败才执行后一条
west flash -d build && echo "烧录成功" || echo "烧录失败"

# if 直接接命令
if ping -c 1 192.168.1.149 > /dev/null; then
    echo "设备在线"
fi
```

文件/目录测试(最常用):

```bash
[ -f file ]   # 是普通文件      [ -d dir ]    # 是目录
[ -e path ]   # 存在(不区分类型) [ ! -e path ] # 不存在
[ -z "$VAR" ] # 变量为空        [ -n "$VAR" ] # 变量非空
```

> **新手第一大坑:中括号两侧必须有空格!** `if [ -f x ]` 写成 `if [-f x]` 直接报错。

### 3.4 循环

```bash
# for:遍历列表/文件
for repo in zephyr hal/espressif esp32s2_net; do
    echo "==== $repo ===="
    git -C "/home/mojo/zephyrproject/$repo" status --short
done

# while read:逐行处理文件
while IFS= read -r line; do
    echo "行内容: $line"
done < serial.log

# while + sleep:轮询等待(设备/文件出现)
while [ ! -e /dev/ttyUSB1 ]; do
    echo "等待串口设备..."; sleep 1
done
echo "设备已连接"
```

### 3.5 函数

```bash
log() { echo "[$(date '+%H:%M:%S')] $*"; }   # 定义(函数参数同样用 $1/$2/$*)

log "开始构建"
log "构建完成,耗时 ${SECONDS}s"              # $SECONDS 是 bash 内置计时器
```

### 3.6 重定向与管道

| 写法 | 含义 |
| --- | --- |
| `cmd > file` | stdout 覆盖写入文件 |
| `cmd >> file` | stdout 追加到文件 |
| `cmd 2>&1` | stderr 合并进 stdout(一起重定向) |
| `cmd 2>/dev/null` | 丢弃 stderr |
| `cmd1 \| cmd2` | 管道:cmd1 输出给 cmd2 当输入 |

开发中最实用的组合——**边看边存日志**:

```bash
west build -b esp32s2_net . 2>&1 | tee build.log
# tee:同时输出到屏幕和文件;构建失败照样留下日志
```

### 3.7 安全开关(推荐每个脚本都加)

```bash
set -euo pipefail
```

| 开关 | 作用 |
| --- | --- |
| `-e` | 任一命令失败(非 0)立即退出 |
| `-u` | 使用未定义变量立即报错(防拼写错误) |
| `-o pipefail` | 管道中任一命令失败就视为失败 |
| `-x` | 打印每条执行的命令(调试用,临时加) |

> 两个常见坑:
> 1. `set -e` 下,`if 命令`、`cmd && cmd` 这类"预期可能失败"的用法**不受影响**,放心用;
> 2. `grep` 没匹配到会返回 1,在 `set -e` 下会中断脚本。不想中断就写成 `grep ... || true`。

## 4. 四个拿来就用的模板

### 模板 1:构建 → 烧录 → 抓日志 一条龙

```bash
#!/bin/bash
set -euo pipefail

BOARD="${1:-esp32s2_net}"
APP=/home/mojo/Projects/zephyr/esp32s2_net
BUILD=$APP/build

cd /home/mojo/zephyrproject
west build -b "$BOARD" "$APP" -d "$BUILD" 2>&1 | tee build.log
west flash -d "$BUILD"

# 复位设备(DTR 拉低 150ms 再拉高)后抓 60 秒日志
python3 - <<'PYEOF'
import serial, time, sys
ser = serial.Serial('/dev/ttyUSB1', 115200, timeout=1)
ser.setDTR(False); time.sleep(0.15); ser.setDTR(True)
end = time.time() + 60
while time.time() < end:
    d = ser.read(1024)
    if d:
        sys.stdout.buffer.write(d); sys.stdout.buffer.flush()
ser.close()
PYEOF
```

> `<<'PYEOF'` 是 **heredoc**:把下面到 `PYEOF` 为止的内容喂给 `python3 -`。
> 带单引号 `'PYEOF'` 表示**不展开**其中的 `$` 变量;不加引号 `<<PYEOF` 则会把 `$VAR` 替换成 shell 变量值(模板 4 有例子)。

### 模板 2:批量检查多个仓库状态

```bash
#!/bin/bash
set -u

REPOS=(
    /home/mojo/zephyrproject/zephyr
    /home/mojo/zephyrproject/modules/hal/espressif
    /home/mojo/Projects/zephyr/esp32s2_net
)

for repo in "${REPOS[@]}"; do
    echo "==== $repo ===="
    git -C "$repo" status --short
    git -C "$repo" log --oneline -1
done
```

> 要点:`git -C <目录>` 免 cd;数组 `${REPOS[@]}` 逐个遍历。以后"批量 pull""批量打 tag"改成一行 git 命令即可。

### 模板 3:日志分析(抓错误、统计、看崩溃现场)

```bash
#!/bin/bash
set -u
LOG="${1:-serial.log}"

# 1) 过滤所有错误行
grep -E "<err>|panic|assert|FATAL" "$LOG" > errors.txt || true

# 2) 统计数量(命令替换 + wc)
echo "错误条数: $(wc -l < errors.txt)"

# 3) 看每条崩溃的上下文(-B 前 5 行 -A 后 15 行)
grep -B5 -A15 "FATAL" "$LOG" | head -80 || true
```

> `wc -l < file` 只输出行数;`grep -B/-A` 是查日志上下文的常用姿势。

### 模板 4:串口自动化(发命令、等回复)

```bash
#!/bin/bash
CMD="${1:-kernel heap}"      # 默认在设备 shell 里执行 kernel heap

python3 - <<PYEOF
import serial, time, sys
ser = serial.Serial('/dev/ttyUSB1', 115200, timeout=1)
ser.setDTR(False); time.sleep(0.15); ser.setDTR(True)

end = time.time() + 20
buf = b''
while time.time() < end:                 # 等 shell 提示符出现
    buf += ser.read(1024)
    if b'uart:~\$' in buf:
        break

ser.write(("$CMD" + "\\n").encode())      # 发命令
time.sleep(0.5)
while time.time() < end:                 # 抓回复
    d = ser.read(1024)
    if d:
        sys.stdout.buffer.write(d); sys.stdout.buffer.flush()
ser.close()
PYEOF
```

> 注意这里 heredoc **没有**单引号,所以 `"$CMD"` 会被 bash 替换成你传入的参数——单引号/无引号 heredoc 的区别就在这。`\\n` 里两个反斜杠是为了让 python 收到 `\n` 而不是换行符。

## 5. 调试与排错

**排查顺序(从快到慢):**

1. `bash -n script.sh` — 语法检查;
2. `bash -x script.sh` — 看每条命令的实际执行结果,90% 的问题在这步解决;
3. 在关键位置 `echo "DEBUG: 变量=$VAR"` 打印中间值;
4. 装 [shellcheck](https://www.shellcheck.net/) 做静态检查(网页版贴进去就能用):
   ```bash
   shellcheck script.sh
   ```

**新手常见坑:**

| 症状 | 原因 | 解法 |
| --- | --- | --- |
| `command not found: script.sh` | 没加 `./` 前缀,或没进目录 | `./script.sh` 或把目录加进 PATH |
| `bad interpreter: /bin/bash^M` | 文件在 Windows 上编辑过,换行是 CRLF | `sed -i 's/\r$//' script.sh` |
| `[ : unexpected operator` | 中括号两侧没空格 | `[ -f x ]` 注意空格 |
| 变量值是"两段" | 没加双引号被拆词 | 一律 `"$VAR"` |
| 脚本里 `cd` 后文件找不到了 | 相对路径依赖当前目录 | 用绝对路径,或开头 `cd "$(dirname "$0")"` |

## 6. 进阶方向

1. **不用建文件的小封装**:单行命令写进 `~/.bashrc` 的 `alias`(别名)或函数,例如:
   ```bash
   alias wb='cd /home/mojo/zephyrproject && west build -b esp32s2_net /home/mojo/Projects/zephyr/esp32s2_net -d /home/mojo/Projects/zephyr/esp32s2_net/build'
   ```
2. **定时任务**:`crontab -e` 加一条 `0 9 * * 1-5 /path/to/check.sh`(工作日 9 点自动跑);
3. **更复杂的自动化**:脚本逻辑超过一两百行、需要跨机器/流程编排时,换 Python 写(本工程的串口脚本就是 bash 调 python heredoc 的混合模式);
4. **让 AI 当助手**:把你手敲过的命令贴给 Claude 说"封装成带参数和错误处理的脚本",再自己审查一遍——生成快,但语法细节(空格、引号)要自己心里有数,本文就是为此准备的。

## 7. 本工程现成的脚本范例

- `patches/apply-hal-patch.sh` — 完整范例:参数默认值、`if/elif/else` 三态判断、`git -C`、`2>/dev/null`、退出码。看懂了它,本文的第 3 节就学完了。
- README「构建与烧录」里的串口复位/抓日志 python 片段 — 模板 1、4 的出处。

## 8. 参考资源

- [BashGuide](https://mywiki.wooledge.org/BashGuide) — 公认最靠谱的 bash 入门
- [shellcheck.net](https://www.shellcheck.net/) — 粘贴即查
- 《Linux 命令行与 Shell 脚本编程大全》 — 系统学习用书
- `man bash` / `man test` — 随时查语法细节
