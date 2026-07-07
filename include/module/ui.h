#ifndef __MODULE_UI_H
#define __MODULE_UI_H

#include <stdint.h>
#include <stdbool.h>

/*
 * UI 模板：只画一个居中的框架欢迎屏 + 运行时长。
 * 真正的项目页面（首页/频谱页/参数页...）请复制这个文件到你的
 * module/xxx_ui.c 里改，或者直接扩展 ui_pages[]。
 *
 * 用户扩展方式：
 *   1) 定义 ui_page_t 里的绘制函数（初次绘制 + 周期刷新）
 *   2) ui_register_page(&my_page)
 *   3) ui_switch_to("my_page") 切换
 */

typedef struct {
    const char *name;
    void (*enter)(void);   /* 进入本页时调用一次：清屏 + 画静态 */
    void (*tick )(void);   /* 每个 ui_task 周期调用：局部刷新 */
} ui_page_t;

void ui_init(void);
void ui_task(void);                       /* 注册到调度器（100ms） */
void ui_register_page(const ui_page_t *p);
bool ui_switch_to(const char *name);

#endif /* __MODULE_UI_H */
