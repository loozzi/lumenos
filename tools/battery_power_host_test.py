#!/usr/bin/env python3
"""Exercise the real low-battery guard, light cap, static-wait planner and page idle deadlines.

中文：用真实源码（纯逻辑头文件与从页面源码抽取的函数）验证低电量分级、关机确认、
前光上限、续航估算、静态等待上限，以及主页/文件/设置/选书页的空闲声明。
English: real sources (pure header plus functions extracted from the pages) verify
levels, shutdown confirmation, the light cap, runtime estimate, static wait cap and
the idle declarations of Home/Files/Settings/the picker.
"""
from pathlib import Path
import re
import subprocess
import tempfile
from home_cover_cache_host_test import function

ROOT = Path(__file__).resolve().parents[1]
CFLAGS = ['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined,address',
          '-fno-sanitize-recover=all']


def run(name, unit, includes=(), headers=None):
    with tempfile.TemporaryDirectory() as d:
        p = Path(d)
        for header, text in (headers or {}).items():
            (p / header).write_text(text)
        (p / 'test.c').write_text(unit)
        cmd = CFLAGS + [f'-I{p}'] + [f'-I{i}' for i in includes] + [str(p / 'test.c'), '-o', str(p / 'test')]
        try:
            subprocess.run(cmd, check=True)
        except subprocess.CalledProcessError:
            # Some hosts lack ASan runtime; fall back to UBSan only, never skip the test.
            cmd = [c for c in cmd if not c.startswith('-fsanitize')] + ['-fsanitize=undefined']
            subprocess.run(cmd, check=True)
        subprocess.run([str(p / 'test')], check=True)
    print('PASS', name)


LOGIC = r'''
#include <assert.h>
#include <stdio.h>
#include "battery_guard_logic.h"
#include "app_wait.h"
static battery_guard_state_t s;
static battery_level_t at(int pct,bool chg,int64_t now,int64_t awake){return battery_guard_update(&s,pct,chg,now,awake);}
int main(void){
 battery_guard_reset(&s);
 assert(at(50,false,1000,0)==BATTERY_LEVEL_NORMAL);
 assert(at(16,false,2000,0)==BATTERY_LEVEL_NORMAL);
 assert(at(15,false,3000,0)==BATTERY_LEVEL_WARN);
 // Hysteresis: 16 stays warn, 17 leaves.
 assert(at(16,false,4000,0)==BATTERY_LEVEL_WARN);
 assert(at(17,false,5000,0)==BATTERY_LEVEL_NORMAL);
 assert(at(15,false,6000,0)==BATTERY_LEVEL_WARN);
 assert(at(8,false,7000,0)==BATTERY_LEVEL_SAVER);
 assert(at(9,false,8000,0)==BATTERY_LEVEL_SAVER);
 assert(at(10,false,9000,0)==BATTERY_LEVEL_WARN);
 assert(at(8,false,10000,0)==BATTERY_LEVEL_SAVER);
 // Critical needs a sustained 20 s window before shutdown.
 assert(at(3,false,20000,0)==BATTERY_LEVEL_CRITICAL && !s.shutdown_due);
 assert(at(3,false,39999,0)==BATTERY_LEVEL_CRITICAL && !s.shutdown_due);
 assert(at(2,false,40000,0)==BATTERY_LEVEL_CRITICAL && s.shutdown_due);
 // A transient recovery cancels the confirmation and restarts it.
 assert(at(4,false,41000,0)==BATTERY_LEVEL_SAVER && !s.shutdown_due && !s.critical_since_ms);
 assert(at(3,false,42000,0)==BATTERY_LEVEL_CRITICAL && !s.shutdown_due);
 // Invalid data never escalates and cancels critical.
 assert(at(-1,false,70000,0)==BATTERY_LEVEL_SAVER && !s.shutdown_due);
 assert(at(3,false,71000,0)==BATTERY_LEVEL_CRITICAL && !s.shutdown_due);
 assert(at(101,false,95000,0)==BATTERY_LEVEL_SAVER && !s.shutdown_due);
 // Charging clears every level and request immediately, even at 0%.
 assert(at(0,false,100000,0)==BATTERY_LEVEL_CRITICAL);
 assert(at(0,false,130000,0)==BATTERY_LEVEL_CRITICAL && s.shutdown_due);
 assert(at(0,true,131000,0)==BATTERY_LEVEL_NORMAL && !s.shutdown_due);
 // Runtime estimate: needs >=3 points and >=20 min awake.
 battery_guard_reset(&s);
 const int64_t MIN=60000;
 at(80,false,1,0);assert(battery_guard_remaining_minutes_from(&s,80,false)==-1);
 at(78,false,2,30*MIN);assert(battery_guard_remaining_minutes_from(&s,78,false)==-1);
 at(77,false,3,10*MIN);assert(!s.estimate_valid); // 3 points but only 10 min awake
 at(76,false,4,40*MIN);assert(s.estimate_valid);
 // 4 points in 40 min = 10 min per point; 76% -> 760 minutes.
 assert(battery_guard_remaining_minutes_from(&s,76,false)==760);
 assert(battery_guard_remaining_minutes_from(&s,76,true)==-1);
 assert(battery_guard_remaining_minutes_from(&s,-1,false)==-1);
 // A gauge jump upward resets the baseline but keeps the last rate.
 at(90,false,5,41*MIN);assert(s.base_pct==90 && s.estimate_valid);
 // Update gate.
 assert(battery_guard_update_allowed(30,false)&&!battery_guard_update_allowed(29,false));
 assert(battery_guard_update_allowed(5,true)&&battery_guard_update_allowed(-1,false));
 // Static wait cap: 1 s ceiling, legacy unchanged, pending zero, expired zero.
 int64_t far[]={100000};
 assert(app_wait_ms_capped(1000,40,1,0,far,1,APP_WAIT_STATIC_CAP_MS)==1000);
 assert(app_wait_ms(1000,40,1,0,far,1)==200);
 assert(app_wait_ms_capped(1000,40,0,0,far,1,APP_WAIT_STATIC_CAP_MS)==40);
 assert(app_wait_ms_capped(1000,40,1,1,far,1,APP_WAIT_STATIC_CAP_MS)==0);
 int64_t near[]={100000,1350,999};
 assert(app_wait_ms_capped(1000,40,1,0,near,3,APP_WAIT_STATIC_CAP_MS)==0);
 int64_t mid[]={100000,1350};
 assert(app_wait_ms_capped(1000,40,1,0,mid,2,APP_WAIT_STATIC_CAP_MS)==350);
 puts("logic ok");
 return 0;
}
'''

RUNTIME_HEADERS = {
    'dpreader_board_config.h': '#pragma once\n#define DPREADER_BOARD_METALIO47 1\n',
    'read_pico_pmu.h': '#pragma once\n#include <stdbool.h>\ntypedef struct { bool present; } pmu_snapshot_t;\n'
        'const pmu_snapshot_t *read_pico_pmu_get(void);\nint pmu_battery_percent(const pmu_snapshot_t*p);\n'
        'bool pmu_battery_charging(const pmu_snapshot_t*p);\n',
    'metalio_hw.h': '#pragma once\nvoid metalio_frontlight_set_cap(unsigned c);\n',
}
RUNTIME = r'''
#include <assert.h>
#include <stdio.h>
#include "dpreader_board_config.h"
#include "read_pico_pmu.h"
#include "metalio_hw.h"
#include "battery_guard.h"
''' + re.sub(r'^#include "[^"]+"\n', '', (ROOT / 'main/battery_guard.c').read_text(), flags=re.M) + r'''
static pmu_snapshot_t snap={true};static int fake_pct=50;static bool fake_chg;
const pmu_snapshot_t *read_pico_pmu_get(void){return &snap;}
int pmu_battery_percent(const pmu_snapshot_t*p){(void)p;return fake_pct;}
bool pmu_battery_charging(const pmu_snapshot_t*p){(void)p;return fake_chg;}
static unsigned cap_calls,last_cap=100;
void metalio_frontlight_set_cap(unsigned c){++cap_calls;last_cap=c;}
int main(void){
 for(int i=0;i<5;i++)battery_guard_poll(1000+i,0);
 assert(cap_calls==0&&battery_guard_level()==BATTERY_LEVEL_NORMAL&&!battery_guard_auto_lock_cap()&&!battery_guard_lock_off_cap_ms());
 fake_pct=12;battery_guard_poll(2000,0);assert(battery_guard_level()==BATTERY_LEVEL_WARN&&cap_calls==0);
 fake_pct=7;for(int i=0;i<10;i++)battery_guard_poll(3000+i,0);
 assert(cap_calls==1&&last_cap==BATTERY_GUARD_SAVER_LIGHT_CAP); // no LEDC rewrite per pass
 assert(battery_guard_auto_lock_cap()==1&&battery_guard_lock_off_cap_ms()==600000);
 assert(!battery_guard_update_ok());
 fake_pct=2;battery_guard_poll(10000,0);assert(!battery_guard_shutdown_due(10000)&&battery_guard_lock_off_cap_ms()==5000);
 battery_guard_poll(30000,0);assert(battery_guard_shutdown_due(30000));
 battery_guard_shutdown_deferred(30000);assert(!battery_guard_shutdown_due(89999)&&battery_guard_shutdown_due(90000));
 fake_chg=true;battery_guard_poll(91000,0);
 assert(battery_guard_level()==BATTERY_LEVEL_NORMAL&&!battery_guard_shutdown_due(200000)&&cap_calls==2&&last_cap==100);
 assert(battery_guard_update_ok()&&battery_guard_remaining_minutes()==-1);
 // 1.5.19 一次性提示：提示/省电各一次，充电后重新启用。/ One-time notices: warning and saver once each, re-armed by charging.
 fake_chg=false;fake_pct=50;battery_guard_poll(100000,0);assert(battery_guard_pending_notice()==BATTERY_LEVEL_NORMAL);
 fake_pct=14;battery_guard_poll(101000,0);assert(battery_guard_pending_notice()==BATTERY_LEVEL_WARN);
 battery_guard_poll(102000,0);assert(battery_guard_pending_notice()==BATTERY_LEVEL_WARN); // stays pending until shown
 battery_guard_ack_notice(BATTERY_LEVEL_WARN);assert(battery_guard_pending_notice()==BATTERY_LEVEL_NORMAL);
 fake_pct=13;battery_guard_poll(103000,0);assert(battery_guard_pending_notice()==BATTERY_LEVEL_NORMAL);
 fake_pct=8;battery_guard_poll(104000,0);assert(battery_guard_pending_notice()==BATTERY_LEVEL_SAVER);
 battery_guard_ack_notice(BATTERY_LEVEL_SAVER);fake_pct=2;battery_guard_poll(105000,0);
 assert(battery_guard_pending_notice()==BATTERY_LEVEL_NORMAL); // critical uses the shutdown frame, not a toast
 fake_chg=true;battery_guard_poll(106000,0);fake_chg=false;fake_pct=14;battery_guard_poll(107000,0);
 assert(battery_guard_pending_notice()==BATTERY_LEVEL_WARN); // a new discharge re-arms
 // Skipping straight to saver announces saver once (not warning afterwards).
 fake_chg=true;battery_guard_poll(108000,0);fake_chg=false;fake_pct=7;battery_guard_poll(109000,0);
 assert(battery_guard_pending_notice()==BATTERY_LEVEL_SAVER);battery_guard_ack_notice(BATTERY_LEVEL_SAVER);
 fake_pct=12;battery_guard_poll(110000,0);assert(battery_guard_level()==BATTERY_LEVEL_WARN&&battery_guard_pending_notice()==BATTERY_LEVEL_NORMAL);
 puts("runtime ok");return 0;
}
'''

LIGHT = r'''
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#define LEDC_LOW_SPEED_MODE 0
#define LEDC_CHANNEL_0 0
#define LEDC_CHANNEL_1 1
static unsigned duty[2],stops,applies;
static void ledc_stop(int m,int c,int l){(void)m;(void)c;(void)l;++stops;}
static void ledc_set_duty(int m,int c,unsigned d){(void)m;duty[c]=d;}
static void ledc_update_duty(int m,int c){(void)m;(void)c;}
static void metalio_light_schedule_invalidate(void){}
static unsigned lamp_notes,last_lamp=999;
static int64_t esp_timer_get_time(void){return 0;}
static void metalio_power_note_lamp(unsigned b,int64_t now){(void)now;++lamp_notes;last_lamp=b;}
static bool fl_ready=true,light_suspended,light_enabled=true;
static unsigned brightness_position=80,warmth_position=0;
static void frontlight_apply_position(unsigned b,unsigned w);
static unsigned s_brightness_cap=100;
''' + function('metalio_frontlight', ROOT / 'components/metalio_hw/metalio_hw.c') + r'''
static void frontlight_apply_position(unsigned b,unsigned w){++applies;metalio_frontlight(b,w);}
''' + function('metalio_frontlight_set_cap', ROOT / 'components/metalio_hw/metalio_hw.c') + r'''
int main(void){
 metalio_frontlight(80,0);assert(duty[0]==80*1023/200);
 metalio_frontlight_set_cap(40);assert(applies==1&&duty[0]==40*1023/200);
 metalio_frontlight(90,0);assert(duty[0]==40*1023/200&&last_lamp==40); // counters record the capped output
 metalio_frontlight(20,0);assert(duty[0]==20*1023/200);
 metalio_frontlight_set_cap(40);assert(applies==1); // unchanged cap does nothing
 light_suspended=true;metalio_frontlight_set_cap(100);assert(applies==1); // locked: no relight
 light_suspended=false;metalio_frontlight(90,0);assert(duty[0]==90*1023/200);
 metalio_frontlight_set_cap(0);metalio_frontlight(90,0);assert(duty[0]==1*1023/200); // cap floor stays lit
 metalio_frontlight(0,0);assert(stops==2&&light_suspended&&last_lamp==0&&lamp_notes>0); // off path untouched, lamp counted off
 puts("light ok");return 0;
}
'''

PAGES = r'''
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
typedef struct { int64_t now_ms; } app_ctx_t;
static long fake_sec=1800000030; static long fake_usec=250000;
static int fake_gettimeofday(struct timeval*t,void*z){(void)z;t->tv_sec=fake_sec;t->tv_usec=fake_usec;return 0;}
#define gettimeofday fake_gettimeofday
#define DPREADER_LOVE_BOOK 0
/* Home */
enum { HOME_ROOT,HOME_COMPONENT,HOME_GOAL,HOME_TIME,HOME_UTILITIES,HOME_UTILITY_PICK,HOME_ADD_BOOKS,HOME_POP_MENU };
static int s_view=HOME_ROOT;
static bool s_scan_pending,s_cover_waiting,s_component_pending,s_clock_pending,s_press_pending,s_pop_menu_pending,
 s_time_pending,s_widget_pending,s_home_force_full_once,s_avatar_pending,motion;
static int s_cover_next=1; static char s_utility_notice[8]; static int64_t s_utility_notice_until;
static bool visible_motion_clock(void){return motion;}
''' + function('home_next_tick_ms', ROOT / 'main/apps/app_dashboard.c') + r'''
/* Files */
enum { FILE_VIEW_LIST, FILE_VIEW_RENAME };
static int f_view=FILE_VIEW_LIST; static bool f_scan; static int f_folder=-1; static int64_t f_space_poll;
static struct { bool mounted, space_valid; } f_sd={true,true};
#define s_view f_view
#define s_scan_pending f_scan
#define s_folder f_folder
#define s_space_poll_ms f_space_poll
#define s_sd f_sd
''' + function('files_next_tick_ms', ROOT / 'main/apps/app_files.c') + r'''
#undef s_view
#undef s_scan_pending
#undef s_folder
#undef s_space_poll_ms
#undef s_sd
/* Settings */
enum { SETTINGS_MAIN, SETTINGS_OTHER };
static int s_page=SETTINGS_MAIN;
static bool s_panel_repair_pending,s_boot_pending,s_upgrade_restart,s_reset_pending,s_scroll_present_pending,s_local_pending;
#define s_avatar_pending set_avatar
static bool set_avatar;
''' + function('settings_next_tick_ms', ROOT / 'main/apps/app_device_settings.c') + r'''
#undef s_avatar_pending
/* Picker */
static bool s_pending;
''' + function('next_tick', ROOT / 'firmware/love_book/apps/app_book_picker.c') + r'''
int main(void){
 app_ctx_t c={50000};
 // 30.25 s into the minute: the next minute is 29.75 s away, so the 1 s cap wins.
 assert(home_next_tick_ms(&c)==51000);
 fake_sec=1800000059; fake_usec=600000; // 59.6 s: boundary in 400 ms (+5 guard)
 assert(home_next_tick_ms(&c)==50405);
 fake_usec=250000; fake_sec=1800000030;
 bool *gates[]={&s_scan_pending,&s_cover_waiting,&s_component_pending,&s_clock_pending,&s_press_pending,&s_pop_menu_pending,
  &s_time_pending,&s_widget_pending,&s_home_force_full_once,&s_avatar_pending,&motion};
 for(unsigned i=0;i<sizeof(gates)/sizeof(*gates);i++){*gates[i]=true;assert(!home_next_tick_ms(&c));*gates[i]=false;}
 s_cover_next=0;assert(!home_next_tick_ms(&c));s_cover_next=1;
 for(int v=HOME_COMPONENT;v<=HOME_POP_MENU;v++){s_view=v;assert(!home_next_tick_ms(&c));}s_view=HOME_ROOT;
 s_utility_notice[0]='x';s_utility_notice_until=50300;assert(home_next_tick_ms(&c)==50300);
 s_utility_notice_until=40000;assert(home_next_tick_ms(&c)==50000);s_utility_notice[0]=0;
 fake_sec=100;assert(home_next_tick_ms(&c)==51000); // invalid wall clock: no minute alignment
 // Files
 f_space_poll=40000;assert(files_next_tick_ms(&c)==51000);
 f_space_poll=35500;assert(files_next_tick_ms(&c)==50500);
 f_space_poll=30000;assert(files_next_tick_ms(&c)==50000);
 f_sd.space_valid=false;assert(!files_next_tick_ms(&c));f_sd.space_valid=true;
 f_folder=2;assert(files_next_tick_ms(&c)==51000);f_folder=-1;
 f_scan=true;assert(!files_next_tick_ms(&c));f_scan=false;
 f_view=FILE_VIEW_RENAME;assert(!files_next_tick_ms(&c));f_view=FILE_VIEW_LIST;
 // Settings
 assert(settings_next_tick_ms(&c)==51000);
 bool *sg[]={&s_panel_repair_pending,&s_boot_pending,&s_upgrade_restart,&s_reset_pending,&s_scroll_present_pending,&s_local_pending,&set_avatar};
 for(unsigned i=0;i<sizeof(sg)/sizeof(*sg);i++){*sg[i]=true;assert(!settings_next_tick_ms(&c));*sg[i]=false;}
 s_page=SETTINGS_OTHER;assert(!settings_next_tick_ms(&c));
 // Picker
 assert(next_tick(&c)==51000);s_pending=true;assert(!next_tick(&c));
 puts("pages ok");return 0;
}
'''

SHELF = r'''
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
typedef struct { int64_t now_ms; int leaf; } app_ctx_t;
static long fake_sec=1800000010,fake_usec=0;
static int fake_gettimeofday(struct timeval*t,void*z){(void)z;t->tv_sec=fake_sec;t->tv_usec=fake_usec;return 0;}
#define gettimeofday fake_gettimeofday
#define DPREADER_LOVE_BOOK 0
enum{SHELF,READING};
static int s_view=SHELF,lock_style=0,s_cover_prefetch_leaf=2,s_cover_prefetch_step=8;
static bool simple,opds,notes,s_scan_pending,s_requested_home_sync,s_fonts_reload_pending,s_clear_confirm,s_batch_confirm,
 s_delete_confirm,s_category_menu,s_shelf_key_mode,s_shelf_key_paint_pending,s_shelf_feedback_pending,s_cover_batch_present_pending;
static unsigned s_cover_pending_mask,s_cover_decode_mask,s_cover_warm_mask;
static char s_requested_open[8];
static bool app_settings_simple_ui(void){return simple;}
static int app_settings_lock_style(void){return lock_style;}
static bool opds_session_active(void){return opds;}
static bool book_notes_active(void){return notes;}
static int shelf_rows(void){return 4;}
''' + function('shelf_next_tick_ms', ROOT / 'main/apps/app_book.c') + r'''
int main(void){
 app_ctx_t c={50000,2};
 assert(shelf_next_tick_ms(&c)==51000); // 10 s into the minute: the 1 s cap wins
 fake_sec=1800000059;fake_usec=800000;assert(shelf_next_tick_ms(&c)==50205); // 59.8 s: minute boundary +5 ms
 fake_sec=1800000010;fake_usec=0;
 bool *gates[]={&simple,&opds,&notes,&s_scan_pending,&s_requested_home_sync,&s_fonts_reload_pending,&s_clear_confirm,&s_batch_confirm,
  &s_delete_confirm,&s_category_menu,&s_shelf_key_mode,&s_shelf_key_paint_pending,&s_shelf_feedback_pending,&s_cover_batch_present_pending};
 for(unsigned i=0;i<sizeof(gates)/sizeof(*gates);i++){*gates[i]=true;assert(!shelf_next_tick_ms(&c));*gates[i]=false;}
 unsigned *masks[]={&s_cover_pending_mask,&s_cover_decode_mask};
 for(unsigned i=0;i<2;i++){*masks[i]=1;assert(!shelf_next_tick_ms(&c));*masks[i]=0;}
 s_requested_open[0]='x';assert(!shelf_next_tick_ms(&c));s_requested_open[0]=0;
 // Lock-cover warm-up only matters for the collage lock style.
 s_cover_warm_mask=3;assert(shelf_next_tick_ms(&c));lock_style=2;assert(!shelf_next_tick_ms(&c));s_cover_warm_mask=0;assert(shelf_next_tick_ms(&c));lock_style=0;
 // Prefetch must have finished on the visible leaf (rows*2 steps).
 s_cover_prefetch_step=7;assert(!shelf_next_tick_ms(&c));s_cover_prefetch_step=8;
 c.leaf=3;assert(!shelf_next_tick_ms(&c));c.leaf=2;
 s_view=READING;assert(!shelf_next_tick_ms(&c));s_view=SHELF;
 fake_sec=5;assert(shelf_next_tick_ms(&c)==51000); // invalid wall clock: no minute alignment
 puts("shelf ok");return 0;
}
'''


if __name__ == '__main__':
    inc = [ROOT / 'main', ROOT / 'main/app']
    run('battery guard levels/hysteresis/critical confirmation/charging/estimate/update gate + static wait cap', LOGIC, inc)
    run('battery guard runtime: cap transitions only, auto-lock and lock power-off caps, shutdown backoff, charging release, one-time warning/saver notices', RUNTIME, inc, RUNTIME_HEADERS)
    run('front-light output cap: clamps without changing positions, no relight while locked, off path unchanged', LIGHT, inc)
    run('page idle declarations: Home/Files/Settings/picker veto pending work and align to minute/space deadlines', PAGES, inc)
    run('shelf idle declaration: scans/covers/prefetch/warm-up/key mode/menus/confirmations/OPDS/notes veto; minute-aligned deadline', SHELF, inc)
