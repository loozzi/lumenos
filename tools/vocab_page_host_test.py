#!/usr/bin/env python3
"""Drive the real Full vocabulary page with stub UI: enter, study, flip, answer, save and lock.
SPDX-License-Identifier: Apache-2.0

中文：用最小 UI 桩编译真实 app_vocab.c，在临时 /vocab 中：无包时显示说明；选包、开始、翻面、记得/忘记、
按键操作、结束保存进度与 today.txt；从文件页打开指定包；切换设置；拔卡后不写卡；锁屏前保存。
English: compile the real app_vocab.c with minimal UI stubs over a temporary /vocab: instructions without a
pack; choose a pack, start, flip, remember/forget, key control, save progress and today.txt at the end; open a
pack from Files; toggle settings; no card writes after removal; save before lock.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
STUBS = {
    'app.h': r'''
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef enum {APP_REDRAW_NONE,APP_REDRAW_AREA,APP_REDRAW_PAGE,APP_REDRAW_FULL,APP_REDRAW_DONE} app_redraw_t;
typedef struct app_desc_s app_desc_t;
typedef struct ui_gesture_event_s ui_gesture_event_t;
typedef struct {int64_t now_ms;const app_desc_t *request_app;} app_ctx_t;
struct app_desc_s {const char *title,*detail;bool owns_keys,persistent_navigation;
 void(*on_enter)(app_ctx_t*);void(*on_exit)(app_ctx_t*);void(*on_before_lock)(app_ctx_t*);
 void(*on_media_lost)(app_ctx_t*);void(*on_media_ready)(app_ctx_t*);void(*render)(app_ctx_t*,uint8_t*);
 app_redraw_t(*on_gesture)(app_ctx_t*,const ui_gesture_event_t*);app_redraw_t(*on_key)(app_ctx_t*,int);int64_t(*next_tick_ms)(app_ctx_t*);};
''',
    'app_registry.h': '#pragma once\n#include "app.h"\nconst app_desc_t *app_home_page(void);\n',
    'app_content_open.h': '#pragma once\n#include <stdbool.h>\nbool app_vocab_request_open(const char *path);\n',
    'ui_gesture.h': '#pragma once\n#include <stdint.h>\nenum{UI_GESTURE_TAP=1,UI_GESTURE_SWIPE_L};\nstruct ui_gesture_event_s{int type;uint16_t x0,y0;};\n',
    'ui_kit.h': r'''
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct{int x,y,width,height;}EpdRect;
enum EpdFontFlags{EPD_DRAW_ALIGN_LEFT,EPD_DRAW_ALIGN_CENTER,EPD_DRAW_ALIGN_RIGHT};
#define UI_GRAY_BLACK 0
void ui_text_box(uint8_t*,EpdRect,int,const char*,enum EpdFontFlags,bool);
void ui_text_vc(uint8_t*,int,int,int,const char*,enum EpdFontFlags,bool);
void ui_fill_round_rect(uint8_t*,EpdRect,int,uint8_t);void ui_draw_round_rect(uint8_t*,EpdRect,int,uint8_t);
void ui_fill_rect_fast(EpdRect,uint8_t,uint8_t*);void ui_clear_page(uint8_t*);
int ui_text_fixed_width_px(int,const char*);int ui_text_effective_px(int);
bool ui_rect_hit(EpdRect,uint16_t,uint16_t);
''',
    'ui_menu.h': '#pragma once\n#define UI_KEY_1 0\n#define UI_KEY_2 1\n#define UI_KEY_3 2\n',
    'ui_nav.h': '#pragma once\n#include "app.h"\nvoid ui_nav_draw(uint8_t*,int);void ui_nav_status(uint8_t*);void ui_nav_back(uint8_t*,int,int);int ui_nav_hit(uint16_t,uint16_t);void ui_nav_request(app_ctx_t*,int);\n',
    'ui_font.h': '#pragma once\n#include <stdbool.h>\nbool ui_font_has_text(const char*);\n',
}
TEST = r'''
#include "app.h"
#include "ui_gesture.h"
#include "ui_kit.h"
#include "vocab_today.h"
#include "vocab_study.h"
#include "app_content_open.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
extern const app_desc_t app_vocab;
static char drawn[64][200];static unsigned drawn_n;
static void note(const char *t){if(drawn_n<64)snprintf(drawn[drawn_n++],200,"%s",t);}
static bool saw(const char *t){for(unsigned i=0;i<drawn_n;i++)if(strstr(drawn[i],t))return true;return false;}
void ui_text_box(uint8_t*f,EpdRect r,int px,const char*t,enum EpdFontFlags a,bool i){(void)f;(void)r;(void)px;(void)a;(void)i;note(t);}
void ui_text_vc(uint8_t*f,int x,int y,int px,const char*t,enum EpdFontFlags a,bool i){(void)f;(void)x;(void)y;(void)px;(void)a;(void)i;note(t);}
void ui_fill_round_rect(uint8_t*f,EpdRect r,int d,uint8_t g){(void)f;(void)r;(void)d;(void)g;}
void ui_draw_round_rect(uint8_t*f,EpdRect r,int d,uint8_t g){(void)f;(void)r;(void)d;(void)g;}
void ui_fill_rect_fast(EpdRect r,uint8_t g,uint8_t*f){(void)r;(void)g;(void)f;}
void ui_clear_page(uint8_t*f){(void)f;drawn_n=0;}
int ui_text_fixed_width_px(int px,const char*t){return px*(int)strlen(t)/2;}
int ui_text_effective_px(int px){return px;}
bool ui_rect_hit(EpdRect r,uint16_t x,uint16_t y){return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;}
void ui_nav_draw(uint8_t*f,int a){(void)f;(void)a;}void ui_nav_status(uint8_t*f){(void)f;}void ui_nav_back(uint8_t*f,int x,int y){(void)f;(void)x;(void)y;}
int ui_nav_hit(uint16_t x,uint16_t y){(void)x;return y>1100?0:-1;}static int nav_requests;void ui_nav_request(app_ctx_t*c,int i){(void)c;(void)i;++nav_requests;}
bool ui_font_has_text(const char*t){return !strchr(t,'/');}
static const app_desc_t home={.title="home"};const app_desc_t *app_home_page(void){return &home;}
static app_ctx_t ctx;static uint8_t fb[16];
static app_redraw_t tap(int x,int y){ui_gesture_event_t e={UI_GESTURE_TAP,(uint16_t)x,(uint16_t)y};return app_vocab.on_gesture(&ctx,&e);}
static void draw(void){app_vocab.render(&ctx,fb);}
static void put(const char *p,const char *t){FILE *f=fopen(p,"w");assert(f);fputs(t,f);fclose(f);}
int main(void){
 assert(!strcmp(app_vocab.title,"Từ vựng")&&app_vocab.owns_keys);
 app_vocab.on_enter(&ctx);draw();assert(saw("Chép tệp CSV vào thư mục /vocab"));app_vocab.on_exit(&ctx);
 assert(!mkdir("vocab",0775));
 put("vocab/b.csv","english,type,vietnamese,pronounce,example\ninvoice,n.,hóa đơn,/x/,Send it.\nbudget,n.,ngân sách,,\nagenda,n.,chương trình,,\n");
 put("vocab/a.csv","english,vietnamese\nalpha,một\n");
 app_vocab.on_enter(&ctx);draw();
 assert(saw("a.csv")&&saw("Đổi bộ")&&saw("1 từ · đã học 0 · đến hạn 0 · mới 1")&&saw("Học 1 thẻ hôm nay"));
 assert(tap(300,250)==APP_REDRAW_PAGE);draw();assert(saw("b.csv")&&saw("3 từ"));
 struct stat st;assert(!stat("vocab/.lumen/active.txt",&st)&&!stat("vocab/.lumen/today.txt",&st));
 const vocab_today_t *t=vocab_today_cached();assert(t->valid&&!strcmp(t->english,"invoice")&&t->show_home&&t->show_lock);
 // 设置：新词 10→20，关闭锁屏显示。/ Settings: new words 10→20, turn the lock line off.
 assert(tap(300,550)==APP_REDRAW_PAGE);draw();assert(saw("20"));
 assert(tap(300,740)==APP_REDRAW_PAGE);assert(!vocab_today_cached()->show_lock);
 // 学习：正面、翻面、按钮回答。/ Study: front, flip, answer with buttons.
 assert(tap(300,430)==APP_REDRAW_PAGE);draw();assert(saw("Thẻ 1/3")&&saw("invoice")&&saw("n.")&&!saw("/x/")&&saw("Chạm thẻ"));
 assert(tap(500,980)==APP_REDRAW_NONE); // 未翻面不能回答 / no answer before flipping
 assert(tap(300,500)==APP_REDRAW_PAGE);draw();assert(saw("hóa đơn")&&saw("Send it.")&&saw("Nhớ")&&saw("Quên"));
 assert(tap(500,980)==APP_REDRAW_PAGE);draw();assert(saw("Thẻ 2/3")&&saw("budget"));
 // 按键：中键翻面，左键忘记。/ Keys: middle flips, left forgets.
 assert(app_vocab.on_key(&ctx,1)==APP_REDRAW_PAGE);assert(app_vocab.on_key(&ctx,0)==APP_REDRAW_PAGE);draw();assert(saw("Thẻ 3/4")&&saw("agenda"));
 assert(app_vocab.on_key(&ctx,2)==APP_REDRAW_PAGE);assert(app_vocab.on_key(&ctx,2)==APP_REDRAW_PAGE);draw();assert(saw("Thẻ 4/4")&&saw("budget"));
 app_vocab.on_key(&ctx,2);app_vocab.on_key(&ctx,2);draw();assert(saw("Xong phần hôm nay")&&saw("Nhớ 3 · Quên 1"));
 vocab_progress_t p={0};assert(vocab_progress_load(&p,"vocab/.lumen","b.csv")&&p.count==3&&p.new_per_day==20&&!p.show_lock);
 assert(vocab_state(&p,vocab_key("invoice"))->box==2&&vocab_state(&p,vocab_key("budget"))->box==2);vocab_progress_free(&p);
 assert(tap(300,430)==APP_REDRAW_PAGE);draw();assert(saw("Hôm nay đã học xong"));
 assert(tap(300,430)==APP_REDRAW_PAGE); // 无卡可学：留在主页 / nothing due: stay on the main view
 // 返回首页。/ Back to Home.
 assert(tap(40,100)==APP_REDRAW_NONE&&ctx.request_app==&home);ctx.request_app=NULL;
 assert(tap(300,1150)==APP_REDRAW_NONE&&nav_requests==1);
 app_vocab.on_exit(&ctx);
 // 从文件页打开 a.csv；/vocab 之外的路径拒绝。/ Open a.csv from Files; paths outside /vocab are refused.
 assert(!app_vocab_request_open("/sdcard/books/x.csv")&&!app_vocab_request_open("vocab/sub/x.csv"));
 assert(app_vocab_request_open("vocab/a.csv"));app_vocab.on_enter(&ctx);draw();assert(saw("a.csv")&&saw("1 từ"));
 // 学一张后锁屏：保存。/ Study one card, then lock: saved.
 assert(tap(300,430)==APP_REDRAW_PAGE);assert(app_vocab.on_key(&ctx,2)==APP_REDRAW_PAGE);
 app_vocab.on_before_lock(&ctx);
 assert(vocab_progress_load(&p,"vocab/.lumen","a.csv")&&p.count==0);vocab_progress_free(&p); // 未回答不写 / nothing answered yet
 assert(app_vocab.on_key(&ctx,2)==APP_REDRAW_PAGE);draw();assert(saw("Xong phần hôm nay"));
 assert(vocab_progress_load(&p,"vocab/.lumen","a.csv")&&p.count==1);vocab_progress_free(&p);
 // 拔卡：不再写卡。/ Card removed: no further writes.
 app_vocab.on_media_lost(&ctx);draw();assert(saw("Chép tệp CSV"));
 unlink("vocab/.lumen/today.txt");app_vocab.on_exit(&ctx);assert(stat("vocab/.lumen/today.txt",&st));
 assert(app_vocab.next_tick_ms(&ctx)==ctx.now_ms+1000);
 // 坏包：显示原因。/ A broken pack shows why.
 put("vocab/a.csv","word,meaning\n");unlink("vocab/b.csv");app_vocab.on_enter(&ctx);draw();assert(saw("Thiếu cột english"));
 app_vocab.on_exit(&ctx);
 puts("vocabulary page: PASS");return 0;
}
'''

if __name__ == '__main__':
    with tempfile.TemporaryDirectory() as d:
        p = Path(d)
        (p / 'stubs').mkdir()
        for name, text in STUBS.items():
            (p / 'stubs' / name).write_text(text)
        (p / 'test.c').write_text(TEST)
        # 引号包含先找源文件目录：把页面复制到桩旁边。/ Quoted includes search the source folder first: copy the page next to the stubs.
        (p / 'stubs/app_vocab.c').write_text((ROOT / 'main/apps/app_vocab.c').read_text())
        (p / 'run').mkdir()
        book = ROOT / 'main/book'
        subprocess.run(['cc', '-std=gnu11', '-g', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                        '-fno-sanitize-recover=all', '-DVOCAB_ROOT="vocab"', '-I' + str(p / 'stubs'), '-I' + str(book),
                        str(p / 'test.c'), str(p / 'stubs/app_vocab.c'), str(book / 'vocab_pack.c'),
                        str(book / 'vocab_study.c'), str(book / 'vocab_today.c'), '-o', str(p / 'test')], check=True)
        subprocess.run([str(p / 'test')], cwd=p / 'run', check=True)
    print('PASS vocabulary page: instructions, pack switch, settings, flip/answer by touch and keys, forget repeat, '
          'saved progress and today.txt, Files entry, lock save, card removal and broken-pack message')
