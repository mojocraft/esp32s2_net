#ifndef __MQTT_H__
#define __MQTT_H__

#ifdef __cplusplus
extern "C" {
#endif

/** 启动 MQTT 客户端线程(自动重连,WiFi 就绪前会一直重试) */
void mqtt_client_start(void);

/** 更新要发布的百分比值(由主循环周期调用) */
void mqtt_set_percent(int8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* __MQTT_H__ */
