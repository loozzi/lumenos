"""Row press outline (1.6.3): register from cards, outline, exact restore, nesting, cancel and drop."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "main/ui"

STUBS = {
    "ui_kit.h": r"""#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef int ui_icon_t;
enum {UI_ICON_HOUSE=1,UI_ICON_LIBRARY_BIG,UI_ICON_FOLDER,UI_ICON_SETTINGS,UI_ICON_ARROW_LEFT,UI_ICON_FOLDER_OPEN,UI_ICON_SLIDERS_HORIZONTAL};
enum {EPD_ROT_LANDSCAPE,EPD_ROT_PORTRAIT,EPD_ROT_INVERTED_LANDSCAPE,EPD_ROT_INVERTED_PORTRAIT};
#define UI_GRAY_WHITE 0xff
#define UI_GRAY_BLACK 0x00
#define UI_TOUCH_SLOP_PX 24
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define W 684
#define H 400
int epd_get_rotation(void);
int epd_width(void);
int epd_height(void);
int epd_rotated_display_width(void);
int epd_rotated_display_height(void);
uint8_t epd_get_pixel(int x,int y,int w,int h,const uint8_t *fb);
void epd_draw_pixel(int x,int y,uint8_t c,uint8_t *fb);
void epd_fill_rect(EpdRect r,uint8_t c,uint8_t *fb);
void ui_fill_round_rect(uint8_t *fb,EpdRect r,int radius,uint8_t c);
void ui_draw_round_rect(uint8_t *fb,EpdRect r,int radius,uint8_t c);
void ui_draw_icon(uint8_t *fb,int cx,int cy,int px,ui_icon_t icon,uint8_t c);
void ui_image_bw_rect(uint8_t *fb,EpdRect r);
extern void (*ui_soft_card_hook)(uint8_t *framebuffer, EpdRect rect, int radius);
""",
    "esp_heap_caps.h": "#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n"
                       "static inline void *heap_caps_malloc(size_t n,int caps){(void)caps;return malloc(n);}\n"
                       "static inline void heap_caps_free(void *p){free(p);}\n",
    "settings.h": "#pragma once\n#include <stdbool.h>\nbool app_settings_main_fast_refresh(void);\n",
    "ui_nav_layout.h": "#pragma once\n#define UI_NAV_TOUCH_TOP 1100\n",
    "ui_nav.h": "#pragma once\n#include \"ui_kit.h\"\nint ui_nav_hit(int x,int y);\nEpdRect ui_nav_tab_rect(int tab);\n",
}

HARNESS = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui_click_feedback.h"
void (*ui_soft_card_hook)(uint8_t *framebuffer, EpdRect rect, int radius);
static uint8_t fb[W*H], snap[W*H];
int epd_get_rotation(void){return EPD_ROT_LANDSCAPE;}
int epd_width(void){return W;}
int epd_height(void){return H;}
int epd_rotated_display_width(void){return W;}
int epd_rotated_display_height(void){return H;}
uint8_t epd_get_pixel(int x,int y,int w,int h,const uint8_t *f){(void)h;return f[y*w+x];}
void epd_draw_pixel(int x,int y,uint8_t c,uint8_t *f){assert(x>=0&&y>=0&&x<W&&y<H);f[y*W+x]=c;}
void epd_fill_rect(EpdRect r,uint8_t c,uint8_t *f){for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)epd_draw_pixel(x,y,c,f);}
void ui_fill_round_rect(uint8_t *f,EpdRect r,int radius,uint8_t c){(void)radius;epd_fill_rect(r,c,f);}
static int outlines;
void ui_draw_round_rect(uint8_t *f,EpdRect r,int radius,uint8_t c){(void)radius;++outlines;
 for(int x=r.x;x<r.x+r.width;++x){epd_draw_pixel(x,r.y,c,f);epd_draw_pixel(x,r.y+r.height-1,c,f);}
 for(int y=r.y;y<r.y+r.height;++y){epd_draw_pixel(r.x,y,c,f);epd_draw_pixel(r.x+r.width-1,y,c,f);}}
void ui_draw_icon(uint8_t *f,int cx,int cy,int px,ui_icon_t icon,uint8_t c){(void)f;(void)cx;(void)cy;(void)px;(void)icon;(void)c;}
void ui_image_bw_rect(uint8_t *f,EpdRect r){(void)f;(void)r;}
bool app_settings_main_fast_refresh(void){return false;}
int ui_nav_hit(int x,int y){(void)x;(void)y;return -1;}
EpdRect ui_nav_tab_rect(int tab){(void)tab;return (EpdRect){0};}
static void card(EpdRect r,uint8_t c){ui_fill_round_rect(fb,r,20,c);ui_soft_card_hook(fb,r,20);}
static bool same(EpdRect a,EpdRect b){return a.x==b.x&&a.y==b.y&&a.width==b.width&&a.height==b.height;}
static bool inside(EpdRect a,EpdRect r){return a.x>=r.x&&a.y>=r.y&&a.x+a.width<=r.x+r.width&&a.y+a.height<=r.y+r.height;}
int main(void){
 memset(fb,0xff,sizeof fb);
 ui_click_feedback_begin(fb);
 assert(ui_soft_card_hook);
 EpdRect row={36,100,612,94},dark={36,220,612,94},chip={36,330,60,30},button={500,120,120,54};
 card(row,0xdd);card(dark,0x40);card(chip,0xdd);card(button,0xbb); // chip too small: not a row
 memcpy(snap,fb,sizeof fb);
 EpdRect a;
 // Ô quá nhỏ không được đăng ký. / Tiny chips are not registered.
 assert(!ui_click_feedback_press(fb,50,340,&a)&&!ui_click_feedback_active());
 // Nhấn hàng: viền đen 3 px, ruột giữ nguyên. / Row press: 3 px black outline, interior untouched.
 assert(ui_click_feedback_press(fb,100,140,&a)&&ui_click_feedback_deferred()&&inside(a,row));
 assert(fb[row.y*W+row.x+300]==0&&fb[(row.y+2)*W+row.x+300]==0&&fb[(row.y+3)*W+row.x+300]==0xdd);
 assert(fb[(row.y+40)*W+row.x+300]==0xdd&&outlines==3);
 assert(same(ui_click_feedback_area(),row));
 // Trượt ra ngoài hoặc quá ngưỡng thì hủy. / Leaving or exceeding slop cancels.
 assert(!ui_click_feedback_cancel_at(110,150)&&ui_click_feedback_cancel_at(100,140+30)&&ui_click_feedback_cancel_at(10,140));
 assert(ui_click_feedback_release(fb,&a)&&inside(a,row)&&!memcmp(fb,snap,sizeof fb)&&!ui_click_feedback_active());
 // Thẻ tối dùng viền trắng. / Dark cards get a white outline.
 assert(ui_click_feedback_press(fb,100,260,&a)&&fb[dark.y*W+dark.x+300]==0xff);
 assert(ui_click_feedback_release(fb,&a)&&!memcmp(fb,snap,sizeof fb));
 // Thẻ đăng ký sau (nút nằm trên hàng) được ưu tiên. / The later card (a button on the row) wins.
 assert(ui_click_feedback_press(fb,550,140,&a)&&inside(a,button)&&same(ui_click_feedback_area(),button));
 ui_click_feedback_discard();assert(!ui_click_feedback_deferred()&&memcmp(fb,snap,sizeof fb));
 memcpy(fb,snap,sizeof fb);
 // Cập nhật vùng: bỏ đăng ký hàng. / Area updates drop row registrations.
 ui_click_feedback_drop_rows();assert(!ui_click_feedback_press(fb,100,140,&a));
 // Khung mới (vẽ lại trang) mới đăng ký lại; ảnh nền khác khung thì bỏ qua. / Only the loop frame registers.
 static uint8_t other[W*H];ui_soft_card_hook(other,row,20);assert(!ui_click_feedback_press(fb,100,140,&a));
 ui_click_feedback_begin(fb);card(row,0xdd);assert(ui_click_feedback_press(fb,100,140,&a));
 ui_click_feedback_begin(fb);assert(!ui_click_feedback_active()); // vẽ lại trang giải phóng cache / repaint frees cache
 // Tối đa 24 ô; ô vượt bị bỏ qua. / At most 24 controls.
 for(int i=0;i<30;++i)card((EpdRect){36,10+i*11,612,44},0xdd);
 puts("ui click row: register, outline, exact restore, dark ink, nesting, cancel, discard, drop and cap passed");
 return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="click-row-") as tmp:
    d = Path(tmp)
    for name, text in STUBS.items():
        (d / name).write_text(text)
    for name in ("ui_click_feedback.c", "ui_click_feedback.h"):
        shutil.copy(UI / name, d / name)
    (d / "test.c").write_text(HARNESS)
    subprocess.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-DDPREADER_BOARD_METALIO47=1",
                    "-fsanitize=address,undefined", "-I", str(d), str(d / "test.c"),
                    str(d / "ui_click_feedback.c"), "-o", str(d / "test")], check=True)
    subprocess.run([str(d / "test")], check=True)
