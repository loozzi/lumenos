/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：四个产品主页面共用的底部导航；阅读页自行隐藏。
 * English: Shared four-tab product navigation; the reader hides it.
 */
#pragma once

#include "app.h"
#include "ui_nav_layout.h"

#define UI_NAV_BACK_BORDER_GRAY 0x58

void ui_nav_draw(uint8_t *fb, int active);
void ui_nav_status(uint8_t *fb);
/// 保留位置与隐藏状态重绘。/ Redraw retaining position and visibility.
void ui_nav_redraw_status(uint8_t *fb);
EpdRect ui_nav_status_area(void);
#define UI_NAV_READER_STATUS_HEIGHT 54
void ui_nav_reader_status(uint8_t *fb);
/// 电量/充电变化且状态栏可见时重画；调用方只差分推状态栏区域。
/// Paint changed power state only when visible; callers present only the status-bar diff.
bool ui_nav_refresh_power(uint8_t *fb);
/// 系统字体就绪时缓存状态栏字宽，阅读页只复用，不重载整份系统字体。
/// Cache status metrics when the system face is ready; reading reuses them without reloading that font.
void ui_nav_status_font_context(bool reading, bool face_changed);
/// 状态栏中段在阅读页显示书名而不是用户签名；传空串即清除。
/// While reading, the status bar's middle shows the book title instead of the user signature;
/// pass an empty string to clear it.
void ui_nav_status_set_title(const char *title);
const char *ui_nav_status_title(void);
/// 书名是否顶替签名；关闭时恢复签名。/ Whether the title replaces the signature; off restores it.
bool ui_nav_status_title_on(void);
void ui_nav_status_set_title_on(bool on);
/// 当前 UTC 分钟，用于显示更新判断；不读取卡或请求网络校时。
/// Current UTC minute for display updates, without card IO or network time requests.
int64_t ui_nav_clock_minute(void);
/// 只绘制状态栏时间，供主页面按分钟局部更新。/ Draw only the status clock for local minute updates.
void ui_nav_clock_draw(uint8_t *fb);
/// 状态栏同款时间文本（HH:MM，未校时为--:--）。/ Status clock text (HH:MM, or --:-- before time sync).
void ui_nav_clock_text(char *out, size_t capacity);
/// 电量与充电签名，阅读页脚据此判断是否重画。/ Battery level and charging signature for reader-footer repaint checks.
int ui_nav_battery_signature(void);
/// 阅读页脚右对齐画小电池（无百分比），fb 为空只测量；返回左边界。
/// Right-align the small reader-footer battery (no percentage); NULL fb measures only. Returns the left edge.
int ui_nav_footer_battery_draw(uint8_t *fb, int right, int cy);
/// 绘制设置页与状态栏共用的 WiFi 标志。/ Draw the WiFi mark shared by Settings and the status bar.
void ui_nav_wifi_icon(uint8_t *fb, int cx, int cy, int size, uint8_t gray);
/// 二级页面共用的圆形返回按钮。/ Shared circular back control for secondary pages.
void ui_nav_back(uint8_t *fb, int x, int y);
/// 四栏整列热区向上扩16px，互不重叠；不改变画面和刷新区域。/ Four nonoverlapping full-column hits extend 16px upward, without changing paint or refresh bounds.
int ui_nav_hit(uint16_t x, uint16_t y);
/// 当前布局下某一栏图标的点击反馈框；不存在时返回空矩形。/ Icon feedback box of a tab in the current layout; empty when absent.
EpdRect ui_nav_tab_rect(int tab);
void ui_nav_request(app_ctx_t *ctx, int index);
