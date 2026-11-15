/* SPDX-License-Identifier: Apache-2.0
 * 中文：顶部快捷菜单的独立手势和纯绘图；动作由主循环执行。
 * English: Independent gesture recognition and pure drawing for the top quick layer; the loop owns actions.
 * 用户修订：展开复用现有底页；只保留顶部4bpp底图，收起还原，点按局部反馈。S31于2026-11-15改为白色面板八方键与横向滑杆。
 * User revision: open over the existing page; retain only a packed top underlay, restore on close, and acknowledge presses locally.
 * S31 switched to a white panel with eight tiles and horizontal sliders on 2026-11-15.
 * 冻结：仅边缘起手；不做连续动画或整帧缓存；关闭、切页、睡眠和错误时释放控件缓存。
 * Frozen: edge-origin capture only, no continuous animation or full-frame cache; release key snapshots on close, navigation, sleep and errors.
 */
#pragma once
#include "app.h"

#define UI_QUICK_EDGE_PX 32
#define UI_QUICK_SWIPE_PX 60
#if DPREADER_BOARD_METALIO47
// 用户2026-11-15修订：白色面板八个带文字圆角方键与两条横向滑杆，面板只包住控件。
// 竖屏两行四列、滑杆上下排列；横屏一行八列、两条滑杆并排，少遮正文。
// 偶发错误提示以胶囊浮在面板下方，平时不占空间。
// User revision 2026-11-15: a white panel that only wraps eight labelled tiles and two horizontal
// sliders. Portrait uses two rows of four with stacked sliders; landscape uses one row of eight with
// side-by-side sliders to cover less text. Rare error feedback floats as a pill below the panel.
#define UI_QUICK_BUTTON_COUNT 8
#define UI_QUICK_TILE 88
#define UI_QUICK_TILE_RADIUS 24
#define UI_QUICK_RADIUS (UI_QUICK_TILE/2)
#define UI_QUICK_LABEL_GAP 22
#define UI_QUICK_FILL_DARK 0x30
#define UI_QUICK_SLIDER_RANGE 240
#define UI_QUICK_SLIDER_LENGTH (2*UI_QUICK_SLIDER_RANGE)
#define UI_QUICK_SLIDER_KNOB 16
#define UI_QUICK_PANEL_TOP 88
#define UI_QUICK_NOTICE_SPACE 60
/// 横屏布局。/ Landscape layout.
static inline bool ui_quick_wide(void) { return epd_rotated_display_width() > epd_rotated_display_height(); }
/// 面板外框。/ Panel frame.
static inline EpdRect ui_quick_panel_rect(void) {
    const int w = epd_rotated_display_width();
    return ui_quick_wide() ? (EpdRect){18, UI_QUICK_PANEL_TOP, w - 36, 242}
                           : (EpdRect){18, UI_QUICK_PANEL_TOP, w - 36, 436};
}
/// 快捷层高度：面板加下方提示胶囊位置。/ Layer height: the panel plus room for the notice pill.
static inline int ui_quick_height(void) {
    const EpdRect p = ui_quick_panel_rect();
    return p.y + p.height + UI_QUICK_NOTICE_SPACE;
}
#define UI_QUICK_HEIGHT (ui_quick_height())
static inline int ui_quick_center_x(unsigned i) {
    const EpdRect p = ui_quick_panel_rect();
    if (ui_quick_wide()) return p.x + (int)(2 * i + 1) * p.width / 16;
    return p.x + 81 + 162 * (int)(i % 4);
}
static inline int ui_quick_center_y(unsigned i) { return ui_quick_wide() ? 152 : (i < 4 ? 156 : 296); }
#define UI_QUICK_CENTER_X(i) ui_quick_center_x(i)
#define UI_QUICK_CENTER_Y(i) ui_quick_center_y(i)
/// 滑杆轨道起点、中心线与图标中心。/ Slider track start, centre line and icon centre.
static inline int ui_quick_slider_x0(bool warmth) {
    const EpdRect p = ui_quick_panel_rect();
    if (ui_quick_wide()) return p.x + 86 + (warmth ? p.width / 2 : 0);
    return p.x + 94;
}
static inline int ui_quick_slider_y(bool warmth) { return ui_quick_wide() ? 278 : (warmth ? 470 : 414); }
static inline int ui_quick_slider_icon_x(bool warmth) { return ui_quick_slider_x0(warmth) - 44; }
#include "ui_quick_brightness.h"
#else
#define UI_QUICK_BUTTON_COUNT 4
#define UI_QUICK_HEIGHT 264
#define UI_QUICK_CIRCLE_Y 156
#define UI_QUICK_RADIUS 54
#define UI_QUICK_HIT_RADIUS 66
#endif

typedef enum {
    UI_QUICK_NONE, UI_QUICK_OPEN, UI_QUICK_CLOSE,
#if DPREADER_BOARD_METALIO47
    UI_QUICK_WIFI, UI_QUICK_BLUETOOTH, UI_QUICK_KOSYNC, UI_QUICK_FULL,
    UI_QUICK_LOCK, UI_QUICK_FRONTLIGHT, UI_QUICK_ROTATE, UI_QUICK_INVERT,
    UI_QUICK_BRIGHTNESS, UI_QUICK_WARMTH, UI_QUICK_LIGHT_COMMIT,
#else
    UI_QUICK_WIFI, UI_QUICK_BLUETOOTH, UI_QUICK_FULL, UI_QUICK_LOCK,
#endif
} ui_quick_action_t;

typedef struct {
    bool open, tracking, swallow, moved;
    uint16_t x0, y0;
    int hit;
#if DPREADER_BOARD_METALIO47
    int slider_value;
    int slider_position;
#else
#endif
    int64_t started_ms;
} ui_quick_menu_t;

/// 清除层和手势；切页、睡眠、媒体失效时使用。/ Clear layer and contact state at navigation, sleep and media-loss boundaries.
void ui_quick_menu_reset(ui_quick_menu_t *menu);
/// 返回true表示整条触摸序列被接管；多指/读错不执行按钮，收起之后仍吞到抬手。
/// True owns the whole contact stream; multitouch/read errors never activate controls, and dismissal still swallows until release.
bool ui_quick_menu_feed(ui_quick_menu_t *menu, const app_ctx_t *ctx, bool allowed,
                       ui_quick_action_t *action);
/// 取消接触但保留已打开的层。/ Cancel a contact while retaining an already open layer.
void ui_quick_menu_cancel_input(ui_quick_menu_t *menu);
/// 只覆盖顶部，保留下层其他像素；44px图标与底栏相同，状态栏正常绘制。
/// Cover the top only, leaving other pixels intact; use the same 44px icon size as navigation and draw the normal status bar.
/// soften_text仅用于阅读正文：轻微柔化玻璃背后的文字，收起恢复原像素，点按不重复模糊。
/// soften_text is for reader body only: gently blur text behind glass, restore original pixels on close and never reblur on input.
#if DPREADER_BOARD_METALIO47
/// active按键序位掩码（见UI_QUICK_*顺序）；rotate_enabled为假时“Xoay”变灰。
/// active is a bit mask in button order; when rotate_enabled is false the rotate tile is greyed.
void ui_quick_menu_draw(uint8_t *fb, unsigned active, bool rotate_enabled, unsigned brightness, unsigned warmth);
/// 独立方键的绘图与内部刷新范围；不重新合成下层。/ Draw one tile without recomposing the underlay.
bool ui_quick_menu_draw_button(uint8_t *fb, unsigned index, bool enabled, bool pressed, EpdRect *dirty);
#else
void ui_quick_menu_draw(uint8_t *fb, bool wifi, bool bluetooth,bool soften_text);
/// 当前按住的圆键，取消/滑出/释放时为-1。/ The held key, or -1 after cancellation, movement or release.
#endif
int ui_quick_menu_pressed(const ui_quick_menu_t *menu);
/// 精确还原顶部底图；缓存不可用时返回false，调用方重绘当前页。/ Restore the exact top underlay; false asks the caller to repaint the current page.
bool ui_quick_menu_restore(uint8_t *fb);
#if DPREADER_BOARD_METALIO47
void ui_quick_menu_end(void);
#else
/// 恢复原玻璃小块并画状态/圆内按压，不分配、不模糊、不排字。/ Restore a glass tile and paint its state/interior press without allocating, blurring or shaping text.
bool ui_quick_menu_draw_button(uint8_t *fb, int button, bool active, bool pressed, EpdRect *dirty);
/// 只重画清晰状态栏，供网络状态变化时局刷。/ Repaint only the sharp status strip for network changes.
#endif
void ui_quick_menu_draw_status(uint8_t *fb);
#if DPREADER_BOARD_METALIO47
EpdRect ui_quick_button_rect(unsigned index);
/// Bounded radio feedback inside the saved top underlay; no allocation.
EpdRect ui_quick_menu_draw_notice(uint8_t *fb,const char *text);
/// 滑块位置0..240，横向每单位2像素。/ Slider positions 0..240, two horizontal pixels per unit.
void ui_quick_menu_draw_slider(uint8_t *fb, bool warmth, unsigned value);
EpdRect ui_quick_slider_rect(bool warmth);
EpdRect ui_quick_slider_change_rect(bool warmth, unsigned before, unsigned after);
#else
/// 释放菜单存续期的控件缓存。/ Release the control cache retained while the menu is open.
void ui_quick_menu_end(void);
#endif
EpdRect ui_quick_menu_rect(void);
