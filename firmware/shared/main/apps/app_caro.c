/* SPDX-License-Identifier: Apache-2.0 */
#include "app.h"
#include "caro.h"
#include "ui_kit.h"
#include "ui_menu.h"
#include "ui_nav.h"
#include "ui_gesture.h"
#include <stdio.h>
extern const app_desc_t app_apps;
// No tasks, timers or SD handles. RAM state survives tab changes and Light Sleep.
static caro_t game;
static bool initialized,confirm_reset;
enum { BOARD_X=42, BOARD_Y=242, CELL=40, BOARD_PX=CARO_SIZE*CELL };
static EpdRect undo_rect(void) { return (EpdRect){36,918,294,76}; }
static EpdRect reset_rect(void) { return (EpdRect){354,918,294,76}; }
static void enter(app_ctx_t *ctx) {
    (void)ctx;if(!initialized) { caro_reset(&game);initialized=true; }
    confirm_reset=false;
}
static void before_lock(app_ctx_t *ctx) { (void)ctx;confirm_reset=false; }
static void render(app_ctx_t *ctx,uint8_t *fb) {
    (void)ctx;ui_clear_page(fb);ui_nav_status(fb);
    ui_nav_back(fb,36,90);
    ui_text_box(fb,(EpdRect){116,92,520,64},36,"Cờ caro",EPD_DRAW_ALIGN_LEFT,false);
    char status[80];
    if(game.winner)snprintf(status,sizeof(status),"%c thắng · %u nước đi",game.winner==1?'X':'O',(unsigned)game.moves);
    else if(caro_finished(&game))snprintf(status,sizeof(status),"Hòa · Bàn cờ đã đầy");
    else snprintf(status,sizeof(status),"Lượt %c · %u nước đi",game.turn==1?'X':'O',(unsigned)game.moves);
    ui_text_box(fb,(EpdRect){36,170,612,48},28,status,EPD_DRAW_ALIGN_LEFT,false);
    for(int i=0;i<=CARO_SIZE;++i) {
        ui_fill_rect_fast((EpdRect){BOARD_X+i*CELL,BOARD_Y,1,BOARD_PX+1},0x70,fb);
        ui_fill_rect_fast((EpdRect){BOARD_X,BOARD_Y+i*CELL,BOARD_PX+1,1},0x70,fb);
    }
    for(int r=0;r<CARO_SIZE;++r)for(int c=0;c<CARO_SIZE;++c) {
        int cell=r*CARO_SIZE+c,v=game.cells[cell];if(!v)continue;
        int x=BOARD_X+c*CELL+CELL/2,y=BOARD_Y+r*CELL+CELL/2;
        if(game.moves&&game.history[game.moves-1]==cell)
            ui_fill_rect_fast((EpdRect){x-18,y-18,36,36},0xdd,fb);
        if(v==1) {
            for(int d=-1;d<=1;++d) {
                epd_draw_line(x-11,y-11+d,x+11,y+11+d,0,fb);
                epd_draw_line(x-11,y+11+d,x+11,y-11+d,0,fb);
            }
        } else for(int radius=10;radius<=12;++radius)epd_draw_circle(x,y,radius,0,fb);
    }
    ui_text_box(fb,(EpdRect){36,860,612,40},21,"5 quân liên tiếp để thắng · Chạm ô để đi",EPD_DRAW_ALIGN_CENTER,false);
    ui_draw_soft_card(fb,undo_rect(),18,false);ui_draw_soft_card(fb,reset_rect(),18,confirm_reset);
    ui_text_box(fb,undo_rect(),25,"Đi lại",EPD_DRAW_ALIGN_CENTER,false);
    ui_text_box(fb,reset_rect(),25,confirm_reset?"Xác nhận ván mới":"Ván mới",EPD_DRAW_ALIGN_CENTER,false);
    if(confirm_reset)ui_text_box(fb,(EpdRect){36,1010,612,48},20,"Chạm Ván mới lần nữa; chạm nơi khác để hủy",EPD_DRAW_ALIGN_CENTER,false);
    ui_nav_draw(fb,UI_NAV_APPS);
}
static app_redraw_t gesture(app_ctx_t *ctx,const ui_gesture_event_t *ev) {
    if(ev->type!=UI_GESTURE_TAP)return APP_REDRAW_NONE;
    int x=ev->x0,y=ev->y0,tab=ui_nav_hit(x,y);
    if(tab>=0) {confirm_reset=false;ui_nav_request(ctx,tab);return APP_REDRAW_NONE;}
    if(ui_rect_hit((EpdRect){24,88,80,76},x,y)) {confirm_reset=false;ctx->request_app=&app_apps;return APP_REDRAW_NONE;}
    if(ui_rect_hit(reset_rect(),x,y)) {
        if(confirm_reset||game.moves==0) {caro_reset(&game);confirm_reset=false;}
        else confirm_reset=true;
        return APP_REDRAW_PAGE;
    }
    bool was_confirm=confirm_reset;confirm_reset=false;
    if(was_confirm)return APP_REDRAW_PAGE;
    if(ui_rect_hit(undo_rect(),x,y))return caro_undo(&game)?APP_REDRAW_PAGE:APP_REDRAW_NONE;
    if(x>=BOARD_X&&x<BOARD_X+BOARD_PX&&y>=BOARD_Y&&y<BOARD_Y+BOARD_PX)
        return caro_play(&game,(y-BOARD_Y)/CELL,(x-BOARD_X)/CELL)?APP_REDRAW_PAGE:APP_REDRAW_NONE;
    return APP_REDRAW_NONE;
}
static app_redraw_t key(app_ctx_t *ctx,int k) {
    bool was_confirm=confirm_reset;confirm_reset=false;
    if(k==UI_KEY_1)ctx->request_app=&app_apps;
    if(k==UI_KEY_2)return APP_REDRAW_FULL;
    if(k==UI_KEY_3 && !was_confirm && caro_undo(&game))return APP_REDRAW_PAGE;
    return was_confirm?APP_REDRAW_PAGE:APP_REDRAW_NONE;
}
static app_redraw_t hold(app_ctx_t *ctx,int k) { (void)ctx;return k==UI_KEY_2?APP_REDRAW_FULL:APP_REDRAW_NONE; }
static bool no_handle(app_ctx_t *ctx) { (void)ctx;return false; }
const app_desc_t app_caro={
    .title="Cờ caro",.detail="Hai người · 15 × 15",.enter_full=true,.binary_ui=true,
    .owns_keys=true,.defer_middle_short=true,.persistent_navigation=true,.menu_handle_enabled=no_handle,
    .on_enter=enter,.on_exit=before_lock,.on_before_lock=before_lock,
    .render=render,.on_gesture=gesture,.on_key=key,.on_key_long=hold,
};
