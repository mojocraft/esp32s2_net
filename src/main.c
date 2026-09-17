#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/ethernet.h>
#include "fonts/yahei_14.h"
#include "lv_wifi.h"
#include "lv_log.h"
#include "mqtt.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static const struct gpio_dt_spec bl = GPIO_DT_SPEC_GET(DT_NODELABEL(bl),gpios);
static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static lv_style_t style_title;
static lv_style_t label_style;
static lv_style_t arc_bg_style;
static lv_style_t indicator_style;
static lv_obj_t *scr; 
static lv_obj_t *t;
static lv_obj_t *arc;
static lv_obj_t *label; 
#define WIFI_SSID "QHDTUC2.4"
#define WIFI_PASSWORD "QHDTUC11305610"
static int connected;
static struct net_mgmt_event_callback wifi_mgmt_cb;

#define ACCENT lv_color_hex(0x6366f1)
#define MASK_WIDTH 250
#define MASK_HEIGHT 25

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;
	
	if (status->status) {
		LOG_ERR("wifi connect failed %d", status->status);
	} else {
		LOG_INF("wifi connected");
		connected = 1;
	}
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		handle_wifi_connect_result(cb);
	}
}

/* 这里是wifi阻塞式连接方式 */
void wifi_connect(void)
{
	struct net_if *iface = net_if_get_default();
	
	static struct wifi_connect_req_params params = {
		.ssid		= WIFI_SSID,
		.ssid_length	= sizeof(WIFI_SSID) -1,
		.psk		= WIFI_PASSWORD,
		.psk_length	= sizeof(WIFI_PASSWORD) -1,
		.channel	= 0,
		.security	= WIFI_SECURITY_TYPE_PSK,
		.band		= WIFI_FREQ_BAND_2_4_GHZ,
		.mfp		= WIFI_MFP_OPTIONAL,
	};
	
	net_mgmt_init_event_callback(&wifi_mgmt_cb,
                                 wifi_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_mgmt_cb);

	connected = 0;

	int nr_tries = 10;
	while (nr_tries-- > 0) {
		int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
				   &params,
				   sizeof(struct wifi_connect_req_params));
	if (ret == 0) {
		break;
	}
		LOG_INF("Waiting for Wi-Fi interface...");
		k_msleep(500);
	}

	/* 等待连接完成 */
	while (connected == 0) {
		k_msleep(100);
	}

}

/* 这里是wifi异步连接方式 */
void wifi_connect_async(void)
{
    struct net_if *iface = net_if_get_default();

    static struct wifi_connect_req_params params = {
        .ssid        = WIFI_SSID,
        .ssid_length = sizeof(WIFI_SSID) - 1,
        .psk         = WIFI_PASSWORD,
        .psk_length  = sizeof(WIFI_PASSWORD) - 1,
        .channel     = 0,
        .security    = WIFI_SECURITY_TYPE_PSK,
	.band	     = WIFI_FREQ_BAND_2_4_GHZ,
	.mfp         = WIFI_MFP_OPTIONAL,
    };

    net_mgmt_init_event_callback(&wifi_mgmt_cb,
                                 wifi_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT |
                                 NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_mgmt_cb);

    connected = 0;

    /* 等待驱动初始化完成再发起连接 */
    k_sleep(K_SECONDS(2));

    int nr_tries = 10;
    while (nr_tries-- > 0) {
        int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
                           &params,
                           sizeof(struct wifi_connect_req_params));
        if (ret == 0) {
            LOG_INF("Wi-Fi connect request sent, waiting for result...");
            return;
        }
        LOG_WRN("Wi-Fi connect request failed (%d), retrying...", ret);
        k_sleep(K_MSEC(500));
    }

    LOG_ERR("All Wi-Fi connect requests failed");
}

void lv_example_label_5(void)
{
	static lv_anim_t animation_template;
	static lv_style_t label_style;

	lv_anim_init(&animation_template);
	lv_anim_set_delay(&animation_template, 1000);
	lv_anim_set_repeat_delay(&animation_template, 3000);
	
	lv_style_init(&label_style);
	lv_style_set_anim(&label_style, &animation_template);
	lv_style_set_text_color(&label_style, lv_color_hex(0x000FFF));
	
	lv_obj_t *label1 = lv_label_create(lv_scr_act());
	lv_label_set_long_mode(label1, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll */
	lv_obj_set_width(label1, 200);
	lv_label_set_text(label1, "Nothing in the world can take the place of persistence.");
	lv_obj_align(label1, LV_ALIGN_CENTER, 0, 100);
	lv_obj_add_style(label1, &label_style, LV_STATE_DEFAULT);
}


static void add_mask_event_cb(lv_event_t * e)
{
	static lv_draw_mask_map_param_t m;
	static int16_t mask_id;

	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * obj = lv_event_get_target(e);
	lv_opa_t * mask_map = lv_event_get_user_data(e);
	if(code == LV_EVENT_COVER_CHECK) {
	lv_event_set_cover_res(e, LV_COVER_RES_MASKED);
	}
	else if(code == LV_EVENT_DRAW_MAIN_BEGIN) {
	lv_draw_mask_map_init(&m, &obj->coords, mask_map);
	mask_id = lv_draw_mask_add(&m, NULL);

	}
	else if(code == LV_EVENT_DRAW_MAIN_END) {
	lv_draw_mask_free_param(&m);
	lv_draw_mask_remove_id(mask_id);
	}
}

/**
 * Draw label with gradient color
 */

 void lv_example_label_4(void)
{
	/* Create the mask of a text by drawing it to a canvas.
	 * 缓冲放 PSRAM(.ext_ram.bss 段, 启动时自动清零):
	 * 仅 CPU 读写、无 DMA, 6.25KB 内部 DRAM 留给网络栈 */
	static lv_opa_t mask_map[MASK_WIDTH * MASK_HEIGHT]
		__attribute__((section(".ext_ram.bss")));

	/*Create a "8 bit alpha" canvas and clear it*/
	lv_obj_t * canvas = lv_canvas_create(lv_scr_act());
	lv_canvas_set_buffer(canvas, mask_map, MASK_WIDTH, MASK_HEIGHT, LV_IMG_CF_ALPHA_8BIT);
	lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

	/*Draw a label to the canvas. The result "image" will be used as mask*/
	lv_draw_label_dsc_t label_dsc;
	lv_draw_label_dsc_init(&label_dsc);
	label_dsc.color = lv_color_white();
	label_dsc.align = LV_TEXT_ALIGN_CENTER;
	label_dsc.font = &yahei_14;
	lv_canvas_draw_text(canvas, 0, 5, MASK_WIDTH, &label_dsc, "欲买桂花同载酒, 终不似, 少年游.");

	/*The mask is reads the canvas is not required anymore*/
	lv_obj_del(canvas);

	/* Create an object from where the text will be masked out.
	* Now it's a rectangle with a gradient but it could be an image too*/
	lv_obj_t * grad = lv_obj_create(lv_scr_act());
	lv_obj_set_size(grad, MASK_WIDTH, MASK_HEIGHT);
	lv_obj_align(grad, LV_ALIGN_TOP_RIGHT, -35, 20);

	/* ===== 新增：去除默认的边框、轮廓和内边距 ===== */
	lv_obj_set_style_border_width(grad, 0, 0);       // 去除边框
	lv_obj_set_style_outline_width(grad, 0, 0);      // 去除轮廓线
	lv_obj_set_style_shadow_width(grad, 0, 0);       // 去除阴影
	lv_obj_set_style_pad_all(grad, 0, 0);            // 去除内边距(防止mask偏移)
	lv_obj_set_style_radius(grad, 0, 0);             // 去除圆角(可选)

	lv_obj_set_style_bg_color(grad, lv_color_hex(0x00ff00), 0);
	lv_obj_set_style_bg_grad_color(grad, lv_color_hex(0xff0000), 0);
	lv_obj_set_style_bg_grad_dir(grad, LV_GRAD_DIR_HOR, 0);
	lv_obj_add_event_cb(grad, add_mask_event_cb, LV_EVENT_ALL, mask_map);
}

void lvgl_ui_test(void)
{
	k_sleep(K_MSEC(100));

	LOG_INF("Hello Zephyr.");

	/* LVGL */
	lv_style_init(&style_title);
	lv_style_set_text_color(&style_title, lv_color_hex(0xFF0000));
	lv_style_set_text_font(&style_title, &lv_font_montserrat_14);
	// lv_style_set_text_font(&style_title, &lv_font_unscii_8);

	/* label style config */
	lv_style_init(&label_style);
	lv_style_set_text_color(&label_style, lv_color_hex(0x00FF00));
	lv_style_set_text_font(&label_style, &lv_font_montserrat_14);

	/* arc back ground style config */
	lv_style_init(&arc_bg_style);
	lv_style_set_arc_color(&arc_bg_style, ACCENT);
	lv_style_set_arc_width(&arc_bg_style, 14);
	lv_style_set_arc_opa(&arc_bg_style, (255 * 20 / 100));
	lv_style_set_arc_rounded(&arc_bg_style, true);

	/* indicator style config */
	lv_style_init(&indicator_style);
	lv_style_set_arc_color(&indicator_style, ACCENT);
	lv_style_set_arc_width(&indicator_style, 14);
	lv_style_set_arc_rounded(&indicator_style, true);

	scr = lv_scr_act();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

	t = lv_label_create(scr);
	lv_label_set_text(t, "MQTT Client Terminal");
	lv_obj_add_style(t, &style_title, 0);
	lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

	/* test arc */
	arc = lv_arc_create(scr);
	lv_obj_set_size(arc, 140, 140);
	lv_arc_set_range(arc, 0, 100);
	lv_arc_set_value(arc, 0);
	lv_obj_add_style(arc, &arc_bg_style, LV_PART_MAIN);
	lv_obj_add_style(arc, &indicator_style, LV_PART_INDICATOR);
	lv_obj_center(arc);
	
	/* arc counter label */
	label = lv_label_create(arc);
	lv_obj_add_style(label, &label_style, 0);
	lv_obj_center(label);

	/* Circular scroll */
	lv_example_label_5();

	/* Rainbow text */
	lv_example_label_4();
}

int main(void)
{
	int ret;

	if (!gpio_is_ready_dt(&bl)) {
		LOG_ERR("device bl get failed!");
	}

	/* 上电即点亮背光 */
	ret = gpio_pin_configure_dt(&bl, GPIO_OUTPUT_INIT_HIGH);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure output on pin %d", ret, bl.pin);
	}

	/* 打开面板显示（DISP ON），驱动初始化后默认是关闭状态 */
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display device not ready");
		return 0;
	}
	display_blanking_off(display_dev);
	LOG_INF("display blanking off");
	/* 面板 DISP ON 后需要一段稳定时间才能可靠接收第一批像素数据，
	 * 否则首帧的前几个刷新块会被丢弃（表现为屏幕顶部内容缺失） */

	lvgl_ui_test();
	wifi_statusbar_init();
	lv_log_init();
	mqtt_client_start();

	static int8_t counter = 0;
	static bool direction = 0;

	wifi_connect_async();

	while (1) {
		uint32_t ms = lv_timer_handler();
		lv_arc_set_value(arc, counter);
		lv_label_set_text_fmt(label, "%d%%", counter);
		if (!direction) counter++;
		else counter--;
		if (counter == 100 || counter == 0) {
			 direction = !direction;
		}
		mqtt_set_percent(counter);
		k_msleep(30);
	}
	return 0;
}
