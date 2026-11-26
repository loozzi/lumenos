#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""Run the real home-entry cache decision against changing last-read paths."""
from pathlib import Path
import re
import subprocess
import tempfile

SOURCE = Path(__file__).resolve().parents[1] / "main/apps/app_dashboard.c"


def function(name):
    from home_cover_cache_host_test import function as extract
    return extract(name, SOURCE)


UNIT = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define DPREADER_BOARD_METALIO47 1
#define DPREADER_LOVE_BOOK 0
#define ESP_LOGI(tag,fmt,...) ((void)(tag),(void)(fmt),(void)(__VA_ARGS__))
typedef struct {int x,y,width,height;} EpdRect;
enum {HOME_ROOT,APP_HOME_FOLDER};
#define BOOK_STORE_PATH_MAX 288
#define ESP_ERR_NOT_FINISHED 7
typedef struct { char path[BOOK_STORE_PATH_MAX]; } home_book_t;
typedef struct { bool present, mounted; } read_pico_sd_info_t;
typedef struct { int unused; } app_ctx_t;
typedef int esp_err_t;
static home_book_t s_current, s_recent[6];
static bool s_books_cache_valid,s_cached_sd_present,s_cached_sd_mounted,s_scan_pending,s_home_force_full_once;
static unsigned s_cached_store_revision;
static char s_cached_books_dir[128],s_cached_last_path[BOOK_STORE_PATH_MAX];
static EpdRect s_component_dirty;
static bool s_cached_simple_ui,s_cached_simple_full_ui,s_clock_base_attempted;
static unsigned s_cover_generation,s_cached_progress_revision,progress_revision=1;
static struct {bool valid;} s_widget_trail,s_time_trail;
static char s_utility_notice[96];static int s_utility_notice_until,s_view;
static bool s_pop_menu_pending,s_clock_pending,s_time_pending,s_widget_pending,s_component_pending,s_press_pending,s_save_error;
static int s_time_stamp,s_widget_stamp,s_motion_ms;
static void *s_folder;static unsigned s_folder_key;static bool s_folder_attempted;
static int64_t esp_timer_get_time(void){return 0;}
static bool app_settings_simple_ui(void){return false;}
static bool app_settings_simple_full_ui(void){return false;}
static unsigned book_progress_revision(void){return progress_revision;}
static unsigned book_cover_jobs_begin(void){return 1;}
static void app_book_release_shelf_cache(void){}
static void clear_cover_cache(void){}
static void app_home_release_clock_cache(void){}
static void ui_home_clock_preview_invalidate(void){}
static void refresh_days(void){}
static void avatar_prepare_state(void){}
#define HOME_VOCAB_ROOT "vocab"
static int vocab_today_get(const char *root){(void)root;return 0;}
__attribute__((unused)) static void vocab_today_invalidate(void){}
static void refresh_excerpt(void){}
static void rp_boot_mark(const char *s){(void)s;}
static int app_settings_home_component(void){return HOME_ROOT;}
static unsigned folder_key(void){return 1;}
static int s_cover_next,scans,refreshes,probes;
static char latest[BOOK_STORE_PATH_MAX];
static esp_err_t read_pico_sd_get_info(read_pico_sd_info_t *out){*out=(read_pico_sd_info_t){.present=true,.mounted=true};return 0;}
static unsigned book_store_revision(void){return 7;}
static const char *app_settings_books_dir(void){return "/sdcard/books";}
static bool app_settings_home_full_refresh(void){return false;}
static bool book_progress_last_path(char *out,size_t cap){snprintf(out,cap,"%s",latest);return latest[0]!=0;}
static void scan_books(void){++scans;snprintf(s_current.path,sizeof(s_current.path),"%s",latest);}
static void refresh_progress(void){++refreshes;}
static void restore_cover_cache(void){}
static void read_pico_sd_start_probe(void){++probes;}
'''
UNIT += function("remember_scan_state") + "\n" + function("on_enter")
UNIT += r'''
int main(void){
    app_ctx_t ctx={0};
    strcpy(latest,"/sdcard/books/old.epub");on_enter(&ctx);
    assert(scans==1&&probes==1&&!strcmp(s_current.path,latest));
    strcpy(latest,"/sdcard/books/new.epub");on_enter(&ctx);
    assert(scans==2&&probes==1&&!s_scan_pending&&!strcmp(s_current.path,latest));
    on_enter(&ctx);
    assert(scans==2&&refreshes==0&&probes==1); /* No unchanged progress reread. */
    ++progress_revision;on_enter(&ctx);
    assert(scans==2&&refreshes==1&&probes==1);
    latest[0]=0;on_enter(&ctx);
    assert(scans==3&&!s_current.path[0]);
    puts("home_recent: latest book invalidates the home cache without an SD reprobe");
}
'''

with tempfile.TemporaryDirectory(prefix="home-recent-") as temp:
    source = Path(temp) / "test.c"
    binary = Path(temp) / "test"
    source.write_text(UNIT, encoding="utf-8")
    subprocess.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
