#!/usr/bin/env python3
"""Compile production catalogue/game callbacks and navigation against small UI fakes."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
def run(args): subprocess.run(args,check=True)
def function(text,name):
    start=text.index(name+'(')
    start=text.rfind('\n',0,start)+1
    brace=text.index('{',start);depth=1;end=brace+1
    while depth:
        depth += (text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]+'\n'
with tempfile.TemporaryDirectory(prefix='lumen-apps-') as folder:
    out=Path(folder)
    (out/'app.h').write_text((ROOT/'main/app/app.h').read_text())
    (out/'ui_gesture.h').write_text((ROOT/'main/ui/ui_gesture.h').read_text())
    (out/'ui_menu.h').write_text((ROOT/'main/ui/ui_menu.h').read_text())
    for name, content in {
        'app_config.h':'',
        'cst836u.h':'typedef void *cst836u_handle_t; typedef struct {int unused;} cst836u_touch_t,cst836u_info_t;',
        'epd_highlevel.h':'typedef struct {int unused;} EpdiyHighlevelState;',
        'sc7a20h_lab.h':'typedef void *sc7a20h_handle_t;',
        'epdiy.h':'#include <stddef.h>\ntypedef struct {int x,y,width,height;} EpdRect;',
    }.items(): (out/name).write_text('#pragma once\n'+content+'\n')
    (out/'ui_nav.h').write_text('''#include "app.h"
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define UI_NAV_TOUCH_TOP 1080
const app_desc_t *app_at(int index);
enum { UI_NAV_HOME=0,UI_NAV_BOOKS=1,UI_NAV_FILES=2,UI_NAV_SETTINGS=3,UI_NAV_APPS=6 };
void ui_nav_status(uint8_t*);void ui_nav_back(uint8_t*,int,int);void ui_nav_draw(uint8_t*,int);
int ui_nav_hit(uint16_t,uint16_t);void ui_nav_request(app_ctx_t*,int);
int ui_nav_slot(int);int ui_nav_next(int,int);
bool app_settings_simple_ui(void);
''')
    (out/'ui_kit.h').write_text('''#include "app.h"
#define EPD_DRAW_ALIGN_LEFT 0
#define EPD_DRAW_ALIGN_CENTER 1
void ui_clear_page(uint8_t*);void ui_draw_soft_card(uint8_t*,EpdRect,int,bool);
void ui_text_box(uint8_t*,EpdRect,int,const char*,int,bool);
bool ui_rect_hit(EpdRect,uint16_t,uint16_t);
void ui_fill_rect_fast(EpdRect,uint8_t,uint8_t*);
void epd_draw_line(int,int,int,int,int,uint8_t*);void epd_draw_circle(int,int,int,int,uint8_t*);
''')
    nav=(ROOT/'main/ui/ui_nav.c').read_text()
    routes=nav[nav.index('static const int nav_routes'):nav.index('\n',nav.index('static const int nav_routes'))]
    (out/'nav.c').write_text('#include "ui_nav.h"\n'+routes+'\n'+''.join(function(nav,n) for n in ['ui_nav_slot','ui_nav_next','ui_nav_hit','ui_nav_request']))
    (out/'test.c').write_text('''#include "app.h"
#include "ui_kit.h"
#include "ui_nav.h"
#include "ui_gesture.h"
#include "ui_menu.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
extern const app_desc_t app_apps,app_caro;
const app_desc_t app_device_settings={0};
static app_desc_t pages[6];static bool simple;
const app_desc_t *app_at(int i){if(i==6)return &app_apps;assert(i>=0&&i<6);return &pages[i];}
bool app_settings_simple_ui(void){return simple;}
static void bounds(EpdRect r){assert(r.x>=0&&r.y>=0&&r.width>0&&r.height>0&&r.x+r.width<=684&&r.y+r.height<=1088);}
void ui_clear_page(uint8_t*f){(void)f;}void ui_nav_status(uint8_t*f){(void)f;}
void ui_nav_back(uint8_t*f,int x,int y){(void)f;(void)x;(void)y;}
void ui_nav_draw(uint8_t*f,int n){(void)f;assert(n==UI_NAV_APPS);}
void ui_draw_soft_card(uint8_t*f,EpdRect r,int n,bool b){(void)f;(void)n;(void)b;bounds(r);}
void ui_text_box(uint8_t*f,EpdRect r,int p,const char*s,int a,bool b){(void)f;(void)p;(void)s;(void)a;(void)b;bounds(r);}
bool ui_rect_hit(EpdRect r,uint16_t x,uint16_t y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
void ui_fill_rect_fast(EpdRect r,uint8_t c,uint8_t*f){(void)c;(void)f;bounds(r);}
void epd_draw_line(int x,int y,int xx,int yy,int c,uint8_t*f){(void)c;(void)f;bounds((EpdRect){x,y,1,1});bounds((EpdRect){xx,yy,1,1});}
void epd_draw_circle(int x,int y,int r,int c,uint8_t*f){(void)c;(void)f;bounds((EpdRect){x-r,y-r,r*2+1,r*2+1});}
static app_redraw_t tap(app_ctx_t*c,int x,int y){ui_gesture_event_t e={.type=UI_GESTURE_TAP,.x0=x,.y0=y};return app_caro.on_gesture(c,&e);}
int main(void){
 app_ctx_t c={0};const int routes[]={0,6,1,2};
 for(int edition=0;edition<2;++edition){simple=edition;
 for(int i=0;i<4;++i){assert(ui_nav_hit(i*171,1100)==routes[i]);assert(ui_nav_slot(routes[i])==i);assert(ui_nav_next(routes[i],1)==routes[(i+1)%4]);assert(ui_nav_next(routes[i],-1)==routes[(i+3)%4]);ui_nav_request(&c,routes[i]);assert(c.request_app==app_at(routes[i]));}
 assert(ui_nav_hit(684,1100)==-1);assert(ui_nav_hit(20,1079)==-1);assert(ui_nav_hit(20,1216)==-1);
 }
 simple=false;app_apps.render(&c,c.fb);ui_gesture_event_t e={.type=UI_GESTURE_TAP,.x0=100,.y0=270};app_apps.on_gesture(&c,&e);assert(c.request_app==&app_caro);
 e.x0=500;e.y0=110;app_apps.on_gesture(&c,&e);assert(c.request_app==&app_device_settings);
 app_caro.on_enter(&c);app_caro.render(&c,c.fb);
 assert(tap(&c,42,242)==APP_REDRAW_PAGE);assert(tap(&c,42,242)==APP_REDRAW_NONE);
 assert(tap(&c,641,841)==APP_REDRAW_PAGE);assert(tap(&c,642,842)==APP_REDRAW_NONE);
 assert(tap(&c,400,950)==APP_REDRAW_PAGE);app_caro.render(&c,c.fb); // reset confirmation
 assert(tap(&c,100,300)==APP_REDRAW_PAGE); // cancel consumes the tap
 assert(tap(&c,42,242)==APP_REDRAW_NONE); // old board preserved
 app_caro.on_exit(&c);app_caro.on_enter(&c);assert(tap(&c,42,242)==APP_REDRAW_NONE);
 assert(tap(&c,400,950)==APP_REDRAW_PAGE);assert(tap(&c,400,950)==APP_REDRAW_PAGE);assert(tap(&c,42,242)==APP_REDRAW_PAGE);
 assert(tap(&c,100,950)==APP_REDRAW_PAGE);assert(tap(&c,42,242)==APP_REDRAW_PAGE);
 app_caro.on_before_lock(&c);assert(tap(&c,42,242)==APP_REDRAW_NONE);
 app_caro.render(&c,c.fb);tap(&c,230,1150);assert(c.request_app==&app_apps);
 assert(app_caro.on_key(&c,UI_KEY_2)==APP_REDRAW_FULL);
 puts("App navigation and game UI: passed");
}
''')
    sources=[ROOT/'main/apps/app_apps.c',ROOT/'main/apps/app_caro.c',ROOT/'main/apps/caro.c',out/'nav.c',out/'test.c']
    run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined,bounds','-I'+str(out),*map(str,sources),'-o',str(out/'ui-test')])
    run([str(out/'ui-test')])
    run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined,bounds','-I'+str(ROOT/'main/apps'),str(ROOT/'main/apps/caro.c'),str(ROOT/'tools/caro_host_test.c'),'-o',str(out/'rules-test')])
    run([str(out/'rules-test')])

registry=(ROOT/'main/app/app_registry.c').read_text()
table=registry[registry.index('s_apps[]'):registry.index('};',registry.index('s_apps[]'))]
import re
assert re.findall(r'&([a-z_]+)',table)==['app_dashboard','app_book','app_files','app_device_settings','app_image','app_book_picker','app_apps','app_caro']
print('Stable registry routes: passed')
