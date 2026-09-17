# MQTT 链路调试记录(ESP32-S2 → 电脑 mosquitto broker)

本文记录"屏幕百分比值以 100ms 周期经 MQTT 发布到电脑"这条链路的搭建与排错全过程,含每个坑的**原因分析**。

## 链路拓扑

```
[ESP32-S2 板子] ──WiFi──> [路由器] ──局域网──> [电脑 enp5s0]
 192.168.1.149                               192.168.1.77:1883
                                              (mosquitto broker)
```

- 板子发布主题:`esp32s2/percent`,QoS 0,周期 100ms;
- broker 地址硬编码在 `src/mqtt.c` 的 `MQTT_BROKER_ADDR`(电脑 IP 是 DHCP 分配,变了要改);
- 固件侧结构:独立线程 `mqtt_pub`(栈 1536B),WiFi 未就绪/断线时每 2 秒重试,主循环通过 `mqtt_set_percent()` 更新值。

## 电脑侧配置

```bash
sudo apt install -y mosquitto mosquitto-clients

# mosquitto 2.x 必须显式开放局域网监听
sudo tee /etc/mosquitto/conf.d/lan.conf <<'EOF'
listener 1883 0.0.0.0
allow_anonymous true
EOF
sudo systemctl restart mosquitto
ss -tlnp | grep 1883   # 应出现 0.0.0.0:1883
```

订阅验证:`mosquitto_sub -h 127.0.0.1 -t esp32s2/percent -v`

## 四个坑(坑 / 现象 / 原因分析 / 修复)

### 坑 1:mosquitto 2.x 默认只监听 127.0.0.1

| | |
| --- | --- |
| 现象 | 板子 TCP 连接一直超时,`MQTT 连接失败 -116`;而电脑本机 `mosquitto_sub -h 127.0.0.1` 却正常 |
| **原因分析** | 服务器"绑定地址"决定它接受**哪些目的地址**的入站包:绑定 `127.0.0.1` 时,只有目的地址是 127.0.0.1 的包(本机回环)匹配监听 socket。板子发往 `192.168.1.77:1883` 的包从 `enp5s0` 网口进来,目的地址对不上 → 内核直接丢弃 → 远端连 RST 都收不到,表现为超时而非"拒绝连接"。mosquitto 2.x 的安全哲学是"开箱即封闭":不写配置默认只开本机回环,避免用户无意间把 broker 暴露到网络 |
| 修复 | `listener 1883 0.0.0.0`(绑定所有接口)+ `allow_anonymous true`(2.x 同时要求显式允许匿名) |

### 坑 2:`CONFIG_NET_MAX_CONN` 默认 4 不够

| | |
| --- | --- |
| 现象 | `net_conn: Not enough connection contexts. Consider increasing CONFIG_NET_MAX_CONN.`,MQTT 连接失败 -2 |
| **原因分析** | Zephyr 3.7 引入独立的"连接池"(`NET_MAX_CONN`,默认 4):每个 TCP 连接占一个槽。**WiFi 栈自身在连接 MQTT 之前就已经占用了槽位**(DHCP 的 socket、net_mgmt 通道、wifi 驱动内部连接等),剩给应用层的槽只有 1~2 个;MQTT 打开 TCP 连接时池子已空 → 分配失败。此前没有 TCP 应用,默认 4 碰巧够用;引入 MQTT 长连接后暴露 |
| 修复 | `CONFIG_NET_MAX_CONN=8` |

### 坑 3:`CONFIG_POSIX_MAX_FDS` 默认 4 只剩 1 个可用槽

| | |
| --- | --- |
| 现象 | `MQTT 连接失败 -23`(ENFILE,"Too many open files") |
| **原因分析** | POSIX 文件描述符表(`POSIX_MAX_FDS`)默认 4 个槽:fd 0/1/2 固定给 stdin/stdout/stderr,实际只剩 **1 个**给 socket。每个 `socket()` 调用占用一个 fd;若连接失败路径上 socket 没有被关闭(fd 泄漏,见坑 4),几次重试就把唯一的槽耗尽 → 之后所有 socket() 都返回 ENFILE,且**不会自动恢复** |
| 修复 | `CONFIG_POSIX_MAX_FDS=16` |

### 坑 4:`mqtt_connect()` 非阻塞 + 出错不关 socket

| | |
| --- | --- |
| 现象 | 日志出现 4 次 `Connect completed` + `发布线程启动`,随后永久 -23;事件回调里"MQTT 已连接 broker"从未出现 |
| **原因分析** | 两个问题叠加:① Zephyr 的 `mqtt_connect()` 是**非阻塞**的 —— 它只负责建立 TCP 并把 CONNECT 包发出去就返回 0;**CONNACK 由 broker 异步回送,必须由应用持续调用 `mqtt_input()` 接收**,收到后才会触发 `MQTT_EVT_CONNACK` 事件回调。直接检查"已连接"标志(事件还没到,标志还是 false)就会误判未连接 → 立刻断开重连,形成 0.3 秒一次的快速循环;② Zephyr 3.7 的 `mqtt_connect` 出错路径 `client_reset()` **只重置内部状态,不关闭 socket** → 每次失败的尝试泄漏一个 fd,重试一次漏一个,直到 fd 表耗尽 |
| 修复 | ① `mqtt_connect` 成功后,先循环 `mqtt_input()` 等待 CONNACK(100ms 间隔,5 秒超时),再进入发布循环;② 所有连接失败路径手动调用 `mqtt_disconnect(&client_ctx)` 关闭 socket,防止 fd 泄漏 |

## 排错方法回顾(可复用)

1. **看板子日志定位错误码**:`-116` 超时(网络层不通)→ `-2`(连接池耗尽)→ `-23`(fd 表耗尽),错误码一路变化说明链路在逐层推进;
2. **电脑侧独立验证**:`ping 192.168.1.149` 确认设备在线;`ss -tlnp | grep 1883` 确认 broker 监听范围;`mosquitto_sub -C 8` 定量验证发布流;
3. **minicom 与脚本抓取互斥**:两个进程同时读串口会抢数据(见 `docs/shell-scripting-guide.md` 模板 4 的说明),诊断时只能留一个。

## 验证结果(2026-09-17)

```text
$ mosquitto_sub -h 127.0.0.1 -t esp32s2/percent -v -C 8
esp32s2/percent 77
esp32s2/percent 75
esp32s2/percent 73
esp32s2/percent 71
esp32s2/percent 69
esp32s2/percent 67
esp32s2/percent 65
esp32s2/percent 63
```

100ms 一条、数值连续、与屏幕同步。断网/断电后固件自动重连 WiFi 并恢复发布。

## 相关参考

- Zephyr 官方示例:`samples/net/mqtt_publisher`(本工程 `src/mqtt.c` 的骨架来源)
- `zephyr/subsys/net/lib/mqtt/mqtt.c`(`mqtt_connect` / `client_reset` 的实现,fd 泄漏现场)
- mosquitto 2.x 迁移说明:https://mosquitto.org/documentation/migrating-to-2-0/
