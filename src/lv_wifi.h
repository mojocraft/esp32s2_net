#ifndef __LV_WIFI_H__
#define __LV_WIFI_H__

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WiFi 信号条图标(定义在 wifi_icons.c) */
LV_IMG_DECLARE(wifi_4);
LV_IMG_DECLARE(wifi_3);
LV_IMG_DECLARE(wifi_2);
LV_IMG_DECLARE(wifi_1);

void wifi_statusbar_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __LV_WIFI_H__ */
