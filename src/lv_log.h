#ifndef __LV_LOG_H__
#define __LV_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

/** 创建屏幕日志面板 + 注册显示日志后端(在 LVGL 初始化后调用) */
void lv_log_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __LV_LOG_H__ */
