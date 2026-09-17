/*
 * 屏幕日志面板: 注册一个自定义 Zephyr 日志后端, 把日志实时显示到屏幕上。
 *
 * 工作原理:
 *  - LOG_MODE_IMMEDIATE 下, 后端 process() 在日志产生者上下文(任意线程)
 *    被同步调用, 用 log_output 格式化后经 char_out 进入行环形缓冲;
 *  - 行环/行缓冲放在 PSRAM(.ext_ram.bss), 纯 CPU 访问, 不占内部 DRAM;
 *  - LVGL 定时器每 300ms 在 LVGL 上下文把新行更新到底部半透明面板。
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_backend_std.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log_core.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(lv_log, LOG_LEVEL_INF);

/* 行环: 8 行 x 112 字符, 放 PSRAM */
#define LOG_LINE_MAX   112
#define LOG_RING_LINES 8
#define DISP_LINES     4	/* 屏幕上显示的最近行数 */
#define DISP_CHARS     44	/* 每显示行最大字符数(320 宽 12px 字体的上限) */

static char ring[LOG_RING_LINES][LOG_LINE_MAX + 1]
	__attribute__((section(".ext_ram.bss")));
static char cur_line[LOG_LINE_MAX + 1]
	__attribute__((section(".ext_ram.bss")));
static uint32_t cur_len;
static uint32_t ring_wr;	/* 已写入行数(单调递增) */
static uint32_t ring_rd;	/* 已被屏幕消费的行数 */
static struct k_spinlock ring_lock;

static lv_obj_t *log_label;

/* ==== log_output 的字符回调(日志产生者上下文, 可能来自 ISR) ====
 * 注意: 必须返回"已消费的字节数"——log_output 的 buffer_write() 依赖
 * 返回值推进 buf/len, 返回 void 会导致用垃圾值推进指针而崩溃 */
static int char_out(uint8_t *data, size_t length, void *ctx)
{
	ARG_UNUSED(ctx);

	k_spinlock_key_t key = k_spin_lock(&ring_lock);

	for (size_t i = 0; i < length; i++) {
		char c = (char)data[i];

		if (c == '\r') {
			continue;
		}

		if (c == '\n' || cur_len >= LOG_LINE_MAX) {
			/* 提交当前行 */
			cur_line[cur_len] = '\0';
			strncpy(ring[ring_wr % LOG_RING_LINES], cur_line,
				LOG_LINE_MAX);
			ring[ring_wr % LOG_RING_LINES][LOG_LINE_MAX] = '\0';
			ring_wr++;
			cur_len = 0;

			if (c == '\n') {
				continue;
			}
		}

		cur_line[cur_len++] = c;
	}

	k_spin_unlock(&ring_lock, key);

	return length;
}

/* log_output 内部缓冲 */
static uint8_t log_buf[128];

LOG_OUTPUT_DEFINE(lv_log_output, char_out, log_buf, sizeof(log_buf));

/* ==== 日志后端 API ==== */
static uint32_t log_format_current;	/* 0 = LOG_OUTPUT_TEXT 默认格式 */

static void process(const struct log_backend *const backend,
		    union log_msg_generic *msg)
{
	ARG_UNUSED(backend);

	uint32_t flags = log_backend_std_get_flags();
	log_format_func_t log_output_func =
		log_format_func_t_get(log_format_current);

	log_output_func(&lv_log_output, &msg->log, flags);
}

static void dropped(const struct log_backend *const backend, uint32_t cnt)
{
	ARG_UNUSED(backend);
	ARG_UNUSED(cnt);
}

static void panic(struct log_backend const *const backend)
{
	ARG_UNUSED(backend);
}

static void lv_log_backend_init(const struct log_backend *const backend)
{
	ARG_UNUSED(backend);
}

static int is_ready(const struct log_backend *const backend)
{
	ARG_UNUSED(backend);
	return 1;
}

static int format_set(const struct log_backend *const backend,
		      uint32_t log_type)
{
	ARG_UNUSED(backend);
	log_format_current = log_type;
	return 0;
}

static const struct log_backend_api log_backend_lvlog_api = {
	.process = process,
	.dropped = dropped,
	.panic = panic,
	.init = lv_log_backend_init,
	.is_ready = is_ready,
	.format_set = format_set,
};

LOG_BACKEND_DEFINE(log_backend_lvlog, log_backend_lvlog_api, false, NULL);

/* ==== LVGL 侧: 定时器把新日志更新到面板 ==== */
static char disp_buf[DISP_LINES * (DISP_CHARS + 3) + 1]
	__attribute__((section(".ext_ram.bss")));

static void log_panel_timer_cb(lv_timer_t *timer)
{
	ARG_UNUSED(timer);

	/* 快照消费计数 */
	k_spinlock_key_t key = k_spin_lock(&ring_lock);
	uint32_t wr = ring_wr;
	uint32_t rd = ring_rd;
	k_spin_unlock(&ring_lock, key);

	uint32_t pending = wr - rd;
	if (pending == 0) {
		return;
	}

	/* 取最近 DISP_LINES 行(旧→新)拼成文本 */
	if (pending > DISP_LINES) {
		rd = wr - DISP_LINES;
	}

	uint32_t pos = 0;
	for (uint32_t i = 0; i < DISP_LINES && rd + i < wr; i++) {
		const char *line = ring[(rd + i) % LOG_RING_LINES];
		uint32_t n = 0;

		while (line[n] && n < DISP_CHARS && pos < sizeof(disp_buf) - 2) {
			disp_buf[pos++] = line[n++];
		}
		if (line[n]) {
			/* 截断标记 */
			if (pos < sizeof(disp_buf) - 3) {
				disp_buf[pos++] = '.';
				disp_buf[pos++] = '.';
			}
		}
		if (pos < sizeof(disp_buf) - 2) {
			disp_buf[pos++] = '\n';
		}
	}
	disp_buf[pos] = '\0';

	lv_label_set_text(log_label, disp_buf);

	/* 标记已消费 */
	key = k_spin_lock(&ring_lock);
	ring_rd = wr;
	k_spin_unlock(&ring_lock, key);
}

/* ==== 公开接口 ==== */
void lv_log_init(void)
{
	/* 底部半透明面板 */
	lv_obj_t *panel = lv_obj_create(lv_scr_act());
	lv_obj_set_size(panel, LV_HOR_RES, 70);
	lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_obj_set_style_bg_color(panel, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(panel, LV_OPA_50, 0);
	lv_obj_set_style_border_width(panel, 0, 0);
	lv_obj_set_style_pad_all(panel, 3, 0);

	log_label = lv_label_create(panel);
	lv_label_set_text(log_label, "");
	lv_obj_set_style_text_font(log_label, &lv_font_montserrat_12, 0);
	lv_obj_set_style_text_color(log_label, lv_color_white(), 0);
	lv_obj_set_width(log_label, LV_HOR_RES - 6);
	lv_label_set_long_mode(log_label, LV_LABEL_LONG_DOT);

	/* 每 300ms 拉取新日志 */
	lv_timer_create(log_panel_timer_cb, 300, NULL);

	/* 注册显示日志后端 */
	log_backend_activate(&log_backend_lvlog, NULL);
}
