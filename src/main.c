#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <zephyr/net/ethernet.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static const struct gpio_dt_spec bl = GPIO_DT_SPEC_GET(DT_NODELABEL(bl),gpios);
static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static lv_style_t style_title;

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
	LOG_INF("display blanking 0ff");
	/* 面板 DISP ON 后需要一段稳定时间才能可靠接收第一批像素数据，
	 * 否则首帧的前几个刷新块会被丢弃（表现为屏幕顶部内容缺失） */
	k_sleep(K_MSEC(100));

	LOG_INF("Hello Zephyr.");

	/* LVGL */
	lv_style_init(&style_title);
	lv_style_set_text_color(&style_title, lv_color_hex(0x00FFFF));
	lv_style_set_text_font(&style_title, &lv_font_montserrat_14);

	lv_obj_t *scr = lv_scr_act();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

	lv_obj_t *t = lv_label_create(scr);
	lv_label_set_text(t, "Hello Zephyr V3.7.2 LTS");
	lv_obj_add_style(t, &style_title, 0);
	lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

	/* test lvgl */
	lv_obj_t *label = lv_label_create(scr);
	lv_label_set_text(label, "Label test");
	lv_obj_center(label);

	while (1) {
		uint32_t ms = lv_timer_handler();
		k_msleep(ms > 1000 ? 1000 : ms);
	}
	return 0;
}
