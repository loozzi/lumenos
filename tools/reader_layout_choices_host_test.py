#!/usr/bin/env python3
"""Actual layout taps open current selectors; gaps and Back never edit settings."""
from pathlib import Path
import re,subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
book=ROOT/'main/apps/app_book.c'
text=book.read_text()
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {bool consumed;} app_ctx_t;
enum {APP_REDRAW_NONE,APP_REDRAW_PAGE};
enum {READER_PANEL_NONE,READER_PANEL_FONT_SETTINGS,READER_PANEL_LAYOUT_SETTINGS};
static int s_reader_panel=READER_PANEL_LAYOUT_SETTINGS,selected=-1;
static unsigned opens;
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
static void reader_choice_open(int kind){selected=kind;++opens;}
'''
unit+=re.search(r'enum \{READ_PICK_[^}]+\};',text)[0]+'\n'
unit+=re.search(r'static const int s_layout_choices\[\]=\{[^}]+\};',text)[0]+'\n'
for name in ('layout_choice_row','reader_layout_back_rect'):
    unit+=function(name,book)+'\n'
action=function('reader_panel_action',book)
match=re.search(r'if\s*\(\s*s_reader_panel\s*==\s*READER_PANEL_LAYOUT_SETTINGS\s*\)\s*\{',action)
assert match
at=match.end();depth=1
while depth:
    depth+=(action[at]=='{')-(action[at]=='}');at+=1
unit+='static int layout_tap(app_ctx_t *ctx,uint16_t x,uint16_t y){(void)ctx;\n'+action[match.start():at]+'\nreturn APP_REDRAW_NONE;}\n'
unit+=r'''
int main(void){
 app_ctx_t ctx={0};
 assert(sizeof(s_layout_choices)/sizeof(*s_layout_choices)==8);
 for(int row=0;row<8;++row){
  EpdRect r=layout_choice_row(row);assert(r.x>=0&&r.y>=180&&r.x+r.width<=684&&r.y+r.height<1132);
  for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x){
   selected=-1;unsigned before=opens;s_reader_panel=READER_PANEL_LAYOUT_SETTINGS;
   assert(layout_tap(&ctx,x,y)==APP_REDRAW_PAGE&&selected==s_layout_choices[row]&&opens==before+1);
   assert(s_reader_panel==READER_PANEL_LAYOUT_SETTINGS&&!ctx.consumed);
  }
 }
 EpdRect back=reader_layout_back_rect();
 for(int y=180;y<1216;++y)for(int x=0;x<684;++x){
  bool hit=false;for(int row=0;row<8;++row)hit|=ui_rect_hit(layout_choice_row(row),x,y);
  if(hit)continue;
  unsigned before=opens;s_reader_panel=READER_PANEL_LAYOUT_SETTINGS;
  int result=layout_tap(&ctx,x,y);
  assert(opens==before);
  if(ui_rect_hit(back,x,y))assert(result==APP_REDRAW_PAGE&&s_reader_panel==READER_PANEL_FONT_SETTINGS);
  else assert(result==APP_REDRAW_NONE&&s_reader_panel==READER_PANEL_LAYOUT_SETTINGS);
 }
 s_reader_panel=READER_PANEL_LAYOUT_SETTINGS;
 assert(layout_tap(&ctx,50,100)==APP_REDRAW_PAGE&&s_reader_panel==READER_PANEL_NONE);
 puts("PASS actual reader layout: eight selectors, every hit/gap/boundary, independent fine-indent choice, Back/header dismissal without implicit edits");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
