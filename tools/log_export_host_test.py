"""中文：真实日志导出不得持锁I/O；覆盖回绕、并发日志和SD错误。/ English: Real exports never do locked I/O; cover wrap, concurrent logs and SD failures."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];source=root/'main/app/log_export.c'
unit=r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#define LOG_BUF_SIZE (32*1024)
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_NO_MEM 2
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4
#define ESP_LOGI(...) ((void)0)
typedef int esp_err_t;
typedef int (*vprintf_like_t)(const char *,va_list);
static char s_log_buf[LOG_BUF_SIZE];
static size_t s_log_head,s_log_total_written;
static bool s_in_vprintf;
static bool cache_ready=true;
static bool log_ring_accessible(void){return cache_ready;}
static int depth,s_log_lock,failure,allocated,freed;
static bool allocation_fail,sd_missing,inject;
static vprintf_like_t s_prev_vprintf;
#define portENTER_CRITICAL(p) do{(void)(p);assert(depth==0);++depth;}while(0)
#define portEXIT_CRITICAL(p) do{(void)(p);assert(depth==1);--depth;}while(0)
static void emit(const char *fmt,...);
static char saved[70000];
static FILE *checked_open(const char *name,const char *mode){
 assert(!depth);if(failure==4)return NULL;
 if(inject){emit("DURING_SD\n");}
 return fopen(name,!strcmp(mode,"w")?"w+":mode);
}
static int checked_print(FILE *f,const char *fmt,...){assert(!depth);va_list a;va_start(a,fmt);int n=vfprintf(f,fmt,a);va_end(a);return n;}
static size_t checked_write(const void *p,size_t size,size_t n,FILE *f){assert(!depth);if(failure==1)return n?fwrite(p,size,n-1,f):0;return fwrite(p,size,n,f);}
static int checked_flush(FILE *f){assert(!depth);return failure==2?EOF:fflush(f);}
static int checked_error(FILE *f){assert(!depth);return failure==5?1:ferror(f);}
static int checked_close(FILE *f){assert(!depth);rewind(f);size_t n=fread(saved,1,sizeof(saved)-1,f);saved[n]=0;int r=fclose(f);return failure==3?EOF:r;}
static int checked_sync(int fd){assert(!depth);return failure==6?-1:fsync(fd);}
static int checked_rename(const char *from,const char *to){assert(!depth);
 if((failure==7&&strstr(from,".tmp"))||(failure==8&&strstr(to,".old"))){errno=EIO;return -1;}
 return rename(from,to);
}
#define fsync checked_sync
#define rename checked_rename
#define fopen checked_open
#define fprintf checked_print
#define fwrite checked_write
#define fflush checked_flush
#define ferror checked_error
#define fputc(c,f) (assert(!depth), fputc((c),(f)))
#define fclose checked_close
static void *heap_caps_malloc(size_t n,int caps){(void)caps;assert(!depth);if(allocation_fail)return NULL;void *p=malloc(n);if(p)++allocated;return p;}
static void heap_caps_free(void *p){assert(!depth);if(p)++freed;free(p);}
static void log_export_heap_checkpoint(const char *phase){assert(phase && !depth);}
static unsigned heap_caps_get_free_size(int caps){(void)caps;return 1000;}
static unsigned heap_caps_get_minimum_free_size(int caps){(void)caps;return 500;}
static unsigned heap_caps_get_total_size(int caps){(void)caps;return 2000;}
typedef struct {bool mounted,space_valid;uint64_t capacity_bytes,free_bytes;}read_pico_sd_info_t;
static int read_pico_sd_get_info(read_pico_sd_info_t *p){p->mounted=!sd_missing;return ESP_OK;}
typedef struct {const char *version,*project_name,*idf_ver,*date,*time;}esp_app_desc_t;
static const esp_app_desc_t *esp_app_get_description(void){static const esp_app_desc_t a={"1.1.7RC5-vn","S31","6.1","date","time"};return &a;}
static int64_t esp_timer_get_time(void){return 1000;}
typedef struct {unsigned battery_mv;}pmu_snapshot_t;
static const pmu_snapshot_t *read_pico_pmu_get(void){return NULL;}
static int pmu_battery_percent(const pmu_snapshot_t *p){(void)p;return 50;}
static bool pmu_battery_charging(const pmu_snapshot_t *p){(void)p;return false;}
static bool app_settings_ble_turner(void){return false;}
static bool ble_pt_running(void){return false;}
static bool ble_pt_connected(void){return false;}
static const char *ble_pt_connected_name(void){return "";}
static unsigned ble_pt_bond_count(void){return 0;}
typedef struct {const char *name,*addr;}ble_pt_bond_t;
static const ble_pt_bond_t *ble_pt_bond(unsigned i){(void)i;return NULL;}
static bool crash_note_valid(void){return false;}
static const char *crash_note_text(void){return "";}
#define CRASH_NOTE_DETAIL_SIZE 160
static const char *crash_note_detail(void){return NULL;}
static unsigned rp_crash_log_count(void){return 0;}
static const char *rp_crash_log_line(unsigned i){(void)i;return NULL;}
typedef struct {int state,mode,last_error;bool network_ready,wifi_configured;unsigned sta_count,cur_bytes;}read_pico_transfer_status_t;
static void read_pico_transfer_get_status(read_pico_transfer_status_t *s){memset(s,0,sizeof(*s));}
static bool ble_pt_connecting(void){return false;}
static bool ble_pt_scanning(void){return false;}
static bool ble_pt_idle_ready(void){return true;}
static uint32_t ble_pt_reconnect_wait_ms(void){return 300000;}
static int esp_reset_reason(void){return 1;}
#if DPREADER_BOARD_METALIO47
typedef struct {bool enabled,ui_pinned,unsafe;unsigned idle_windows,rejected,wakes,leases,faults;int last_error;}metalio_cpu_stats_t;
static metalio_cpu_stats_t metalio_cpu_stats(void){return (metalio_cpu_stats_t){.enabled=true,.idle_windows=42};}
static int esp_clk_tree_src_get_freq_hz(int src,int precision,uint32_t *hz){(void)src;(void)precision;*hz=80000000;return 0;}
#define SOC_MOD_CLK_CPU 1
#define ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED 1
// 中文：1.5.19 电量/延迟计数使用真实实现，低电量保护用桩。/ English: real 1.5.19 power/latency counters; stubbed battery guard.
#include "metalio_power_stats.h"
static int battery_guard_level(void){return 1;}
static int battery_guard_remaining_minutes(void){return 321;}
static void metalio_sleep_previous_summary(char *out,size_t n){snprintf(out,n,"sleep_previous test");}
#endif
'''
for name in ('log_export_vprintf','log_export_clear','log_snapshot','log_export_to_sd'):
 unit+=function(name,source).replace('/sdcard/','')+'\n'
unit+=(root/'components/metalio_hw/metalio_power_stats.c').read_text()+'\n'
unit+=r'''
static int sink(const char *fmt,va_list a){(void)fmt;(void)a;return 0;}
static void emit(const char *fmt,...){va_list a;va_start(a,fmt);log_export_vprintf(fmt,a);va_end(a);}
int main(void){
 s_prev_vprintf=sink;
 char path[64],error[128];
 cache_ready=false;emit("CACHE_OFF");assert(s_log_total_written==0);
 log_export_clear();char scratch[LOG_BUF_SIZE];assert(log_snapshot(scratch)==0);cache_ready=true;
 metalio_power_note_wait(true,1200);metalio_power_note_scan(METALIO_SCAN_GC);metalio_power_note_latency(METALIO_LATENCY_TURN,75);
 emit("BEFORE_SD\n");inject=true;
 assert(log_export_to_sd(path,sizeof(path),error,sizeof(error))==ESP_OK);
 assert(strstr(saved,"BEFORE_SD")&&!strstr(saved,"DURING_SD"));
 // 1.5.19：导出包含电量与延迟计数。/ The export includes power and latency counters.
 assert(strstr(saved,"Power: wait_idle_ms=1200 waits_idle=1 ")&&strstr(saved,"scans GC=1 GL=0 DU=0")&&strstr(saved,"battery_level=1 remaining_min=321"));
 assert(strstr(saved,"Latency turn: <50=0 <100=1 ")&&strstr(saved,"p50<=100 p95<=100")&&strstr(saved,"Latency boot: <50=0"));
 assert(!depth&&allocated==freed);
 inject=false;
 for(int i=0;i<6000;++i)emit("%06d\n",i);
 char expected[LOG_BUF_SIZE];assert(log_snapshot(expected)==LOG_BUF_SIZE);
 assert(log_export_to_sd(path,sizeof(path),error,sizeof(error))==ESP_OK);
 const char *payload=strstr(saved,"--------------------------------------------------------\n");
 assert(payload);payload+=strlen("--------------------------------------------------------\n");
 assert(!memcmp(payload,expected,LOG_BUF_SIZE));
 char previous_bytes[sizeof(saved)];strcpy(previous_bytes,saved);
 for(failure=1;failure<=8;++failure){
  error[0]=0;assert(log_export_to_sd(path,sizeof(path),error,sizeof(error))==ESP_FAIL);
  assert(error[0]&&!depth&&allocated==freed);
  FILE *old=checked_open("dpreader_log.txt","r");
  // Open failure is injected only on the export path.
  if(!old){int original=failure;failure=0;old=checked_open("dpreader_log.txt","r");failure=original;}
  assert(old);char actual[sizeof(saved)];size_t n=fread(actual,1,sizeof(actual)-1,old);actual[n]=0;
  assert(!strcmp(previous_bytes,actual));checked_close(old);
 }
 failure=0;allocation_fail=true;
 assert(log_export_to_sd(path,sizeof(path),error,sizeof(error))==ESP_ERR_NO_MEM);
 allocation_fail=false;sd_missing=true;
 assert(log_export_to_sd(path,sizeof(path),error,sizeof(error))==ESP_ERR_INVALID_STATE);
 assert(!depth&&allocated==freed);
 puts("PASS: actual SD export has no locked I/O; stable wrapped snapshot survives concurrent logging; short writes/open/flush/close/OOM/media errors clean up safely");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.c';p.write_text(unit);binary=p.with_suffix('')
 subprocess.run(['cc','-DDPREADER_BOARD_METALIO47=1','-std=gnu11','-Wall','-Wextra','-Werror','-I'+str(root/'components/metalio_hw/include'),str(p),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],cwd=tmp,check=True)
