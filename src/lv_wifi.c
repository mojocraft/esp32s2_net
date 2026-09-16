#include <lvgl.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include "lv_wifi.h"

LOG_MODULE_REGISTER(wifi_statusbar, LOG_LEVEL_INF);

/* ==== UI 对象 ==== */
static lv_obj_t *statusbar;
static lv_obj_t *wifi_icon;
static lv_obj_t *rssi_label;
static lv_obj_t *ssid_label;

/* ==== 根据 RSSI 选择 WIFI 图标 ==== */
static const lv_img_dsc_t *get_wifi_icon_img(int8_t rssi)
{
	if (rssi >= -50) return &wifi_4;	/* 信号强:4 格 */
	if (rssi >= -65) return &wifi_3;	/* 信号中:3 格 */
	if (rssi >= -75) return &wifi_2;	/* 信号弱:2 格 */
	return &wifi_1;				/* 信号极弱:1 格 */
}

/* ===== 更新状态栏显示 (经 lv_async_call 在 LVGL 上下文执行) ==== */
static void update_statusbar(void *param)
{
	struct wifi_iface_status *s = (struct wifi_iface_status *)param;

	if (!statusbar) {
		if (s) k_free(s);
		return;
	}

	if (s) {
		/* 有数据, 更新 WiFi 信息 */
		lv_img_set_src(wifi_icon, get_wifi_icon_img(s->rssi));
		lv_obj_clear_flag(wifi_icon, LV_OBJ_FLAG_HIDDEN);

		char rssi_buf[8];
		snprintf(rssi_buf, sizeof(rssi_buf), "%d", s->rssi);
		lv_label_set_text(rssi_label, rssi_buf);

		lv_label_set_text(ssid_label, s->ssid);

		k_free(s);
	} else {
		/* 无数据(断开连接), 显示默认状态 */
		lv_obj_add_flag(wifi_icon, LV_OBJ_FLAG_HIDDEN);
		lv_label_set_text(rssi_label, "--");
		lv_label_set_text(ssid_label, "Disconnected");
	}
}


/* ==== 创建状态栏 UI ==== */
void statusbar_create(void)
{
	/* status bar: 占满顶行, 内容靠右 */
	statusbar = lv_obj_create(lv_scr_act());

	lv_obj_set_size(statusbar, LV_HOR_RES, 22);
	lv_obj_align(statusbar, LV_ALIGN_TOP_RIGHT, 0, 0);
	lv_obj_set_style_bg_opa(statusbar, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(statusbar, 0, 0);
	lv_obj_set_style_pad_all(statusbar, 0, 0);
	lv_obj_set_flex_flow(statusbar, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(statusbar, LV_FLEX_ALIGN_END,
			      LV_FLEX_ALIGN_CENTER,
			      LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_column(statusbar, 4, 0);

	/* wifi 图标(信号条, 用 1bit 位图 + 白色重着色) */
	wifi_icon = lv_img_create(statusbar);
	lv_img_set_src(wifi_icon, &wifi_4);
	lv_obj_set_style_img_recolor(wifi_icon, lv_color_white(), 0);
	lv_obj_set_style_img_recolor_opa(wifi_icon, LV_OPA_COVER, 0);
	lv_obj_add_flag(wifi_icon, LV_OBJ_FLAG_HIDDEN);	/* 连上后才显示 */

	/* RSSI */
	rssi_label = lv_label_create(statusbar);
	lv_label_set_text(rssi_label, "--");
	lv_obj_set_style_text_font(rssi_label, &lv_font_montserrat_12, 0);

	/* SSID name */
	ssid_label = lv_label_create(statusbar);
	lv_label_set_text(ssid_label, "Not Connected");
	lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_12, 0);
	lv_obj_set_width(ssid_label, 90);	/* 限制宽度 */
	lv_label_set_long_mode(ssid_label, LV_LABEL_LONG_DOT);	/* 超长显示 */
}

/* === WIFI 事件回调 (运行在网络线程) === */
static struct net_mgmt_event_callback wifi_cb;

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
			       uint32_t mgmt_event,
			       struct net_if *iface)
{
	struct wifi_iface_status status = {0};

	switch (mgmt_event) {

	case NET_EVENT_WIFI_CONNECT_RESULT:
		LOG_INF("Wi-Fi connected");
		if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface,
			     &status, sizeof(status)) == 0) {
			struct wifi_iface_status *copy = k_malloc(sizeof(*copy));
			if (copy) {
				memcpy(copy, &status, sizeof(*copy));
				lv_async_call(update_statusbar, copy);
			}
		}
		break;

	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		LOG_INF("Wi-Fi disconnected");
		lv_async_call(update_statusbar, NULL);
		break;
	}
}

/* ===== 公开初始化接口 ===== */
void wifi_statusbar_init(void)
{
	statusbar_create();

	net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT |
				     NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);
}
