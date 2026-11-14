/* SPDX-License-Identifier: Apache-2.0 */
#include "app.h"
#include "ui_kit.h"
#include "ui_menu.h"
#include "ui_nav.h"
#include "ui_gesture.h"
extern const app_desc_t app_caro, app_device_settings;
// Built-in catalogue: adding an entry does not change the event loop.
typedef struct { const char *name, *description; const app_desc_t *page; } catalogue_entry_t;
static const catalogue_entry_t catalogue[]={
    {"Cờ caro", "2 người · Bàn 15 × 15 · Chơi ngoại tuyến", &app_caro},
};
static EpdRect entry_rect(unsigned i) { return (EpdRect){36,238+(int)i*166,612,142}; }
static EpdRect settings_rect(void) { return (EpdRect){444,92,204,64}; }
static void render(app_ctx_t *ctx,uint8_t *fb) {
    (void)ctx;ui_clear_page(fb);ui_nav_status(fb);
    ui_text_box(fb,(EpdRect){36,92,370,64},40,"App",EPD_DRAW_ALIGN_LEFT,false);
    ui_draw_soft_card(fb,settings_rect(),16,false);
    ui_text_box(fb,settings_rect(),22,"Cài đặt",EPD_DRAW_ALIGN_CENTER,false);
    ui_text_box(fb,(EpdRect){36,176,612,42},23,"Ứng dụng có sẵn trên LumenOS",EPD_DRAW_ALIGN_LEFT,false);
    for(unsigned i=0;i<sizeof(catalogue)/sizeof(catalogue[0]);++i) {
        EpdRect r=entry_rect(i);ui_draw_soft_card(fb,r,20,false);
        ui_text_box(fb,(EpdRect){r.x+24,r.y+20,r.width-48,48},30,catalogue[i].name,EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){r.x+24,r.y+82,r.width-48,38},20,catalogue[i].description,EPD_DRAW_ALIGN_LEFT,false);
    }
    ui_nav_draw(fb,UI_NAV_APPS);
}
static app_redraw_t gesture(app_ctx_t *ctx,const ui_gesture_event_t *ev) {
    if(ev->type!=UI_GESTURE_TAP) return APP_REDRAW_NONE;
    int tab=ui_nav_hit(ev->x0,ev->y0);
    if(tab>=0) { ui_nav_request(ctx,tab);return APP_REDRAW_NONE; }
    if(ui_rect_hit(settings_rect(),ev->x0,ev->y0)) ctx->request_app=&app_device_settings;
    for(unsigned i=0;i<sizeof(catalogue)/sizeof(catalogue[0]);++i)
        if(ui_rect_hit(entry_rect(i),ev->x0,ev->y0))ctx->request_app=catalogue[i].page;
    return APP_REDRAW_NONE;
}
static bool visible(app_ctx_t *ctx) { (void)ctx;return true; }
static bool no_handle(app_ctx_t *ctx) { (void)ctx;return false; }
static app_redraw_t key(app_ctx_t *ctx,int k) {
    if(k==UI_KEY_2)ctx->request_app=&app_caro;
    return APP_REDRAW_NONE;
}
static bool main_key(app_ctx_t *ctx,int k,app_redraw_t *redraw) {
    if(k!=UI_KEY_2)return false;
    *redraw=key(ctx,k);return true;
}
const app_desc_t app_apps={
    .title="App",.detail="Ứng dụng tích hợp",.enter_full=true,.binary_ui=true,
    .owns_keys=true,.defer_middle_short=true,.persistent_navigation=true,.main_page_visible=visible,
    .menu_handle_enabled=no_handle,.render=render,.on_gesture=gesture,.on_key=key,.on_main_key=main_key,
};
