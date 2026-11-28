#!/usr/bin/env python3
"""Exercise persisted completion, real list routing and sliced sibling suggestions.
SPDX-License-Identifier: Apache-2.0
Run in a metalio47 --tests board source view.
"""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
reader = root / 'main/apps/app_book.c'
flags = ['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
         '-fsanitize=undefined', '-DESP_ERR_INVALID_RESPONSE=0x108', '-DESP_ERR_INVALID_SIZE=0x104',
         '-I'+str(root/'main/book'), '-I'+str(root/'tools/book_storage_stubs')]
with tempfile.TemporaryDirectory() as directory:
    folder = Path(directory)
    def run(name, code, extra=()):
        source = folder / (name+'.c')
        source.write_text(code)
        binary = folder / name
        subprocess.run([*flags, str(source), *extra, '-o', str(binary)], check=True)
        subprocess.run([str(binary)], cwd=folder, check=True)
        return binary

    state = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "book_reading_state.h"
#include "book_state_store.h"
int main(int argc,char **argv) {
 (void)argv;book_reading_state_t s;
 if(argc>1){assert(book_reading_state_load("book.epub",42,100,&s)==ESP_OK);
   assert(!s.finished&&s.reading_ms==123456);puts("PASS: completion survives process restart");return 0;}
 assert(!mkdir("card",0700));assert(!mkdir("internal",0700));
 assert(book_reading_state_load("book.epub",42,0,&s)==ESP_OK&&!s.finished&&!s.reading_ms);
 assert(book_reading_state_load("book.epub",42,100,&s)==ESP_OK&&s.finished);
 s.reading_ms=123456;assert(book_reading_state_save("book.epub",&s)==ESP_OK);
 assert(book_reading_state_load("book.epub",42,0,&s)==ESP_OK&&s.finished&&s.reading_ms==123456);
 s.finished=0;assert(book_reading_state_save("book.epub",&s)==ESP_OK);
 assert(book_reading_state_load("book.epub",42,100,&s)==ESP_OK&&!s.finished&&s.reading_ms==123456);
 assert(book_reading_state_load("book.epub",43,0,&s)==ESP_OK&&!s.finished&&!s.reading_ms&&s.file_size==43);
 assert(book_state_write("card","forgotten.epub","profile","",0)==ESP_OK);
 assert(book_reading_state_load("forgotten.epub",42,0,&s)==ESP_OK&&!s.finished);
 assert(book_state_write("card","corrupt.epub","profile","bad",3)==ESP_OK);
 assert(book_reading_state_load("corrupt.epub",42,0,&s)==ESP_ERR_INVALID_RESPONSE);
 s.reserved=1;assert(book_reading_state_save("corrupt.epub",&s)==ESP_ERR_INVALID_ARG);
 assert(book_reading_state_load("other.epub",42,0,&s)==ESP_OK&&!s.finished);
 puts("PASS: completion override, measured time, size changes, cleared/corrupt profiles and path isolation");
}
'''
    # State roots are compile-time constants in the production checked store.
    flags += ['-DBOOK_STATE_SD_ROOT="card"', '-DBOOK_STATE_FLASH_ROOT="internal"']
    binary = run('state', state, [str(root/'main/book/book_reading_state.c'),
                                str(root/'main/book/book_state_store.c')])
    subprocess.run([str(binary), 'restart'], cwd=folder, check=True)

    boundary = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define BOOK_TOC_ROWS 8
static struct{uint8_t depth;int64_t until_ms;bool shown;}s_jump_back __attribute__((unused));
static size_t current_toc_position(void){return 0;}
static size_t book_navigation_count(void){return 0;}
static void book_toc_view_open(size_t n,size_t c){(void)n;(void)c;}
static int book_toc_page_of(size_t n,size_t c){(void)n;return (int)(c/BOOK_TOC_ROWS);}
#define MODE_GL16 1
#define APP_READER_TURN_FLASH 7
#define E0470_TURN_RTL 1
#define E0470_TURN_LTR -1
#define ESP_LOGI(...) ((void)0)
enum {READING,TOC,SHELF};
typedef enum {APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_AREA} app_redraw_t;
typedef struct {int64_t now_ms;int leaf;} app_ctx_t;
static bool s_reader_resume_pending,s_reader_raster_failed,s_clear_confirm,s_reader_text_frame=true,s_reader_image_frame;
static bool s_reader_cleanup,s_reader_image_exit_pending,s_reader_turn_pending,s_water_turn_pending,load_fail;
static int s_turn_direction,s_turns,s_unsaved,s_view=READING,s_water_turn_dir;
static size_t s_page,s_chapter,s_jump_offset,page_count=3,chapter_count=2;
static int64_t s_last_turn_ms,s_stats_activity_ms;
static unsigned s_stats_pending_turns,s_session_turns,s_file_size=100,finished_calls;
static const char *s_text="text",*s_path="book.epub";
static bool reader_reflow_font(void){return false;}
static void reader_zoom_reset(void){}
static size_t book_layout_page_count(void){return page_count;}
static size_t book_chapter_count(void){return chapter_count;}
static bool load_chapter(app_ctx_t *c,size_t ch,size_t off,bool last){(void)c;assert(!off);if(load_fail)return false;s_chapter=ch;s_page=last?page_count-1:0;return true;}
static app_redraw_t finished_open(app_ctx_t *c){(void)c;++finished_calls;return APP_REDRAW_PAGE;}
static bool reader_turn_chrome(app_ctx_t *c){(void)c;return false;}
static void set_reader_view(int view){s_view=view;}
static void skip_hidden_image_pages(app_ctx_t *c,int d){(void)c;(void)d;}
static uint8_t app_settings_reader_full_pages(void){return 5;}
static void kosync_state_touch(const char *p,unsigned z,uint16_t ch,uint32_t off){(void)p;(void)z;(void)ch;(void)off;}
static size_t reader_page_offset(size_t p){return p*10;}
static app_redraw_t paint_reading(app_ctx_t *c,int mode){(void)c;assert(mode==MODE_GL16);return APP_REDRAW_AREA;}
static unsigned book_layout_page_image_count(size_t p){(void)p;return 0;}
static const char *app_settings_book_background_path(void){return "";}
static int app_settings_reader_turn_effect(void){return 0;}
'''+function('turn_page',reader)+r'''
int main(void){app_ctx_t c={.now_ms=1000};
 assert(turn_page(&c,-1)==APP_REDRAW_NONE&&!finished_calls&&!s_page&&!s_chapter);
 assert(turn_page(&c,1)==APP_REDRAW_AREA&&s_page==1&&!finished_calls);
 assert(turn_page(&c,1)==APP_REDRAW_AREA&&s_page==2&&!finished_calls);
 assert(turn_page(&c,1)==APP_REDRAW_AREA&&s_page==0&&s_chapter==1&&!finished_calls);
 assert(turn_page(&c,1)==APP_REDRAW_AREA&&s_page==1&&!finished_calls);
 assert(turn_page(&c,1)==APP_REDRAW_AREA&&s_page==2&&!finished_calls); // Reaching the last page is still reading.
 assert(turn_page(&c,1)==APP_REDRAW_PAGE&&finished_calls==1&&s_page==2&&s_chapter==1);
 s_page=2;s_chapter=0;load_fail=true;assert(turn_page(&c,1)==APP_REDRAW_PAGE&&s_view==TOC&&finished_calls==1);
 load_fail=false;s_reader_resume_pending=true;assert(turn_page(&c,1)==APP_REDRAW_NONE&&finished_calls==1);s_reader_resume_pending=false;
 page_count=chapter_count=0;s_page=s_chapter=0;assert(turn_page(&c,1)==APP_REDRAW_NONE&&finished_calls==1);
 puts("PASS: actual page/chapter turns finish only past the final page; first page, load errors, resume and empty books stay guarded");}
'''
    run('boundary', boundary)

    volume = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "book_reading_state.h"
typedef enum {APP_REDRAW_NONE,APP_REDRAW_PAGE} app_redraw_t;
typedef struct {int leaf;} app_ctx_t;
enum {READING,TOC,FINISHED,OTHER,READER_PANEL_NONE=0,READER_PANEL_TOOLS,
      READER_PANEL_FONT_PICKER,READER_PANEL_BOOKMARKS,OTHER_PANEL};
#define BOOK_FONT_PAGE 9
static int s_view=READING,s_reader_panel,s_font_page,s_bookmark_page,s_pressed_control;
static bool s_scan_pending,s_clear_confirm,s_toc_jump_open,s_bookmark_edit,s_bookmark_delete_confirm;
static char *s_text="text";
static struct {bool open;} s_reader_choice;
static struct {bool pending;} s_reader_tap;
static int saved_calls,action_calls,family_count=20,marks_count=16,notes_direction;
static bool notes_open;
typedef int app_reader_key_action_t;
typedef struct {int count;} reader_bookmarks_t;
static bool bookmark_load(reader_bookmarks_t *m){m->count=marks_count;return true;}
static int bookmark_pages(int count){return (count+5)/6;}
static int ttf_font_family_count(void){return family_count;}
static bool ui_choice_page(void *p,int direction){(void)p;notes_direction=direction;return true;}
static bool book_notes_volume_key(int direction,app_redraw_t *r){notes_direction=direction;if(notes_open)*r=APP_REDRAW_PAGE;return notes_open;}
static int leaves(void){return 3;}
static app_redraw_t finished_back(void){s_view=READING;return APP_REDRAW_PAGE;}
static void reader_indent_hold_cancel(void){}
static app_reader_key_action_t app_settings_volume_key_action(int index){++saved_calls;return index+10;}
static app_redraw_t reader_key_action(app_ctx_t *ctx,int action){(void)ctx;assert(action==10||action==11);++action_calls;return APP_REDRAW_PAGE;}
'''+function('app_book_volume_key', reader)+r'''
int main(void){app_ctx_t ctx={0};app_redraw_t r;
 s_view=TOC;assert(app_book_volume_key(&ctx,1,&r)&&ctx.leaf==1&&r==APP_REDRAW_PAGE);
 assert(app_book_volume_key(&ctx,-1,&r)&&ctx.leaf==0);
 assert(app_book_volume_key(&ctx,-1,&r)&&ctx.leaf==0&&r==APP_REDRAW_NONE);
 ctx.leaf=2;assert(app_book_volume_key(&ctx,1,&r)&&ctx.leaf==2&&r==APP_REDRAW_NONE);
 s_toc_jump_open=true;assert(app_book_volume_key(&ctx,-1,&r)&&ctx.leaf==2);s_toc_jump_open=false;
 s_view=READING;s_reader_panel=READER_PANEL_FONT_PICKER;
 assert(app_book_volume_key(&ctx,1,&r)&&s_font_page==1);
 assert(app_book_volume_key(&ctx,1,&r)&&s_font_page==2);
 assert(app_book_volume_key(&ctx,1,&r)&&s_font_page==2&&r==APP_REDRAW_NONE);
 assert(app_book_volume_key(&ctx,-1,&r)&&s_font_page==1);
 family_count=0;s_font_page=0;assert(app_book_volume_key(&ctx,1,&r)&&!s_font_page);
 s_reader_panel=READER_PANEL_BOOKMARKS;
 assert(app_book_volume_key(&ctx,1,&r)&&s_bookmark_page==1);
 s_bookmark_edit=true;assert(app_book_volume_key(&ctx,1,&r)&&s_bookmark_page==1);s_bookmark_edit=false;
 assert(app_book_volume_key(&ctx,1,&r)&&s_bookmark_page==2);
 assert(app_book_volume_key(&ctx,1,&r)&&s_bookmark_page==2&&r==APP_REDRAW_NONE);
 s_reader_panel=READER_PANEL_NONE;notes_open=true;
 assert(app_book_volume_key(&ctx,-1,&r)&&notes_direction==-1&&r==APP_REDRAW_PAGE);notes_open=false;
 s_reader_choice.open=true;assert(app_book_volume_key(&ctx,1,&r)&&notes_direction==1);s_reader_choice.open=false;
 s_reader_panel=OTHER_PANEL;assert(app_book_volume_key(&ctx,1,&r)&&r==APP_REDRAW_NONE);
 assert(!saved_calls&&!action_calls); // Lists must bypass all saved shortcuts.
 s_reader_panel=READER_PANEL_NONE;assert(app_book_volume_key(&ctx,1,&r)&&saved_calls==1&&action_calls==1);
 s_view=OTHER;assert(!app_book_volume_key(&ctx,1,&r));
 puts("PASS: TOC/font/bookmark/notes/choice ownership, list bounds and restored reader shortcuts");}
'''
    run('volume', volume)

    finish = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "book_reading_state.h"
#define BOOK_STORE_PATH_MAX 288
#define READING 1
#define FINISHED 2
#define SHELF 0
typedef enum {APP_REDRAW_NONE,APP_REDRAW_PAGE} app_redraw_t;
typedef struct {int home;} app_ctx_t;
typedef struct {int type,x0,y0;} ui_gesture_event_t;
#define UI_GESTURE_TAP 1
typedef struct {int x,y,width,height;} EpdRect;
static book_reading_state_t s_reading_state={.finished=1};
static bool s_reading_state_valid=true,s_reading_state_dirty,s_finished_save_failed,s_finished_target,fail_write,s_shelf_cache_valid=true;
static unsigned s_finished_count=1,s_file_size=42,s_page;
static char s_path[288]="current.epub",*s_text="text";
static struct {char path[288];} s_finished_next[3]={{{"next.epub"}}};
static unsigned saved_mark=1,notifications;
static void *s_finished_dir;
esp_err_t book_reading_state_load(const char *p,uint32_t z,unsigned pct,book_reading_state_t *out){(void)p;(void)z;(void)pct;*out=s_reading_state;return ESP_OK;}
esp_err_t book_reading_state_save(const char *p,const book_reading_state_t *v){assert(!strcmp(p,s_path));if(fail_write)return ESP_FAIL;saved_mark=v->finished;return ESP_OK;}
static unsigned percent(unsigned p){(void)p;return 100;}
static void book_progress_notify_changed(void){++notifications;}
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;}
static void copy_text(char *d,size_t n,const char *v){snprintf(d,n,"%s",v);}
static bool open_book(app_ctx_t *c,const char *p){(void)c;assert(!strcmp(p,"next.epub"));return true;}
static void set_reader_view(int v){(void)v;}
static app_redraw_t finished_back(void){return APP_REDRAW_PAGE;}
static void ui_nav_request(app_ctx_t *c,int v){assert(v==0);++c->home;}
'''+function('finished_control',reader)+'\n'+function('finished_gesture',reader)+r'''
int main(void){app_ctx_t ctx={0};ui_gesture_event_t ev={.type=UI_GESTURE_TAP,.x0=100,.y0=400};
 assert(finished_gesture(&ctx,&ev)==APP_REDRAW_PAGE&&!s_reading_state.finished&&!saved_mark&&notifications==1&&!s_shelf_cache_valid);
 assert(finished_gesture(&ctx,&ev)==APP_REDRAW_PAGE&&s_reading_state.finished&&saved_mark==1);
 fail_write=true;assert(finished_gesture(&ctx,&ev)==APP_REDRAW_PAGE&&s_finished_save_failed&&s_reading_state.finished&&saved_mark==1);
 fail_write=false;assert(finished_gesture(&ctx,&ev)==APP_REDRAW_PAGE&&!s_finished_save_failed&&!s_reading_state.finished&&!saved_mark);
 ev.y0=650;s_finished_dir=&ctx;assert(finished_gesture(&ctx,&ev)==APP_REDRAW_NONE);s_finished_dir=NULL;
 assert(finished_gesture(&ctx,&ev)==APP_REDRAW_PAGE); // Open the first suggestion.
 ev.x0=500;ev.y0=1060;assert(finished_gesture(&ctx,&ev)==APP_REDRAW_NONE&&ctx.home==1);
 puts("PASS: reversible completion, failed-write retry preserves intent, suggested open and Home");}
'''
    run('finish',finish)

    batch = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_err.h"
typedef struct {int leaf;} app_ctx_t;
typedef struct {char path[32];bool selected,removed,has_progress,finished;unsigned pct,chapter,recent;} shelf_entry_t;
typedef struct {shelf_entry_t entry;} delete_retry_t;
static shelf_entry_t s_shelf[2];
static int s_count=2,s_batch_kind;
enum {BATCH_CLEAR,BATCH_DELETE,BATCH_UNSHELF};
static char s_path[32]="other.epub",s_batch_message[128];
static bool s_batch_confirm=true;
static unsigned s_store_revision;
static bool shelf_hidden_save(const char *p,bool v){(void)p;(void)v;return true;}
static void free_book(void){assert(0);}
static delete_retry_t *delete_retry_reserve(shelf_entry_t *e){(void)e;return NULL;}
static void delete_retry_discard(const char *p){(void)p;}
static esp_err_t book_store_delete(const char *p,bool *r){(void)p;(void)r;return ESP_FAIL;}
static void pending_discard(const char *p){(void)p;}
static void book_store_notify_changed(void){}
static unsigned book_store_revision(void){return 0;}
static esp_err_t book_progress_forget(const char *p){return strstr(p,"failure")?ESP_FAIL:ESP_OK;}
static void refresh_capacity(void){}
static void sort_shelf(app_ctx_t *ctx){(void)ctx;}
static int leaves(void){return 1;}
'''+function('batch_apply',root/'firmware/full/library/app_book_library.inc')+r'''
int main(void){app_ctx_t ctx={0};
 strcpy(s_shelf[0].path,"success.epub");strcpy(s_shelf[1].path,"failure.epub");
 for(int i=0;i<2;++i){s_shelf[i].selected=s_shelf[i].has_progress=s_shelf[i].finished=true;s_shelf[i].pct=100;}
 batch_apply(&ctx);
 assert(!s_shelf[0].finished&&!s_shelf[0].has_progress&&!s_shelf[0].pct&&!s_shelf[0].selected);
 assert(s_shelf[1].finished&&s_shelf[1].has_progress&&s_shelf[1].pct==100&&s_shelf[1].selected);
 assert(!s_batch_confirm&&s_count==2);
 puts("PASS: bulk progress clear resets completion only after successful persistence and preserves failed items");}
'''
    run('batch',batch)

    status = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
typedef struct {bool has_progress,finished,favorite;unsigned pct;} shelf_entry_t;
'''+function('shelf_status_is',reader)+r'''
int main(void){shelf_entry_t item={.has_progress=true,.pct=100,.finished=true};
 assert(shelf_status_is(&item,3)&&!shelf_status_is(&item,1)&&!shelf_status_is(&item,2));
 item.finished=false;assert(!shelf_status_is(&item,3)&&shelf_status_is(&item,1));
 item.pct=0;assert(shelf_status_is(&item,2)&&!shelf_status_is(&item,1));
 item.finished=true;assert(!shelf_status_is(&item,2)&&shelf_status_is(&item,3));
 puts("PASS: shelf status distinguishes completion from percentage without overlapping unread/completed");}
'''
    run('status',status)

    scan = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include "book_reading_state.h"
#define BOOK_STORE_PATH_MAX 288
typedef enum {APP_REDRAW_NONE,APP_REDRAW_PAGE} app_redraw_t;
typedef struct {bool touched;} touch_t;
typedef struct {bool consumed,request_app,request_menu,request_return;touch_t *touch;} app_ctx_t;
static DIR *s_finished_dir;
static char s_path[288]="shelf/M.epub",s_finished_folder[288]="shelf";
static struct {char path[288],name[128];} s_finished_next[3];
static unsigned s_finished_count,s_finished_scanned;
static bool s_finished_scan_failed;
static int64_t esp_timer_get_time(void){static int64_t n;return ++n;}
static void copy_text(char *d,size_t n,const char *t){snprintf(d,n,"%s",t);}
static bool book_title_from_path(const char *p,char *d,size_t n){(void)p;(void)d;(void)n;return false;}
static void clean_filename(char *d,size_t n,const char *p){snprintf(d,n,"%s",p);}
static bool book_file_path_supported(const char *p){const char *e=strrchr(p,'.');return e&&(!strcasecmp(e,".epub")||!strcasecmp(e,".txt"));}
typedef struct {unsigned pct;} book_progress_t;
static bool book_progress_load(const char *p,uint32_t size,book_progress_t *out){(void)size;out->pct=strstr(p,"started")?1:0;return out->pct>0;}
esp_err_t book_reading_state_load(const char *p,uint32_t size,unsigned pct,book_reading_state_t *s){(void)size;(void)pct;memset(s,0,sizeof(*s));s->finished=strstr(p,"done")!=NULL;return strstr(p,"broken")?ESP_FAIL:ESP_OK;}
'''+function('finished_compare',reader)+'\n'+function('finished_tick',reader)+r'''
static void file(const char *name){char p[288];snprintf(p,sizeof(p),"shelf/%s",name);FILE *f=fopen(p,"w");assert(f);fclose(f);}
int main(void){app_ctx_t ctx={0};assert(!mkdir("shelf",0700));
 const char *names[]={"M.epub","A.epub","Z.epub","N.epub","O.epub","Q-started.epub","P-done.epub","P-broken.epub",".hidden.epub","ignore.ttf"};
 for(unsigned i=0;i<sizeof(names)/sizeof(*names);++i)file(names[i]);
 assert(!mkdir("shelf/P-dir.epub",0700));
 s_finished_dir=opendir("shelf");assert(s_finished_dir);
 ctx.consumed=true;assert(finished_tick(&ctx)==APP_REDRAW_NONE&&!s_finished_scanned);ctx.consumed=false;
 unsigned calls=0;while(s_finished_dir){finished_tick(&ctx);assert(++calls<20);}
 assert(!s_finished_scan_failed&&s_finished_count==3);
 assert(!strcmp(s_finished_next[0].name,"N.epub")&&!strcmp(s_finished_next[1].name,"O.epub")&&!strcmp(s_finished_next[2].name,"Z.epub"));
 strcpy(s_path,"shelf/Z.epub");s_finished_count=s_finished_scanned=0;s_finished_dir=opendir("shelf");
 while(s_finished_dir)finished_tick(&ctx);
 assert(!strcmp(s_finished_next[0].name,"A.epub")&&!strcmp(s_finished_next[1].name,"M.epub"));
 puts("PASS: sliced, sorted, wrapping suggestions exclude current/started/done/corrupt/hidden/non-book/directory entries");}
'''
    run('scan', scan)

# Check the integration edges that are outside the extracted list controller.
loop = (root/'main/app/app_loop.c').read_text()
route = function('volume_key_action', root/'main/app/app_loop.c')
assert route.index('app_book_volume_key') < route.index('app_settings_volume_key_action')
turn = function('turn_page', reader)
assert turn.index('s_page + 1 < book_layout_page_count()') < turn.index('return finished_open(ctx)')
assert turn.index('s_chapter + 1 < book_chapter_count()') < turn.index('return finished_open(ctx)')
print('PASS: list dispatch precedes global shortcuts; completion follows page/chapter boundaries')
