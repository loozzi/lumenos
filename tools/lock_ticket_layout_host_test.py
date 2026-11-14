#!/usr/bin/env python3
"""Actual ticket composition shares cover geometry/signatures across both editions."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {unsigned pct,chapter;} book_progress_t;
#define EPD_DRAW_ALIGN_LEFT 0
#define EPD_DRAW_ALIGN_CENTER 1
#define UI_GRAY_WHITE 255
#define UI_GRAY_LIGHT 192
#define UI_GRAY_BLACK 0
static char signature[96],author[128]="Author";
static EpdRect cover,caption;
static char caption_text[128];
static unsigned caption_calls,head_lines=1;
static const char *app_settings_status_signature(void){return signature;}
static bool book_progress_last_path(char *p,size_t n){snprintf(p,n,"missing.epub");return true;}
static bool book_progress_load(const char*p,size_t n,book_progress_t *out){(void)p;(void)n;(void)out;return false;}
static bool book_title_get(const char*p,char*t,size_t n){(void)p;snprintf(t,n,"Title");return true;}
static bool book_title_from_path(const char*p,char*t,size_t n){return book_title_get(p,t,n);}
static int book_epub_metadata_cached(const char*p,char*t,size_t n,char*a,size_t m){(void)p;(void)t;(void)n;snprintf(a,m,"%s",author);return 0;}
static uint8_t *cover_load(const char*p,const char*t,const char*a,unsigned*w,unsigned*h,bool decode){(void)p;(void)t;(void)a;(void)w;(void)h;(void)decode;return NULL;}
static void book_ticket_draw_background(uint8_t*f,const uint8_t*g,unsigned w,unsigned h,bool packed){(void)f;(void)g;(void)w;(void)h;(void)packed;}
static void book_ticket_draw_shadow(uint8_t*f){(void)f;}
static void book_ticket_draw_paper(uint8_t*f){(void)f;}
static void ui_text(uint8_t*f,int x,int y,int px,const char*t,int a,bool inv){(void)f;(void)x;(void)y;(void)px;(void)t;(void)a;(void)inv;}
static void ui_hairline(uint8_t*f,int y,int x,int w,int c){(void)f;(void)c;assert(y>=0&&y<1216&&x>=0&&x+w<=684);}
static int ticket_heading(uint8_t*f,int y,int h,int px,const char*t){(void)f;(void)h;(void)px;(void)t;return y==202?head_lines:1;}
static void cover_draw(uint8_t*f,const uint8_t*g,unsigned w,unsigned h,const char*t,EpdRect box){(void)f;(void)g;(void)w;(void)h;(void)t;cover=box;assert(box.y>200&&box.y+box.height<870);}
static void ticket_stat(uint8_t*f,int y,int h,int px,const char*t){(void)f;(void)h;(void)px;(void)t;assert(y==904);}
static void ui_fill_rect_fast(EpdRect box,int c,uint8_t*f){(void)c;(void)f;assert(box.x>=0&&box.y>=0&&box.x+box.width<=684&&box.y+box.height<=1216);}
static void ui_text_caption(uint8_t*f,EpdRect box,int px,const char*t,int a,bool inv){(void)f;(void)px;(void)a;(void)inv;caption=box;snprintf(caption_text,sizeof(caption_text),"%s",t);++caption_calls;}
'''
for name in ('book_ticket_draw','book_lock_signature_draw'):
    unit+=function(name,ROOT/'main/book/book_ticket.c')+'\n'
unit+=r'''
int main(void){
 uint8_t fb=0;
 for(unsigned lines=1;lines<=2;++lines)for(unsigned has_author=0;has_author<2;++has_author){
  head_lines=lines;snprintf(author,sizeof(author),"%s",has_author?"Author":"");
  for(unsigned custom=0;custom<2;++custom){
   snprintf(signature,sizeof(signature),"%s",custom?"Chữ ký riêng":"");
   assert(book_ticket_draw(&fb,false));
   assert(cover.x==150&&cover.width==384&&cover.y+cover.height==844);
   assert(caption.y==1030&&caption.height==88&&caption.x==82&&caption.width==520);
   assert(!strcmp(caption_text,custom?signature:"Lưu giữ dấu ấn thời gian qua từng trang sách"));
  }
 }
 signature[0]=0;unsigned before=caption_calls;book_lock_signature_draw(&fb);assert(caption_calls==before);
 strcpy(signature,"Tên tôi");book_lock_signature_draw(&fb);
 assert(caption_calls==before+1&&!strcmp(caption_text,signature)&&caption.y==1104&&caption.height==64);
 puts("PASS: actual shared Full/Love ticket geometry, one/two-line titles, missing author, default/custom footer and image-lock signature bounds");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(unit)
    for love in (0,1):
        subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',f'-DDPREADER_LOVE_BOOK={love}',str(p/'test.c'),'-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True)
