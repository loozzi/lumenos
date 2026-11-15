#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：用实际源码验证横屏面板视窗的像素搬运、滚动范围与触摸换算；不接触屏幕或串口。
English: Verify the landscape panel viewport pixel move, scroll range and touch conversion from the
actual source; no display or serial access."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
book = root / 'main/apps/app_book.c'
if 'static void reader_viewport_blit(' not in book.read_text():
    print('SKIP: landscape panel viewport is not used on this board')
    raise SystemExit(0)
pieces = '\n'.join(function(name, book) for name in
                   ('reader_viewport_window', 'reader_viewport_blit', 'reader_viewport_point'))
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define READER_VIEWPORT_W 684
#define READER_VIEWPORT_PORTRAIT_H 1216
#define PW 1216
#define PH 684
static int epd_width(void){return PW;}
static int epd_height(void){return PH;}
static uint8_t *s_viewport_fb;
static int s_viewport_ox, s_viewport_win, s_viewport_scroll;
''' + pieces + r'''
static uint8_t scratch[PW*PH/2], frame[PW*PH/2];
static void put(uint8_t *fb,int px,int py,unsigned v){int i=py*PW+px,sh=(i&1)*4;fb[i/2]=(uint8_t)((fb[i/2]&~(15u<<sh))|(v<<sh));}
static unsigned get(const uint8_t *fb,int px,int py){int i=py*PW+px;return (fb[i/2]>>((i&1)*4))&15;}
// 竖屏逻辑(x,y)在倒置竖屏下的物理位置。/ Physical position of portrait (x,y) under inverted portrait.
static void portrait_put(int x,int y,unsigned v){put(scratch,y,PH-1-x,v);}
int main(void){
    // 短面板贴底；长面板从顶边开始并可滚到底。/ Short sheets sit at the bottom; tall sheets start at the top and scroll to the end.
    s_viewport_scroll=0; assert(reader_viewport_window(700,64)==532 && s_viewport_scroll==0);
    s_viewport_scroll=0; assert(reader_viewport_window(180,0)==180);
    s_viewport_scroll=999; assert(reader_viewport_window(180,0)==532 && s_viewport_scroll==352);
    s_viewport_scroll=-5; assert(reader_viewport_window(180,0)==180 && s_viewport_scroll==0);
    // 面板顶边放在状态栏下：636高面板起于状态栏下方，可再滚16像素到底；整页内容从0开始。
    // The sheet top sits below the status bar: a 636px sheet starts under it and scrolls 16px to its end; full pages start at 0.
    s_viewport_scroll=0; assert(reader_viewport_window(580,64)==516);
    s_viewport_scroll=999; assert(reader_viewport_window(580,64)==532 && s_viewport_scroll==16);
    s_viewport_scroll=0; assert(reader_viewport_window(0,64)==0);
    // 搬运：面板像素落在右侧视窗，顶边以上和视窗左侧保留正文。
    // Move: sheet pixels land in the right viewport; rows above the top and the left side keep the body.
    s_viewport_fb=scratch; s_viewport_ox=PW-READER_VIEWPORT_W;
    memset(frame,0x77,sizeof(frame)); memset(scratch,0xff,sizeof(scratch));
    const int top=600, win=532;
    portrait_put(0,top,1); portrait_put(683,1215,2); portrait_put(10,top-1,3); portrait_put(341,900,4);
    reader_viewport_blit(frame,top,win);
    assert(get(frame,s_viewport_ox+0,top-win)==1);
    assert(get(frame,s_viewport_ox+683,1215-win)==2);
    assert(get(frame,s_viewport_ox+341,900-win)==4);
    assert(get(frame,s_viewport_ox+10,top-1-win)==7);       // above sheet top: body kept
    assert(get(frame,s_viewport_ox-1,300)==7);              // left of viewport: body kept
    assert(get(frame,s_viewport_ox+5,top-win+5)==15);       // sheet background copied
    // 触摸：视窗内换算回竖屏；起点在左侧视为面板外；拖动点夹到左缘；按键区不变。
    // Touch: convert inside the viewport; a left origin is outside the sheet; drags clamp; the key area stays raw.
    s_viewport_win=win; uint16_t x=s_viewport_ox+100,y=50;
    reader_viewport_point(&x,&y,true); assert(x==100&&y==50+win);
    x=10;y=200; reader_viewport_point(&x,&y,true); assert(x==0&&y==0);
    x=10;y=200; reader_viewport_point(&x,&y,false); assert(x==0&&y==200+win);
    x=300;y=1250; reader_viewport_point(&x,&y,true); assert(x==300&&y==1250);
    puts("PASS: landscape panel viewport move, top/left body retention, scroll clamp and touch conversion");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
    path = Path(folder)
    (path / 'test.c').write_text(unit)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
