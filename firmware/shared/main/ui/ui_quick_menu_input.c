/* SPDX-License-Identifier: Apache-2.0
 * 中文：顶部边缘识别器只产出快捷动作，不访问页面、硬件或刷新。
 * English: The top-edge recognizer emits quick actions without accessing pages, hardware or presentation.
 */
#include "ui_quick_menu.h"
#include <stdlib.h>
#include <string.h>
#include "ui_kit.h"
#include "ui_menu.h"

#if DPREADER_BOARD_METALIO47
#else
static int center_x(int button) { return (button * 2 + 1) * UI_LOCK_WIDTH / 8; }

#endif
#if DPREADER_BOARD_METALIO47
EpdRect ui_quick_menu_rect(void) { return (EpdRect){0, 0, epd_rotated_display_width(), UI_QUICK_HEIGHT}; }
#else
EpdRect ui_quick_menu_rect(void) { return (EpdRect){0, 0, UI_LOCK_WIDTH, UI_QUICK_HEIGHT}; }
#endif

void ui_quick_menu_reset(ui_quick_menu_t *menu) { memset(menu, 0, sizeof(*menu)); }

void ui_quick_menu_cancel_input(ui_quick_menu_t *menu) {
    menu->tracking = false;
    menu->swallow = true;
}

#if DPREADER_BOARD_METALIO47
#else
int ui_quick_menu_pressed(const ui_quick_menu_t *menu) {
    return menu->open && menu->tracking && !menu->moved ? menu->hit : -1;
}

#endif
static void dismiss(ui_quick_menu_t *menu, ui_quick_action_t *action) {
    menu->open = menu->tracking = false;
    menu->swallow = true;
    *action = UI_QUICK_CLOSE;
}

static int hit_test(int x, int y) {
#if DPREADER_BOARD_METALIO47
    // 方键命中区含下方文字；滑杆命中区上下各放宽，便于手指拖动。
    // Tile hits include the label below; slider hits are padded vertically for finger drags.
    for (int i=0;i<2;i++) {
        const int x0=ui_quick_slider_x0(i!=0),cy=ui_quick_slider_y(i!=0);
        if(x>=x0-24 && x<=x0+UI_QUICK_SLIDER_LENGTH+24 && y>=cy-30 && y<=cy+30)
            return UI_QUICK_BUTTON_COUNT+i;
    }
    for (int i = 0; i < UI_QUICK_BUTTON_COUNT; ++i) {
        const int cx=UI_QUICK_CENTER_X(i),cy=UI_QUICK_CENTER_Y(i);
        if (x>=cx-70 && x<=cx+70 && y>=cy-UI_QUICK_RADIUS-8 && y<=cy+UI_QUICK_RADIUS+UI_QUICK_LABEL_GAP+16) return i;
    }
    return -1;
}
static int slider_position(int x, bool warmth) {
    const int x0=ui_quick_slider_x0(warmth);
    int position=(x-x0+1)/2;
    if(position<0) position=0;
    if(position>UI_QUICK_SLIDER_RANGE) position=UI_QUICK_SLIDER_RANGE;
    return position;
}
#else
    for (int i = 0; i < UI_QUICK_BUTTON_COUNT; ++i) {
        const int dx = x - center_x(i), dy = y - UI_QUICK_CIRCLE_Y;
        if (dx * dx + dy * dy <= UI_QUICK_HIT_RADIUS * UI_QUICK_HIT_RADIUS) return i;
    }
    return -1;
}
#endif

static bool outside_layer(int x, int y) {
#if DPREADER_BOARD_METALIO47
    // 面板外任意处点按即收起（含横屏两侧与提示胶囊）。/ Any tap outside the panel dismisses, including the notice pill.
    const EpdRect p = ui_quick_panel_rect();
    return y >= p.y + p.height || x < p.x || x >= p.x + p.width;
#else
    (void)x;
    return y >= UI_QUICK_HEIGHT;
#endif
}

bool ui_quick_menu_feed(ui_quick_menu_t *menu, const app_ctx_t *ctx, bool allowed,
                       ui_quick_action_t *action) {
    *action = UI_QUICK_NONE;
    if (!allowed) {
        bool owned = menu->tracking || menu->swallow || menu->open;
        if (menu->open) dismiss(menu, action);
        else ui_quick_menu_reset(menu);
        return owned;
    }
    if (!ctx->touch || (ctx->touch->touched && ctx->touch->count != 1)) {
        bool owned = menu->open || menu->tracking || menu->swallow;
        if (owned) ui_quick_menu_cancel_input(menu);
        return owned;
    }
    if (menu->swallow) {
        if (!ctx->touch->touched) menu->swallow = false;
        return true;
    }
    int x = ctx->touch->x, y = ctx->touch->y;
    if (ctx->pressed && ctx->touch->touched) {
        if (!menu->open && y > UI_QUICK_EDGE_PX) return false;
        if (menu->open && ui_key_hit_test(x, y) >= 0) {
            dismiss(menu, action);
            return true;
        }
        menu->tracking = true;
        menu->moved = false;
        menu->x0 = x; menu->y0 = y;
        menu->started_ms = ctx->now_ms;
        menu->hit = hit_test(x, y);
#if DPREADER_BOARD_METALIO47
        if(menu->open && menu->hit>=UI_QUICK_BUTTON_COUNT) {
            menu->slider_position=slider_position(x,menu->hit!=UI_QUICK_BUTTON_COUNT);
            menu->slider_value=((menu->hit==UI_QUICK_BUTTON_COUNT?ui_quick_brightness_value(menu->slider_position):(unsigned)menu->slider_position)*100+UI_QUICK_SLIDER_RANGE/2)/UI_QUICK_SLIDER_RANGE;
            *action=menu->hit==UI_QUICK_BUTTON_COUNT?UI_QUICK_BRIGHTNESS:UI_QUICK_WARMTH;
            return true;
        }
#else
#endif
    }
    if (!menu->tracking) return menu->open;
#if DPREADER_BOARD_METALIO47
    // 滑块一旦捕获，整段拖动归它所有，越界或上滑不能切页。
    // Captured sliders own the complete drag, including movement outside the track.
    if(menu->open && menu->hit>=UI_QUICK_BUTTON_COUNT) {
        if(ctx->released) {
            menu->tracking=false;
            *action=UI_QUICK_LIGHT_COMMIT;
        } else if(ctx->touch->touched) {
            int position=slider_position(x,menu->hit!=UI_QUICK_BUTTON_COUNT);
            if(position!=menu->slider_position) {
                menu->slider_position=position;
                menu->slider_value=((menu->hit==UI_QUICK_BUTTON_COUNT?ui_quick_brightness_value(position):(unsigned)position)*100+UI_QUICK_SLIDER_RANGE/2)/UI_QUICK_SLIDER_RANGE;
                *action=menu->hit==UI_QUICK_BUTTON_COUNT?UI_QUICK_BRIGHTNESS:UI_QUICK_WARMTH;
            }
        }
        return true;
    }
#else
#endif
    int dx = x - (int)menu->x0, dy = y - (int)menu->y0;
    if (ctx->now_ms < menu->started_ms) {
        ui_quick_menu_cancel_input(menu);
        return true;
    }
    if (abs(dx) > UI_TOUCH_SLOP_PX || abs(dy) > UI_TOUCH_SLOP_PX) menu->moved = true;
    if (!menu->open && dy >= UI_QUICK_SWIPE_PX && dy >= abs(dx) * 2 &&
        ctx->now_ms - menu->started_ms <= 1500) {
        menu->open = true;
        menu->tracking = false;
        menu->swallow = true;
        *action = UI_QUICK_OPEN;
    } else if (menu->open && dy <= -UI_QUICK_SWIPE_PX && -dy >= abs(dx) * 2) {
        dismiss(menu, action);
    } else if (ctx->released) {
        menu->tracking = false;
        if (menu->open && !menu->moved) {
            if (menu->hit >= 0 && hit_test(x, y) == menu->hit) {
                static const ui_quick_action_t actions[] = {
#if DPREADER_BOARD_METALIO47
                    UI_QUICK_WIFI, UI_QUICK_BLUETOOTH, UI_QUICK_KOSYNC, UI_QUICK_FULL,
                    UI_QUICK_LOCK, UI_QUICK_FRONTLIGHT, UI_QUICK_ROTATE, UI_QUICK_INVERT,
#else
                    UI_QUICK_WIFI, UI_QUICK_BLUETOOTH, UI_QUICK_FULL, UI_QUICK_LOCK,
#endif
                };
                *action = actions[menu->hit];
            } else if (outside_layer(menu->x0, menu->y0) && outside_layer(x, y)) {
                dismiss(menu, action);
                menu->swallow = false;
            }
        }
    }
    return true;
}
#if DPREADER_BOARD_METALIO47

// Only a stationary held circle can show press feedback; sliders own their entire drag.
int ui_quick_menu_pressed(const ui_quick_menu_t *menu) {
    return menu&&menu->open&&menu->tracking&&!menu->swallow&&!menu->moved&&menu->hit>=0&&menu->hit<UI_QUICK_BUTTON_COUNT ? menu->hit : -1;
}
#else
#endif
