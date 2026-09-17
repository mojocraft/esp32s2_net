/*
 * MQTT 客户端: 每 100ms 向 broker 发布一次屏幕中间的百分比值。
 * 参考 Zephyr 官方示例 samples/net/mqtt_publisher。
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <string.h>
#include <stdio.h>

#include "mqtt.h"

LOG_MODULE_REGISTER(mqtt_client, LOG_LEVEL_INF);

/* ==== 配置 ==== */
/* broker = 测试电脑(运行 mosquitto 的主机)的 IP。
 * 注意: 电脑 IP 是 DHCP 动态分配的, 变了要改这里 */
#define MQTT_BROKER_ADDR	"192.168.1.77"
#define MQTT_BROKER_PORT	1883
#define MQTT_CLIENT_ID		"esp32s2_kaluga"
#define MQTT_TOPIC		"esp32s2/percent"
#define MQTT_PUBLISH_PERIOD_MS	100
#define MQTT_RECONNECT_DELAY_MS	2000

/* ==== 缓冲区 ==== */
static uint8_t rx_buffer[128];
static uint8_t tx_buffer[128];

/* MQTT client context */
static struct mqtt_client client_ctx;

/* Broker 地址 */
static struct sockaddr_storage broker;

/* 主循环更新的百分比值 */
static volatile int8_t percent_value;

/* 连接状态(由事件回调维护) */
static volatile bool mqtt_connected;

/* ==== 线程 ==== */
K_THREAD_STACK_DEFINE(mqtt_stack, 1536);
static struct k_thread mqtt_thread_data;

/* ==== 事件回调 ==== */
static void mqtt_evt_handler(struct mqtt_client *client,
			     const struct mqtt_evt *evt)
{
	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result == 0) {
			LOG_INF("MQTT connected to broker");
			mqtt_connected = true;
		} else {
			LOG_ERR("MQTT connection rejected: %d", evt->result);
			mqtt_connected = false;
		}
		break;

	case MQTT_EVT_DISCONNECT:
		LOG_INF("MQTT disconnected: %d", evt->result);
		mqtt_connected = false;
		break;

	/* PUBACK/PUBREC/PUBREL/PUBCOMP 只在 QoS>0 时需要处理,
	 * 本项目用 QoS 0, 忽略即可 */
	default:
		break;
	}
}

/* ==== 初始化 ==== */
static void init_context(void)
{
	mqtt_client_init(&client_ctx);

	client_ctx.broker = &broker;
	client_ctx.evt_cb = mqtt_evt_handler;
	client_ctx.client_id.utf8 = (uint8_t *)MQTT_CLIENT_ID;
	client_ctx.client_id.size = sizeof(MQTT_CLIENT_ID) - 1;
	client_ctx.password = NULL;
	client_ctx.user_name = NULL;
	client_ctx.protocol_version = MQTT_VERSION_3_1_1;
	client_ctx.transport.type = MQTT_TRANSPORT_NON_SECURE;

	client_ctx.rx_buf = rx_buffer;
	client_ctx.rx_buf_size = sizeof(rx_buffer);
	client_ctx.tx_buf = tx_buffer;
	client_ctx.tx_buf_size = sizeof(tx_buffer);
}

/* 解析 broker 地址(IP + 端口) */
static int broker_init(void)
{
	struct zsock_addrinfo *addr;
	struct zsock_addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	char port_str[8];

	snprintf(port_str, sizeof(port_str), "%d", MQTT_BROKER_PORT);

	int rc = zsock_getaddrinfo(MQTT_BROKER_ADDR, port_str, &hints, &addr);
	if (rc != 0) {
		LOG_ERR("Broker address resolution failed: %d", rc);
		return rc;
	}

	memcpy(&broker, addr->ai_addr, addr->ai_addrlen);
	zsock_freeaddrinfo(addr);
	return 0;
}

/* ==== 线程主体 ==== */
static void mqtt_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	init_context();

	while (1) {
		/* WiFi 没起来/broker 不可达时循环重试 */
		if (broker_init() != 0) {
			k_msleep(MQTT_RECONNECT_DELAY_MS);
			continue;
		}

		int rc = mqtt_connect(&client_ctx);
		if (rc != 0) {
			LOG_WRN("MQTT connect failed %d, retry in %d s",
				rc, MQTT_RECONNECT_DELAY_MS / 1000);
			/* 注意: Zephyr 3.7 的 mqtt_connect 出错时只重置状态、
			 * 不关闭 socket(fd 泄漏), 必须手动 disconnect 清理 */
			mqtt_disconnect(&client_ctx);
			k_msleep(MQTT_RECONNECT_DELAY_MS);
			continue;
		}

		/* mqtt_connect 非阻塞: CONNECT 包已发出, CONNACK 稍后由
		 * mqtt_input() 收到并触发 MQTT_EVT_CONNACK 事件 */
		LOG_INF("MQTT CONNECT sent, waiting for CONNACK...");
		int connack_wait = 50;	/* 50 x 100ms = 5 秒超时 */
		while (!mqtt_connected && connack_wait-- > 0) {
			rc = mqtt_input(&client_ctx);
			if (rc != 0 && rc != -EAGAIN) {
				break;
			}
			k_msleep(100);
		}
		if (!mqtt_connected) {
			LOG_WRN("CONNACK timeout, reconnecting");
			mqtt_disconnect(&client_ctx);
			k_msleep(MQTT_RECONNECT_DELAY_MS);
			continue;
		}

		/* 连接成功, 进入发布循环 */
		LOG_INF("MQTT publish loop started: topic=%s, period=%d ms",
			MQTT_TOPIC, MQTT_PUBLISH_PERIOD_MS);

		while (mqtt_connected) {
			char payload[8];
			int len = snprintf(payload, sizeof(payload),
					   "%d", percent_value);

			struct mqtt_publish_param param = {
				.message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE,
				.message.topic.topic.utf8 = (uint8_t *)MQTT_TOPIC,
				.message.topic.topic.size = sizeof(MQTT_TOPIC) - 1,
				.message.payload.data = payload,
				.message.payload.len = len,
			};

			rc = mqtt_publish(&client_ctx, &param);
			if (rc != 0) {
				LOG_ERR("MQTT publish failed %d, reconnecting", rc);
				break;
			}

			/* 处理 broker 下行数据(心跳响应等) */
			rc = mqtt_input(&client_ctx);
			if (rc != 0 && rc != -EAGAIN) {
				LOG_ERR("mqtt_input failed %d, reconnecting", rc);
				break;
			}

			/* 保活(必要时发 ping) */
			rc = mqtt_live(&client_ctx);
			if (rc != 0 && rc != -EAGAIN) {
				LOG_ERR("mqtt_live failed %d, reconnecting", rc);
				break;
			}

			k_msleep(MQTT_PUBLISH_PERIOD_MS);
		}

		mqtt_disconnect(&client_ctx);
	}
}

/* ==== 公开接口 ==== */
void mqtt_set_percent(int8_t percent)
{
	percent_value = percent;
}

void mqtt_client_start(void)
{
	k_thread_create(&mqtt_thread_data, mqtt_stack,
			K_THREAD_STACK_SIZEOF(mqtt_stack),
			mqtt_thread_fn, NULL, NULL, NULL,
			K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
	k_thread_name_set(&mqtt_thread_data, "mqtt_pub");
}
