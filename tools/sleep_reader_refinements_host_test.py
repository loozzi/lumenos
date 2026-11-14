#!/usr/bin/env python3
"""Exercise production sleep policy, bounded log preview and persisted wake choices."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
sleep=ROOT/'boards/metalio47/src/main/sleep.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG -1
#define NVS_READWRITE 1
#define NVS_NS "test"
typedef int nvs_handle_t;
static uint8_t s_wake_buttons,buttons,presses=1,stored;
static int store_error;
static int backup_nvs_open(const char*n,int m,int*h){(void)n;(void)m;*h=1;return store_error;}
static int backup_nvs_set_u8(int h,const char*k,uint8_t v){(void)h;assert(!strcmp(k,"wake_buttons"));stored=v;return store_error;}
static int settings_backup_commit(int h){(void)h;return store_error;}
static void nvs_close(int h){(void)h;}
static int64_t now;
static uint8_t app_settings_wake_buttons(void){return buttons;}
static uint8_t app_settings_wake_presses(void){return presses;}
enum { APP_WAKE_BUTTON_AI=1,APP_WAKE_BUTTON_VOLUME=2,METALIO_AI_NONE=0,METALIO_AI_SHORT=1,METALIO_AI_LONG=2 };
static bool s_lock_aux_wake_pending,s_lock_keys_restore_pending,power;
static int ai,volume;
static int metalio_take_ai(void){int v=ai;ai=0;return v;}
static int metalio_take_volume(void){int v=volume;volume=0;return v;}
static bool read_pico_pmu_take_key_wakeup_single(void){bool v=power;power=false;return v;}
static bool read_pico_pmu_take_key_wakeup(void){return read_pico_pmu_take_key_wakeup_single();}
static int64_t esp_timer_get_time(void){return now;}
#define METALIO_AI_KEY_GPIO 61
#define pdMS_TO_TICKS(x) (x)
static int ai_level=1,read_count,fail_read;
static uint16_t inputs[2]={0xffff,0xffff};
static int gpio_get_level(int pin){assert(pin==61);return ai_level;}
static void vTaskDelay(int ms){now+=ms*1000;}
static int metalio_io_read(uint16_t *v){int n=read_count++;*v=inputs[n%2];return read_count==fail_read?-1:ESP_OK;}
static bool busy,font_busy,usb,wifi_ok=true,ble_ok=true,jobs_ok=true;
static unsigned cancels,font_cancels,wifi_calls,ble_calls,resumes;
static void pico_online_request_cancel(void){++cancels;}
static bool online_font_request_cancel(void){++font_cancels;return !font_busy;}
static bool pico_online_busy(void){return busy;}
static bool usb_storage_active(void){return usb;}
static bool read_pico_transfer_pause_for_sleep(void){++wifi_calls;return wifi_ok;}
static int ble_pt_suspend_for_sleep(void){++ble_calls;return ble_ok?0:-1;}
static bool book_cover_jobs_quiesce(unsigned ms){assert(ms==2000);return jobs_ok;}
static bool ui_avatar_quiesce(unsigned ms){assert(ms==2000);return jobs_ok;}
static void ble_pt_resume_after_sleep(void){++resumes;}
static void read_pico_transfer_resume_after_sleep(void){++resumes;}
'''
for name in ('lock_take_key_wakeup','lock_confirm_aux_wake','lock_light_sleep_slice_us','app_lock_prepare_resources'):
    unit+=function(name,sleep)+'\n'
unit+=function('app_settings_set_wake_buttons',ROOT/'firmware/shared/main/settings.c')+'\n'
unit+=r'''
typedef struct {int x,y,width,height;} EpdRect;
#define EPD_DRAW_ALIGN_LEFT 0
static int ui_text_effective_px(int px){return px;}
static int ui_text_fixed_width_px(int px,const char*s){(void)px;(void)s;return 0;}
static unsigned painted;
static void ui_text_box(uint8_t*f,EpdRect r,int px,const char*s,int align,bool inv){
 (void)f;(void)px;(void)align;(void)inv;
 assert(r.width==580&&strlen(s)<512);++painted;
}
'''
unit+=function('settings_log_preview',ROOT/'firmware/shared/main/apps/app_device_settings.c')+'\n'
unit+=r'''
typedef struct {void *hl;} app_ctx_t;
typedef struct {void (*on_before_lock)(app_ctx_t *);} app_desc_t;
typedef struct {uint8_t tab,reader,fullscreen;char path[288];} pico_resume_t;
#define DPREADER_BOARD_METALIO47 1
static unsigned feedbacks,checkpoints,before_locks;
static void app_lock_feedback(void *hl){(void)hl;++feedbacks;}
static void before_lock(app_ctx_t *ctx){(void)ctx;++before_locks;}
static int app_index_of(const app_desc_t *app){(void)app;return 1;}
static bool app_book_resume_context(char *path,size_t n,bool *fullscreen){snprintf(path,n,"book.epub");*fullscreen=false;return true;}
static int pico_boot_save_resume(const pico_resume_t *p){assert(p->reader&&p->tab==1&&!strcmp(p->path,"book.epub"));++checkpoints;return ESP_OK;}
static void app_lock_set_resume_ready(bool ready){assert(ready);}

typedef struct {bool present,mounted;} read_pico_sd_info_t;
static bool s_books_cache_valid,s_cached_sd_present,s_cached_sd_mounted,s_scan_pending;
static unsigned s_cached_store_revision=5,revision=5,avatar_prepares;
static char s_cached_books_dir[40]="/sdcard/books";
static read_pico_sd_info_t media={true,true};
static int media_error;
static int read_pico_sd_get_info(read_pico_sd_info_t *out){*out=media;return media_error;}
static unsigned book_store_revision(void){return revision;}
static const char *app_settings_books_dir(void){return "/sdcard/books";}
static void avatar_prepare_state(void){++avatar_prepares;}
'''
unit+=function('on_media_ready',ROOT/'firmware/shared/main/apps/app_dashboard.c')+'\n'
unit+=function('prepare_lock',ROOT/'firmware/shared/main/app/app_loop.c')+'\n'

unit+=r'''
int main(void){
 for(buttons=0;buttons<4;++buttons)for(presses=1;presses<=2;++presses){
  ai=1;volume=0;assert(lock_take_key_wakeup()==!!(buttons&1));
  volume=1;assert(lock_take_key_wakeup()==!!(buttons&2));
  power=true;assert(lock_take_key_wakeup());assert(!lock_take_key_wakeup());
 }
 buttons=0;s_lock_aux_wake_pending=true;s_lock_keys_restore_pending=true;assert(!lock_take_key_wakeup());s_lock_keys_restore_pending=false;assert(lock_take_key_wakeup());assert(!lock_take_key_wakeup());
 buttons=2;inputs[0]=inputs[1]=0xfeff;read_count=0;assert(lock_confirm_aux_wake());
 inputs[1]=0xffff;read_count=0;assert(!lock_confirm_aux_wake());
 inputs[1]=0xfdff;read_count=0;assert(!lock_confirm_aux_wake());
 inputs[1]=0xfeff;read_count=0;fail_read=2;assert(!lock_confirm_aux_wake());fail_read=0;
 buttons=1;ai_level=0;assert(lock_confirm_aux_wake());buttons=0;assert(!lock_confirm_aux_wake());
 now=100;assert(lock_light_sleep_slice_us(0)==0);assert(lock_light_sleep_slice_us(50)==1);
 assert(lock_light_sleep_slice_us(150)==50);assert(lock_light_sleep_slice_us(100000000)==60000000);
 busy=true;assert(!app_lock_prepare_resources());assert(cancels==1&&font_cancels==1&&!wifi_calls);
 busy=false;font_busy=true;assert(!app_lock_prepare_resources());font_busy=false;
 usb=true;assert(!app_lock_prepare_resources());usb=false;
 wifi_ok=false;assert(!app_lock_prepare_resources()&&resumes==2);
 wifi_ok=true;ble_ok=false;assert(!app_lock_prepare_resources()&&resumes==4);
 ble_ok=true;jobs_ok=false;assert(!app_lock_prepare_resources()&&resumes==6);
 jobs_ok=true;assert(app_lock_prepare_resources()&&resumes==6&&wifi_calls==4&&ble_calls==4);
 for(unsigned n=0;n<4;++n){assert(app_settings_set_wake_buttons(n)==0&&s_wake_buttons==n&&stored==n);}
 assert(app_settings_set_wake_buttons(4)==ESP_ERR_INVALID_ARG&&s_wake_buttons==3);
 store_error=-5;assert(app_settings_set_wake_buttons(0)==-5&&s_wake_buttons==3);
 app_ctx_t ctx={0};app_desc_t app={before_lock};
 busy=true;assert(!prepare_lock(&ctx,&app,true)&&!feedbacks&&!checkpoints&&!before_locks);
 busy=false;assert(prepare_lock(&ctx,&app,true)&&feedbacks==1&&checkpoints==1&&before_locks==1);
 s_books_cache_valid=s_cached_sd_present=s_cached_sd_mounted=true;
 on_media_ready(&ctx);on_media_ready(&ctx);assert(!s_scan_pending&&!avatar_prepares&&s_books_cache_valid);
 ++revision;on_media_ready(&ctx);assert(s_scan_pending&&!s_books_cache_valid&&avatar_prepares==1);
 s_scan_pending=false;s_books_cache_valid=true;media_error=-1;on_media_ready(&ctx);assert(s_scan_pending);
 char text[4096];uint8_t fb=0;
 for(unsigned n=500;n<520;++n){memset(text,'x',n);text[n]=0;settings_log_preview(&fb,text);}
 memset(text,'x',sizeof(text)-1);text[sizeof(text)-1]=0;settings_log_preview(&fb,text);
 strcpy(text,"Tiếng Việt\n漢字\r\n");settings_log_preview(&fb,text);assert(painted);
 puts("PASS: wake selection/debounce/IO faults, sleep deadline, resource refusal, NVS failure and ASan log-preview bounds");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
