#!/usr/bin/env python3
"""Exercise real LCD park/resume routing through repeated sleep cycles."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
s=r/'boards/metalio47/src/components/epdiy/src/output_lcd/lcd_driver.c'
u=r'''#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#define ESP_OK 0
#define ESP_GOTO_ON_ERROR(value,label,...) do{if(value)goto label;}while(0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
typedef int esp_err_t;
typedef struct {int bus_width,pixel_clock;}LcdEpdConfig_t;
static struct{int line_bytes,native_height,bb_size;LcdEpdConfig_t config;}lcd;
static bool lcd_buffers_retained;
static int allocations,frees,stops,peripherals;
static void assign_lcd_parameters_from_config(const LcdEpdConfig_t*c,int w,int h){lcd.config=*c;lcd.line_bytes=w/4;lcd.native_height=h;}
static void check_cache_configuration(void){}
static int allocate_lcd_buffers(void){++allocations;return 0;}
static int init_lcd_peripheral(void){++peripherals;return 0;}
static int init_dma_trans_link(void){return 0;}
static int init_bus_gpio(void){return 0;}
static void init_ckv_rmt(void){}
static void epd_lcd_set_pixel_clock_MHz(int n){assert(n==20);}
static void epd_lcd_line_source_cb(void*a,void*b){assert(!a&&!b);}
static void deinit_bus_gpio(void){++stops;}
static void deinit_lcd_peripheral(void){--peripherals;}
static void deinit_dma_trans_link(void){}
static void free_lcd_buffers(void){++frees;}
static void deinit_ckv_rmt(void){}
static void abort(void){assert(0);}
'''
for name in ('epd_lcd_init','lcd_stop','epd_lcd_suspend','epd_lcd_deinit'):u+=function(name,s)+'\n'
u+=r'''int main(void){LcdEpdConfig_t config={8,20000000};
 epd_lcd_init(&config,1216,684);assert(allocations==1&&peripherals==1);
 for(int n=0;n<100;n++){
  epd_lcd_suspend();assert(lcd_buffers_retained&&!frees&&peripherals==0);
  epd_lcd_init(&config,1216,684);assert(!lcd_buffers_retained&&allocations==1&&peripherals==1);
 }
 epd_lcd_deinit();assert(frees==1&&!lcd_buffers_retained&&peripherals==0&&stops==101);
 epd_lcd_init(&config,1216,684);assert(allocations==2);epd_lcd_deinit();assert(frees==2);
 puts("PASS real LCD lifecycle:100 light-sleep cycles reuse buffers, stop peripherals and release allocations only on teardown");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(u)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
