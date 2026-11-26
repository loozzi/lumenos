/* SPDX-License-Identifier: Apache-2.0
 * Long-press text selects instead of opening TOC; TOC remains in reader tools.
 * Geometry and annotations are restricted to horizontal text in this first version.
 * A tap on an EPUB footnote reference opens its text in a small popup instead of jumping pages.
 * User 2026-11-19: every tap dismisses a footnote without turning the book;
 * long footnotes page with swipes or volume. Notes lists also accept volume paging.
 */
#include "book_notes_ui.h"
#include "book_annotations.h"
#include "book_annotation_exchange.h"
#include "book_dictionary.h"
#include "book_dict_order.h"
#include "book_notes_book.h"
#include <time.h>
#include "ui_keyboard.h"
#include "ui_kit.h"
#include "ttf_font.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef NOTES_ROOT
#define NOTES_ROOT "/sdcard/.kiikoread-notes"
#endif
#ifndef DICT_ROOT
#define DICT_ROOT "/sdcard/dict"
#endif
enum { CLOSED, SELECT, EDIT_NOTE, LOOKUP, LIST, FOOTNOTE, DICTS };
enum { DEFINITION_PX=32, DEFINITION_LINE_H=44, DEFINITION_LINES=10, NOTES_LIST_ROWS=7, NOTES_LIST_ROW_H=80 };
// 词典管理与未命中建议的行。/ Rows of the dictionary manager and miss suggestions.
enum { DICT_ROWS=7, DICT_ROW_H=72, DICT_ROW_Y=256, SUGGEST_ROW_Y=400, SUGGEST_ROW_H=62 };
// 整本书列表：标题行后 6 行。/ Whole-book list: a header row, then 6 rows.
enum { BOOK_ROWS=6, BOOK_ROW_Y=316 };
static char s_book_title[128];
static book_notes_title_fn s_chapter_title;
// 脚注引用的触控外扩，编号字很小。/ Touch slop around tiny footnote markers.
enum { FOOTNOTE_SLOP=8, FOOTNOTE_BUTTON_H=58 };
typedef struct {
    book_annotations_t annotations, committed;
    book_dictionary_t dictionary;
    const char *text;
    size_t length,start,end,anchor_start,anchor_end,page;
    uint32_t generation;
    int mode,list_at,definition_page,menu_y;
    bool valid,dragging,searching,unsupported;
    char draft[BOOK_NOTE_CAP],query[256],message[160];
    ui_text_edit_t edit;
    book_dict_list_t dicts;     // 已排序的词典清单 / Ordered dictionary list
    unsigned dict_at;           // 下一个要打开的词典 / Next dictionary to open
    int dict_list_at;           // 管理页首行 / First row of the manager page
    bool dicts_dirty;
    unsigned suggestion_count;
    char suggestions[BOOK_DICT_SUGGESTIONS][BOOK_DICT_SUGGESTION_CAP];
    char book_path[512],storage_root[512];
    uint32_t chapter;            // 绑定的章 / Bound chapter
    bool list_book;              // 列表显示整本书 / The list shows the whole book
    book_notes_book_t *book;     // 整本书列表（PSRAM）/ Whole-book list (PSRAM)
    bool jump_pending;           // 待跳到其他章 / Pending jump to another chapter
    uint32_t jump_chapter;
    size_t jump_offset;
    const html_link_t *links;
    size_t link_count;
    book_notes_footnote_fn footnote_load;
    char *footnote;
    unsigned footnote_lines,footnote_page_lines;
    EpdRect footnote_panel;
} notes_t;
static notes_t *s;
static const EpdRect dictionary_panel={36,240,612,650};
static const EpdRect notes_list_panel={24,240,636,700};
static void close_dirs(void) { s->searching=false; }
static void book_list_free(void) {
    if(s->book){book_notes_book_end(s->book);free(s->book);s->book=NULL;}
    s->list_book=false;
}
void book_notes_cancel(void) {
    if(!s)return;
    ui_gesture_enable_held_drag(false);
    if(s->mode==EDIT_NOTE)ui_keyboard_end();
    book_dictionary_close(&s->dictionary);close_dirs();s->mode=CLOSED;s->dragging=false;
    free(s->footnote);s->footnote=NULL;
    book_list_free();
}
void book_notes_set_book(const char *title,book_notes_title_fn chapter_title) {
    snprintf(s_book_title,sizeof(s_book_title),"%s",title?title:"");s_chapter_title=chapter_title;
}
bool book_notes_take_jump(uint32_t *chapter,size_t *offset) {
    if(!s||!s->jump_pending)return false;
    s->jump_pending=false;*chapter=s->jump_chapter;*offset=s->jump_offset;return true;
}
void book_notes_reset(void) { if(!s)return;book_notes_cancel();free(s);s=NULL; }
bool book_notes_active(void) {return s&&s->mode!=CLOSED;}
bool book_notes_footnote_active(void) {return s&&s->mode==FOOTNOTE;}
bool book_notes_open_list(size_t page) {
    if(!s)return false;
    book_notes_cancel();s->mode=LIST;s->list_at=0;s->page=page;
    s->generation=book_layout_generation();return true;
}
size_t book_notes_page(size_t fallback) {return book_notes_active()?s->page:fallback;}
void book_notes_bind(const char *path,uint32_t chapter,const char *text,size_t length) {
    book_notes_reset();
    s=heap_caps_calloc(1,sizeof(*s),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s)return;
    s->text=text;s->length=length;s->chapter=chapter;
    if(strlen(path)>=sizeof(s->book_path))return;
    strcpy(s->book_path,path);
    char directory[512];
    if(!book_annotation_directory(path,directory,sizeof(directory))||
       snprintf(s->storage_root,sizeof(s->storage_root),"%s/snapshots",directory)>=(int)sizeof(s->storage_root))return;
    s->valid=book_annotations_load(&s->annotations,s->storage_root,"",chapter,text,length);
    /* Prefer RC3..5 sidecar snapshots, then RC2. Old files stay untouched.
     * Preserve the snapshot sequence while committing the migrated copy. */
    if(s->valid&&!s->annotations.sequence) {
        book_annotations_t *legacy=heap_caps_calloc(1,sizeof(*legacy),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!legacy){s->valid=false;return;}
        char sidecar[512],old_root[540];
        bool ok=book_annotation_sidecar(path,sidecar,sizeof(sidecar))&&
            snprintf(old_root,sizeof(old_root),"%s/kiikoread",sidecar)<(int)sizeof(old_root);
        if(ok)ok=book_annotations_load(legacy,old_root,"",chapter,text,length);
        if(ok&&!legacy->sequence)ok=book_annotations_load(legacy,NOTES_ROOT,path,chapter,text,length);
        if(!ok){free(legacy);s->valid=false;return;}
        if(legacy->sequence) {
            s->annotations.count=legacy->count;memcpy(s->annotations.items,legacy->items,sizeof(legacy->items));
            s->annotations.sequence=legacy->sequence-1;
            s->valid=book_annotations_save(&s->annotations,s->storage_root);
        }
        free(legacy);
    }
    s->committed=s->annotations;
}
void book_notes_bind_links(const html_link_t *links,size_t count,book_notes_footnote_fn load) {
    if(!s)return;
    s->links=links;s->link_count=links?count:0;s->footnote_load=load;
}
static bool hit(EpdRect r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;}
static ui_icon_t action_icon(const char *label) {
    if(!strcmp(label,"Tra từ"))return UI_ICON_SEARCH;
    if(!strcmp(label,"Tô sáng"))return UI_ICON_HIGHLIGHTER;
    if(!strcmp(label,"Ghi chú"))return UI_ICON_PENCIL;
    if(!strcmp(label,"Lưu ghi chú"))return UI_ICON_SAVE;
    if(!strcmp(label,"Các ghi chú"))return UI_ICON_LIST;
    if(!strcmp(label,"Trước")||!strcmp(label,"Trang trước"))return UI_ICON_CHEVRON_LEFT;
    if(!strcmp(label,"Sau")||!strcmp(label,"Trang sau")||!strcmp(label,"Từ điển kế"))return UI_ICON_CHEVRON_RIGHT;
    if(!strcmp(label,"Quản lý"))return UI_ICON_SLIDERS_HORIZONTAL;
    if(!strcmp(label,"Lên"))return UI_ICON_CHEVRON_UP;
    if(!strcmp(label,"Xuống"))return UI_ICON_CHEVRON_DOWN;
    if(!strcmp(label,"Xong"))return UI_ICON_SAVE;
    return UI_ICON_X;
}
static void button_icon(uint8_t *fb,EpdRect r,const char *label,ui_icon_t icon) {
    ui_fill_round_rect(fb,r,12,0xe0);
    ui_draw_icon(fb,r.x+20,r.y+r.height/2,22,icon,0);
    r.x+=36;r.width-=40;ui_text_box(fb,r,25,label,EPD_DRAW_ALIGN_CENTER,false);
}
static void button(uint8_t *fb,EpdRect r,const char *label) { button_icon(fb,r,label,action_icon(label)); }
static EpdRect menu_action_rect(int action) {
    int y=s->mode==SELECT?s->menu_y:0;
    return (EpdRect){26+action*126,y+4,122,64};
}
typedef struct {int top,bottom;bool found;} selection_bounds_t;
static void selection_bounds(void *ctx,size_t a,size_t b,EpdRect r) {
    selection_bounds_t *bounds=ctx;
    if(a>=s->end||b<=s->start)return;
    if(!bounds->found||r.y<bounds->top)bounds->top=r.y;
    if(!bounds->found||r.y+r.height>bounds->bottom)bounds->bottom=r.y+r.height;
    bounds->found=true;
}
static void place_menu(size_t page,EpdRect body,int px) {
    selection_bounds_t bounds={0};
    book_layout_visit_page(page,body,px,selection_bounds,&bounds);
    s->menu_y=0; /* Reserved status band is always outside body (starts at 80). */
    if(!bounds.found)return;
    if(bounds.bottom+84<=body.y+body.height)s->menu_y=bounds.bottom+8;
    else if(bounds.top-84>=body.y)s->menu_y=bounds.top-80;
}
static size_t previous(size_t at) {if(!at)return 0;--at;while(at&&((unsigned char)s->text[at]&0xc0)==0x80)--at;return at;}
static size_t next(size_t at) {return ui_text_edit_next(s->text,at);}
static uint32_t cp(size_t at) {
    const unsigned char *p=(const unsigned char *)s->text+at;uint32_t c=*p++;
    if(c<128)return c;
    unsigned n=c<0xe0?1:c<0xf0?2:3;c&=n==1?31:n==2?15:7;
    while(n--&&(size_t)(p-(const unsigned char *)s->text)<s->length)c=(c<<6)|(*p++&63);
    return c;
}
static bool word(size_t at) {
    uint32_t c=cp(at);
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='\''||
        (c>=0xc0&&c<=0x2af)||(c>=0x300&&c<=0x36f)||(c>=0x1e00&&c<=0x1eff);
}
static void range(size_t at,size_t *a,size_t *b) {
    *a=at;*b=next(at);
    if(!word(at))return;
    while(*a&&word(previous(*a)))*a=previous(*a);
    while(*b<s->length&&word(*b))*b=next(*b);
}
typedef struct {int x,y;size_t at;bool found;} pick_t;
static void pick(void *ctx,size_t a,size_t b,EpdRect r) {
    (void)b;pick_t *p=ctx;if(hit(r,p->x,p->y)){p->at=a;p->found=true;}
}
static bool select_at(size_t page,EpdRect body,int px,int x,int y,size_t *a,size_t *b) {
    pick_t p={.x=x,.y=y};
    if(!book_layout_visit_page(page,body,px,pick,&p)||!p.found||p.at>=s->length)return false;
    range(p.at,a,b);return true;
}
typedef struct {uint8_t *fb;EpdRect clip;} underline_t;
static void underline(void *ctx,size_t a,size_t b,EpdRect r) {
    underline_t *paint=ctx;uint8_t *fb=paint->fb;bool selected=s->mode&&a<s->end&&b>s->start;
    bool saved=false;
    for(uint32_t i=0;i<s->annotations.count;i++)if(a<s->annotations.items[i].end&&b>s->annotations.items[i].start){saved=true;break;}
    if(selected||saved){
        r.y+=r.height-2;r.height=selected?3:2;
        int right=r.x+r.width;
        if(r.x<paint->clip.x)r.x=paint->clip.x;
        if(right>paint->clip.x+paint->clip.width)right=paint->clip.x+paint->clip.width;
        r.width=right-r.x;
        if(r.y+r.height>paint->clip.y+paint->clip.height)r.y=paint->clip.y+paint->clip.height-r.height;
        if(r.width>0&&r.y>=paint->clip.y)epd_fill_rect(r,selected?0x00:0x60,fb);
    }
}
/* Page text by explicit bounded UTF-8 lines, avoiding an unbounded text-box layout.
 * A NULL framebuffer counts every line instead of drawing one page.
 * Wrap at the last space so Vietnamese words stay whole; a spaceless row (CJK, long token) breaks per glyph. */
static unsigned definition_draw(uint8_t *fb,const char *t,int x,int y) {
    unsigned page_lines=s->mode==FOOTNOTE?s->footnote_page_lines:DEFINITION_LINES;
    size_t at=0;unsigned line=0,first=(unsigned)s->definition_page*page_lines;
    while(t[at]&&(!fb||line<first+page_lines)) {
        char row[192];size_t used=0,cut_used=0,cut_at=0;int width=0;
        while(t[at]&&t[at]!='\n'&&used+5<sizeof(row)) {
            size_t end=ui_text_edit_next(t,at),n=end-at;char glyph[5]={0};
            if(n>4)return line;
            memcpy(glyph,t+at,n);
            int w=ttf_text_width_px(DEFINITION_PX,glyph);
            if(width+w>560&&used) {
                if(t[at]==' ')at=end;
                else if(cut_used){used=cut_used;at=cut_at;}
                break;
            }
            if(t[at]==' '&&used){cut_used=used;cut_at=end;}
            memcpy(row+used,t+at,n);used+=n;at=end;width+=w;
        }
        row[used]=0;if(t[at]=='\n')++at;
        if(fb&&line>=first)ui_text_box(fb,(EpdRect){x,y+(int)(line-first)*DEFINITION_LINE_H,560,DEFINITION_LINE_H},DEFINITION_PX,row,EPD_DRAW_ALIGN_LEFT,false);
        ++line;
    }
    return line;
}
/* ---- 脚注弹窗 / Footnote popup ---- */
static bool footnote_paged(void) {return s->footnote_lines>s->footnote_page_lines;}
// 引用表按正文顺序；二分找包含该字节的链接。/ References are in text order; binary-search the one containing the byte.
static size_t link_find(size_t at) {
    size_t lo=0,hi=s->link_count;
    while(lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        if(s->links[mid].end<=at)lo=mid+1;
        else if(s->links[mid].start>at)hi=mid;
        else return mid;
    }
    return SIZE_MAX;
}
// 纯文字[1]没有href时也消费点击并说明缺失，不能误当翻页。
// Consume taps on plain [1] markers too, explaining missing links instead of turning pages.
static bool reference_marker(size_t start,size_t *end) {
    if(start>=s->length||s->text[start]!='[')return false;
    size_t at=start+1;
    while(at<s->length&&at-start<=6&&s->text[at]>='0'&&s->text[at]<='9')++at;
    if(at==start+1||at>=s->length||s->text[at]!=']')return false;
    *end=at+1;return true;
}
static bool page_has_reference_marker(size_t first,size_t end) {
    size_t at=first>7?first-7:0;
    for(;at<end&&at<s->length;++at){size_t last;if(reference_marker(at,&last)&&last>first)return true;}
    return false;
}
typedef struct {int x,y,best,glyph_y;size_t link,start,end;bool on_body_text;} link_pick_t;
static void link_pick(void *ctx,size_t a,size_t b,EpdRect r) {
    (void)b;link_pick_t *p=ctx;
    size_t i=link_find(a);
    size_t start=SIZE_MAX,end=0;
    if(i==SIZE_MAX) {
        for(size_t back=0;back<=7&&back<=a;++back) {
            if(reference_marker(a-back,&end)&&a<end){start=a-back;break;}
        }
        if(start==SIZE_MAX){if(hit(r,p->x,p->y))p->on_body_text=true;return;}
    } else {start=s->links[i].start;end=s->links[i].end;}
    int dx=p->x<r.x?r.x-p->x:p->x>=r.x+r.width?p->x-r.x-r.width+1:0;
    int dy=p->y<r.y?r.y-p->y:p->y>=r.y+r.height?p->y-r.y-r.height+1:0;
    if(dx>FOOTNOTE_SLOP||dy>FOOTNOTE_SLOP)return;
    int d=dx*dx+dy*dy;
    if(p->start==SIZE_MAX||d<p->best){p->best=d;p->link=i;p->start=start;p->end=end;p->glyph_y=r.y;}
}
// 弹窗避开被点的引用：引用在上半屏就放下方，否则放上方；高度随行数收缩。
// Keep the popup clear of the tapped reference: below it in the upper half, above otherwise; height follows the lines.
static bool footnote_open(size_t page,EpdRect body,int px,int x,int y) {
    if(!s->footnote_load||book_layout_vertical()||body.height<172)return false;
    const size_t first=book_layout_page_start_offset(page);
    const size_t end=page+1<book_layout_page_count()?book_layout_page_start_offset(page+1):s->length;
    size_t lo=0,hi=s->link_count;
    while(lo<hi) {
        const size_t mid=lo+(hi-lo)/2;
        if(s->links[mid].end<=first)lo=mid+1;
        else hi=mid;
    }
    // Chương có chú thích ở trang khác không cần duyệt lại chữ của trang đang chạm.
    if((lo==s->link_count||s->links[lo].start>=end)&&!page_has_reference_marker(first,end))return false;
    link_pick_t p={.x=x,.y=y,.link=SIZE_MAX,.start=SIZE_MAX};
    if(!book_layout_visit_page(page,body,px,link_pick,&p)||p.start==SIZE_MAX)return false;
    // Dung sai chỉ áp dụng ở khoảng trống, không lấn vào chữ bên cạnh.
    if(p.best!=0&&p.on_body_text)return false;
    char *text=NULL;size_t length=0;
    s->message[0]=0;
    if(p.link==SIZE_MAX) {
        snprintf(s->message,sizeof(s->message),"Ký hiệu này không có liên kết chú thích trong sách.");
    } else if(s->footnote_load(s->links[p.link].href,&text,&length)!=ESP_OK||!text||!length){
        free(text);text=NULL;
        snprintf(s->message,sizeof(s->message),"Không tải được chú thích. Đóng và chạm lại để thử.");
    }
    free(s->footnote);s->footnote=text;
    s->definition_page=0;
    s->footnote_lines=definition_draw(NULL,text?text:s->message,0,0);
    s->footnote_page_lines=(unsigned)(body.height-16-112)/DEFINITION_LINE_H;
    if(s->footnote_page_lines>DEFINITION_LINES)s->footnote_page_lines=DEFINITION_LINES;
    unsigned lines=s->footnote_lines<s->footnote_page_lines?s->footnote_lines:s->footnote_page_lines;
    if(!lines)lines=1;
    int height=24+(int)lines*DEFINITION_LINE_H+16+FOOTNOTE_BUTTON_H+14;
    int top=p.glyph_y<body.y+body.height/2?body.y+body.height-height-8:body.y+8;
    int left=body.width>684?body.x+(body.width-612)/2:36;
    s->footnote_panel=(EpdRect){left,top,612,height};
    s->start=p.start;s->end=p.end;
    s->mode=FOOTNOTE;s->page=page;s->generation=book_layout_generation();
    return true;
}
static void footnote_draw(uint8_t *fb) {
    EpdRect p=s->footnote_panel;
    ui_fill_round_rect(fb,p,16,0xff);
    ui_draw_control_frame(fb,p,16,0x00);
    definition_draw(fb,s->footnote?s->footnote:s->message,p.x+26,p.y+24);
    char hint[128];
    if(footnote_paged())snprintf(hint,sizeof(hint),"%d/%u · Vuốt/âm lượng: lật · Chạm: đóng",s->definition_page+1,
        (s->footnote_lines+s->footnote_page_lines-1)/s->footnote_page_lines);
    else snprintf(hint,sizeof(hint),"Chạm bất kỳ đâu để đóng");
    ui_text_box(fb,(EpdRect){p.x+20,p.y+p.height-72,p.width-40,58},20,hint,EPD_DRAW_ALIGN_CENTER,false);
}
bool book_notes_volume_key(int direction,app_redraw_t *redraw) {
    if(!book_notes_active())return false;
    *redraw=APP_REDRAW_NONE;
    if(direction!=-1&&direction!=1)return true;
    if(s->mode==DICTS) {
        int next=s->dict_list_at+direction*DICT_ROWS;
        if(next>=0&&next<(int)s->dicts.count){s->dict_list_at=next;*redraw=APP_REDRAW_PAGE;}
    } else if(s->mode==LIST&&s->list_book) {
        int next=s->list_at+direction*BOOK_ROWS;
        if(s->book&&s->book->state==BOOK_NOTES_BOOK_DONE&&next>=0&&next<(int)s->book->count){s->list_at=next;*redraw=APP_REDRAW_PAGE;}
    } else if(s->mode==LIST) {
        int next=s->list_at+direction*NOTES_LIST_ROWS;
        if(next>=0&&next<(int)s->annotations.count){s->list_at=next;*redraw=APP_REDRAW_PAGE;}
    } else if(s->mode==FOOTNOTE) {
        unsigned pages=(s->footnote_lines+s->footnote_page_lines-1)/s->footnote_page_lines;
        int next=s->definition_page+direction;
        if(next>=0&&(unsigned)next<pages){s->definition_page=next;*redraw=APP_REDRAW_PAGE;}
    }
    return true;
}
/* Preview only: retain the full annotation in storage and the editor. */
static void note_preview(const char *text,char out[128]) {
    size_t at=0,used=0;unsigned words=0;
    while(text[at]&&words<6) {
        while(text[at]&&((unsigned char)text[at]<=32))++at;
        if(!text[at])break;
        if(used)out[used++]=' ';
        while(text[at]&&(unsigned char)text[at]>32) {
            size_t end=ui_text_edit_next(text,at),n=end-at;
            if(used+n>120)goto done;
            memcpy(out+used,text+at,n);used+=n;at=end;
        }
        ++words;
    }
    while(text[at]&&(unsigned char)text[at]<=32)++at;
done:
    if(text[at]){memcpy(out+used,"…",3);used+=3;}
    out[used]=0;
}
static const EpdRect dict_manage_rect={470,722,166,56};
static const EpdRect list_tab_rect={232,844,216,58};
static const EpdRect book_export_rect={414,256,224,52};
static EpdRect book_row_rect(int row) {return (EpdRect){42,BOOK_ROW_Y+row*NOTES_LIST_ROW_H,596,72};}
static void book_list_draw(uint8_t *fb) {
    book_notes_book_t *b=s->book;char header[96];
    bool done=b&&b->state==BOOK_NOTES_BOOK_DONE;
    if(!b||b->state==BOOK_NOTES_BOOK_ERROR)snprintf(header,sizeof(header),"Không đọc được ghi chú cả cuốn");
    else if(!done)snprintf(header,sizeof(header),"Đang đọc ghi chú cả cuốn…");
    else snprintf(header,sizeof(header),"Cả cuốn · %u ghi chú%s",b->count,b->errors?" (có chương lỗi)":"");
    ui_text_box(fb,(EpdRect){42,256,364,52},25,header,EPD_DRAW_ALIGN_LEFT,false);
    if(done&&b->count)button(fb,book_export_rect,"Xuất ra thẻ");
    if(!done)return;
    for(int row=0;row<BOOK_ROWS;row++) {
        int i=s->list_at+row;if(i>=(int)b->count)break;
        const book_note_ref_t *r=&b->items[i];
        char quote[128],note[128],line[160];note_preview(r->quote,quote);note_preview(r->note,note);
        snprintf(line,sizeof(line),"Ch.%u · %s",(unsigned)r->chapter+1,quote);
        EpdRect at=book_row_rect(row);
        ui_text_box(fb,(EpdRect){at.x,at.y,at.width,36},27,line,EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){at.x,at.y+36,at.width,36},25,note,EPD_DRAW_ALIGN_LEFT,false);
    }
}
static EpdRect suggestion_rect(unsigned i) {return (EpdRect){62,SUGGEST_ROW_Y+(int)i*SUGGEST_ROW_H,560,54};}
static void dict_toggle(uint8_t *fb,EpdRect r,bool on) {
    ui_fill_round_rect(fb,r,12,on?0x00:0xe0);
    ui_text_box(fb,r,23,on?"Bật":"Tắt",EPD_DRAW_ALIGN_CENTER,on);
}
void book_notes_draw(uint8_t *fb,size_t page,EpdRect body,int px) {
    if(!s)return;
    underline_t paint={fb,body};
    if(s->annotations.count||s->mode)book_layout_visit_page(page,body,px,underline,&paint);
    if(!s->mode)return;
    if(s->mode==SELECT&&s->dragging)return; /* Never cover text under the held finger. */
    if(s->mode==FOOTNOTE){footnote_draw(fb);return;}
    int menu_y=s->mode==SELECT?s->menu_y:0;
    ui_fill_round_rect(fb,(EpdRect){24,menu_y,636,72},12,0xff);
    ui_draw_control_frame(fb,(EpdRect){24,menu_y,636,72},12,0x50);
    const char *labels[]={"Tra từ","Tô sáng","Ghi chú","Các ghi chú","Đóng"};
    for(int i=0;i<5;i++) {
        EpdRect r=menu_action_rect(i);
        ui_fill_round_rect(fb,r,10,0xe0);
        ui_draw_icon(fb,r.x+r.width/2,r.y+18,24,action_icon(labels[i]),0);
        ui_text_box(fb,(EpdRect){r.x+2,r.y+36,r.width-4,26},23,labels[i],EPD_DRAW_ALIGN_CENTER,false);
    }
    if(s->message[0]) {
        ui_fill_rect_fast((EpdRect){24,1176,636,38},0xff,fb);
        ui_text_box(fb,(EpdRect){32,1178,620,32},18,s->message,EPD_DRAW_ALIGN_LEFT,false);
    }
    if(s->mode==EDIT_NOTE) {
        ui_fill_rect_fast((EpdRect){24,240,636,830},0xff,fb);
        ui_text_input_draw(fb,&s->edit,(EpdRect){36,260,612,68},24,false,NULL);
        button(fb,(EpdRect){36,350,180,58},"Lưu ghi chú");
        ui_keyboard_draw(fb,560);
    } else if(s->mode==LOOKUP) {
        ui_fill_round_rect(fb,dictionary_panel,16,0xff);
        ui_draw_control_frame(fb,dictionary_panel,16,0x00);
        if(s->dictionary.state==4)definition_draw(fb,s->dictionary.text,62,260);
        else {
            ui_text_box(fb,(EpdRect){62,260,560,130},23,s->message,EPD_DRAW_ALIGN_LEFT,false);
            if(!s->searching)for(unsigned i=0;i<s->suggestion_count;i++)
                button_icon(fb,suggestion_rect(i),s->suggestions[i],UI_ICON_SEARCH);
        }
        ui_text_box(fb,(EpdRect){62,720,400,60},19,s->dictionary.name,EPD_DRAW_ALIGN_LEFT,false);
        button(fb,dict_manage_rect,"Quản lý");
        button(fb,(EpdRect){48,810,170,58},"Trang trước");button(fb,(EpdRect){230,810,170,58},"Trang sau");button(fb,(EpdRect){412,810,224,58},"Từ điển kế");
    } else if(s->mode==DICTS) {
        ui_fill_round_rect(fb,dictionary_panel,16,0xff);
        ui_draw_control_frame(fb,dictionary_panel,16,0x00);
        if(!s->dicts.count)ui_text_box(fb,(EpdRect){62,260,560,130},23,"Chưa có bộ StarDict nào trong /dict",EPD_DRAW_ALIGN_LEFT,false);
        for(int row=0;row<DICT_ROWS;row++) {
            int i=s->dict_list_at+row;if(i>=(int)s->dicts.count)break;
            const book_dict_entry_t *e=&s->dicts.items[i];
            int y=DICT_ROW_Y+row*DICT_ROW_H;
            ui_text_box(fb,(EpdRect){56,y,270,56},21,e->rel,EPD_DRAW_ALIGN_LEFT,false);
            dict_toggle(fb,(EpdRect){334,y,96,56},e->enabled);
            button(fb,(EpdRect){436,y,96,56},"Lên");button(fb,(EpdRect){538,y,98,56},"Xuống");
        }
        button(fb,(EpdRect){48,810,170,58},"Trước");button(fb,(EpdRect){230,810,170,58},"Sau");button(fb,(EpdRect){412,810,224,58},"Xong");
    } else if(s->mode==LIST&&s->list_book) {
        ui_fill_round_rect(fb,notes_list_panel,16,0xff);
        ui_draw_control_frame(fb,notes_list_panel,16,0x00);
        book_list_draw(fb);
        button(fb,(EpdRect){42,844,180,58},"Trước");button(fb,list_tab_rect,"Chương này");button(fb,(EpdRect){458,844,180,58},"Sau");
    } else if(s->mode==LIST) {
        ui_fill_round_rect(fb,notes_list_panel,16,0xff);
        ui_draw_control_frame(fb,notes_list_panel,16,0x00);
        for(int row=0;row<NOTES_LIST_ROWS;row++) {
            int i=s->list_at+row;if(i>=(int)s->annotations.count)break;
            book_annotation_t *a=&s->annotations.items[i];
            char quote[128],note[128];note_preview(a->quote,quote);note_preview(a->note,note);
            ui_text_box(fb,(EpdRect){42,252+row*NOTES_LIST_ROW_H,500,36},29,quote,EPD_DRAW_ALIGN_LEFT,false);
            ui_text_box(fb,(EpdRect){42,288+row*NOTES_LIST_ROW_H,500,36},27,note,EPD_DRAW_ALIGN_LEFT,false);
            button(fb,(EpdRect){560,260+row*NOTES_LIST_ROW_H,78,52},"Xóa");
        }
        button(fb,(EpdRect){42,844,180,58},"Trước");button(fb,list_tab_rect,"Cả cuốn");button(fb,(EpdRect){458,844,180,58},"Sau");
    }
}
static bool persist(void) {
    if(!s->valid){snprintf(s->message,sizeof(s->message),"Không đọc được ghi chú cũ; chưa ghi đè");return false;}
    if(!book_annotations_save(&s->annotations,s->storage_root)) {
        int error=errno;
        s->annotations=s->committed;
        snprintf(s->message,sizeof(s->message),"Không ghi được thẻ nhớ (lỗi %d). Có thể thử lại.",error);return false;
    }
    s->committed=s->annotations;
    snprintf(s->message,sizeof(s->message),"Đã lưu trên thẻ nhớ");
    return true;
}
static bool save_note(const char *note) {
    if(!s->valid)return persist();
    if(!book_annotations_put(&s->annotations,s->text,s->start,s->end,note)) {
        snprintf(s->message,sizeof(s->message),"Tối đa 32 đoạn/chương, ghi chú 127 byte");return false;
    }
    return persist();
}
// 从清单中打开下一个启用的词典；没有了返回 false。/ Open the next enabled dictionary; false when none remain.
static bool open_next_dictionary(void) {
    while(s->dict_at<s->dicts.count) {
        unsigned i=s->dict_at++;
        if(!s->dicts.items[i].enabled)continue;
        char path[512];
        if(!book_dict_list_path(&s->dicts,DICT_ROOT,i,path,sizeof(path)))continue;
        if(book_dictionary_open(&s->dictionary,path,s->query))return true;
        s->unsupported=true;
    }
    return false;
}
static bool any_enabled(void) {
    for(unsigned i=0;i<s->dicts.count;i++)if(s->dicts.items[i].enabled)return true;
    return false;
}
static void begin_lookup(void) {
    book_dictionary_close(&s->dictionary);memset(&s->dictionary,0,sizeof(s->dictionary));close_dirs();
    s->unsupported=false;s->definition_page=0;s->suggestion_count=0;s->dict_at=0;
    s->mode=LOOKUP;
    bool root=book_dict_list_load(&s->dicts,DICT_ROOT);
    s->searching=root&&any_enabled();
    snprintf(s->message,sizeof(s->message),s->searching?"Đang tra từ trên thẻ nhớ…":
             root&&s->dicts.count?"Tất cả từ điển đang tắt; mở Quản lý để bật":"Chép bộ StarDict vào /dict trên thẻ nhớ");
}
static void start_lookup(void) {
    size_t n=s->end-s->start;
    if(!n||n>=sizeof(s->query)){snprintf(s->message,sizeof(s->message),"Chọn từ hoặc cụm từ dưới 256 byte");return;}
    memcpy(s->query,s->text+s->start,n);s->query[n]=0;
    begin_lookup();
}
static bool dictionary_running(int state) {return (state>=1&&state<=3)||state>=5;}
app_redraw_t book_notes_tick(void) {
    if(s&&s->mode&&s->generation!=book_layout_generation()){book_notes_cancel();return APP_REDRAW_PAGE;}
    if(s&&s->mode==LIST&&s->list_book&&s->book&&
       (s->book->state==BOOK_NOTES_BOOK_LISTING||s->book->state==BOOK_NOTES_BOOK_LOADING)) {
        int state=book_notes_book_step(s->book);
        return state==BOOK_NOTES_BOOK_DONE||state==BOOK_NOTES_BOOK_ERROR?APP_REDRAW_PAGE:APP_REDRAW_NONE;
    }
    if(!s||s->mode!=LOOKUP||!s->searching)return APP_REDRAW_NONE;
    if(dictionary_running(s->dictionary.state)) {
        int result=book_dictionary_step(&s->dictionary);
        if(result==4){s->searching=false;snprintf(s->message,sizeof(s->message),s->dictionary.truncated?"Đã rút gọn định nghĩa dài":"Tra offline: %s",s->query);return APP_REDRAW_PAGE;}
        if(result==-1)s->unsupported=true;
        // 第一个给出相邻词的词典提供建议。/ The first dictionary with neighbours supplies suggestions.
        if(result==-2&&!s->suggestion_count) {
            s->suggestion_count=s->dictionary.suggestion_count;
            memcpy(s->suggestions,s->dictionary.suggestions,sizeof(s->suggestions));
        }
        return APP_REDRAW_NONE;
    }
    if(open_next_dictionary())return APP_REDRAW_NONE;
    close_dirs();
    if(s->suggestion_count)snprintf(s->message,sizeof(s->message),"Không tìm thấy \"%.60s\". Có thể bạn muốn:",s->query);
    else snprintf(s->message,sizeof(s->message),s->unsupported?"Không tìm thấy; kiểm tra đủ bộ .ifo, .idx/.idx.gz, .dict/.dict.dz (mhgxt, 32-bit)":"Không tìm thấy từ trong /dict");
    return APP_REDRAW_PAGE;
}
// 保存顺序后用新顺序重新查询。/ Save the order, then search again in the new order.
static void dicts_done(void) {
    bool failed=s->dicts_dirty&&!book_dict_list_save(&s->dicts,DICT_ROOT);
    s->dicts_dirty=false;
    begin_lookup();
    if(failed)snprintf(s->message,sizeof(s->message),"Không ghi được thứ tự từ điển lên thẻ nhớ");
}
static void book_list_open(void) {
    s->list_book=true;
    s->book=heap_caps_calloc(1,sizeof(*s->book),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s->book)return;
    book_notes_book_begin(s->book,s->storage_root,s->valid,s->chapter,book_annotations_text_hash(s->text,s->length));
}
static void book_list_export(void) {
    char path[600];size_t n=strlen(s->storage_root);
    // storage_root 以 /snapshots 结尾，导出写在它的上一级。/ storage_root ends in /snapshots; export one level up.
    if(n<10||strcmp(s->storage_root+n-10,"/snapshots")||
       snprintf(path,sizeof(path),"%.*s/ghi-chu.md",(int)(n-10),s->storage_root)>=(int)sizeof(path)) {
        snprintf(s->message,sizeof(s->message),"Đường dẫn ghi chú quá dài");return;
    }
    char date[16]={0};time_t now=time(NULL);struct tm tm;
    if(now>1700000000&&localtime_r(&now,&tm))strftime(date,sizeof(date),"%d/%m/%Y",&tm);
    const char *title=s_book_title[0]?s_book_title:s->book_path;
    if(book_notes_book_export(s->book,path,title,date,s_chapter_title))
        snprintf(s->message,sizeof(s->message),"Đã xuất %u ghi chú ra ghi-chu.md",s->book->count);
    else snprintf(s->message,sizeof(s->message),"Không ghi được ghi-chu.md lên thẻ nhớ (lỗi %d)",errno);
}
bool book_notes_gesture(app_ctx_t *ctx,const ui_gesture_event_t *ev,size_t page,EpdRect body,int px,app_redraw_t *redraw) {
    *redraw=APP_REDRAW_NONE;if(!s)return false;
    if(s->mode&& (page!=s->page||s->generation!=book_layout_generation()))book_notes_cancel();
    if(!s->mode) {
        if(ev->type==UI_GESTURE_TAP&&hit(body,ev->x0,ev->y0)&&footnote_open(page,body,px,ev->x0,ev->y0)){*redraw=APP_REDRAW_PAGE;return true;}
        if(ev->type!=UI_GESTURE_LONG_PRESS||!hit(body,ev->x0,ev->y0)||book_layout_vertical())return false;
        if(!select_at(page,body,px,ev->x0,ev->y0,&s->start,&s->end))return false;
        s->anchor_start=s->start;s->anchor_end=s->end;s->mode=SELECT;s->page=page;s->generation=book_layout_generation();s->dragging=true;s->message[0]=0;place_menu(page,body,px);ui_gesture_enable_held_drag(true);
        *redraw=APP_REDRAW_PAGE;return true;
    }
    if(s->mode==FOOTNOTE) {
        if(ev->type==UI_GESTURE_TAP){book_notes_cancel();*redraw=APP_REDRAW_PAGE;}
        else if(ev->type==UI_GESTURE_SWIPE_U||ev->type==UI_GESTURE_SWIPE_L)book_notes_volume_key(1,redraw);
        else if(ev->type==UI_GESTURE_SWIPE_D||ev->type==UI_GESTURE_SWIPE_R)book_notes_volume_key(-1,redraw);
        return true;
    }
    if(ev->type==UI_GESTURE_CANCEL){if(s->dragging){s->dragging=false;place_menu(page,body,px);*redraw=APP_REDRAW_PAGE;}return true;}
    if(ev->type==UI_GESTURE_MOVE&&s->dragging) {
        size_t a,b;if(select_at(page,body,px,ev->x,ev->y,&a,&b)) {
            size_t start=a<s->anchor_start?a:s->anchor_start,end=b>s->anchor_end?b:s->anchor_end;
            if(start!=s->start||end!=s->end){s->start=start;s->end=end;place_menu(page,body,px);*redraw=APP_REDRAW_PAGE;}
        }return true;
    }
    if(ev->type!=UI_GESTURE_TAP&&ev->type!=UI_GESTURE_LONG_PRESS)return true;
    int x=ev->x,y=ev->y;s->dragging=false;
    if((s->mode==LIST||s->mode==LOOKUP||s->mode==DICTS)&&ev->type==UI_GESTURE_TAP&&
       !hit(s->mode==LIST?notes_list_panel:dictionary_panel,x,y)&&
       !hit((EpdRect){24,0,636,72},x,y)) {
        book_notes_cancel();*redraw=APP_REDRAW_PAGE;return true;
    }
    if(ev->type==UI_GESTURE_TAP&&hit(menu_action_rect(4),x,y)){book_notes_cancel();*redraw=APP_REDRAW_PAGE;return true;}
    if(s->mode==EDIT_NOTE) {
        if(hit((EpdRect){36,350,180,58},x,y)) {if(!ui_keyboard_pending()&&save_note(s->draft)){ui_keyboard_end();s->mode=SELECT;place_menu(page,body,px);}}
        else {
            ui_text_input_tap(&s->edit,(EpdRect){36,260,612,68},24,false,NULL,x,y);
            if(ui_keyboard_tap(x,y,560,ctx->now_ms)==UI_KEYBOARD_DONE&&
               !ui_keyboard_pending()&&save_note(s->draft))book_notes_cancel();
        }
        *redraw=APP_REDRAW_PAGE;return true;
    }
    if(s->mode==DICTS) {
        if(ev->type!=UI_GESTURE_TAP){*redraw=APP_REDRAW_NONE;return true;}
        if(hit((EpdRect){48,810,170,58},x,y)&&s->dict_list_at>=DICT_ROWS)s->dict_list_at-=DICT_ROWS;
        else if(hit((EpdRect){230,810,170,58},x,y)&&s->dict_list_at+DICT_ROWS<(int)s->dicts.count)s->dict_list_at+=DICT_ROWS;
        else if(hit((EpdRect){412,810,224,58},x,y))dicts_done();
        else for(int row=0;row<DICT_ROWS;row++) {
            int i=s->dict_list_at+row;if(i>=(int)s->dicts.count)break;
            int y0=DICT_ROW_Y+row*DICT_ROW_H;
            if(hit((EpdRect){334,y0,96,56},x,y)){s->dicts.items[i].enabled=!s->dicts.items[i].enabled;s->dicts_dirty=true;break;}
            if(hit((EpdRect){436,y0,96,56},x,y)){if(book_dict_list_move(&s->dicts,(unsigned)i,-1)){s->dicts_dirty=true;if(i==s->dict_list_at&&s->dict_list_at)s->dict_list_at-=DICT_ROWS;}break;}
            if(hit((EpdRect){538,y0,98,56},x,y)){if(book_dict_list_move(&s->dicts,(unsigned)i,1)){s->dicts_dirty=true;if(row==DICT_ROWS-1)s->dict_list_at+=DICT_ROWS;}break;}
        }
        *redraw=APP_REDRAW_PAGE;return true;
    }
    if(ev->type==UI_GESTURE_TAP&&hit(menu_action_rect(0),x,y))start_lookup();
    else if(ev->type==UI_GESTURE_TAP&&hit(menu_action_rect(1),x,y)) {
        const char *note="";
        for(uint32_t i=0;i<s->annotations.count;i++)if(s->annotations.items[i].start==s->start&&s->annotations.items[i].end==s->end)note=s->annotations.items[i].note;
        /* put clears its destination, so copy an existing note before updating. */
        snprintf(s->draft,sizeof(s->draft),"%s",note);if(save_note(s->draft))book_notes_cancel();
    }
    else if(ev->type==UI_GESTURE_TAP&&hit(menu_action_rect(2),x,y)) {
        s->draft[0]=0;
        for(uint32_t i=0;i<s->annotations.count;i++)if(s->annotations.items[i].start==s->start&&s->annotations.items[i].end==s->end)snprintf(s->draft,sizeof(s->draft),"%s",s->annotations.items[i].note);
        book_dictionary_close(&s->dictionary);close_dirs();s->mode=EDIT_NOTE;ui_text_edit_init(&s->edit,s->draft,sizeof(s->draft));ui_keyboard_begin(&s->edit,false);
    } else if(ev->type==UI_GESTURE_TAP&&hit(menu_action_rect(3),x,y)){book_dictionary_close(&s->dictionary);close_dirs();s->mode=LIST;s->list_at=0;}
    else if(s->mode==LOOKUP) {
        if(hit((EpdRect){48,810,170,58},x,y)&&s->definition_page)s->definition_page--;
        else if(hit((EpdRect){230,810,170,58},x,y)&&s->definition_page<50)s->definition_page++;
        else if(hit((EpdRect){412,810,224,58},x,y)) {
            book_dictionary_close(&s->dictionary);s->dictionary.state=0;s->definition_page=0;
            bool more=false;for(unsigned i=s->dict_at;i<s->dicts.count;i++)if(s->dicts.items[i].enabled)more=true;
            s->searching=more;snprintf(s->message,sizeof(s->message),more?"Đang tra từ điển kế…":"Đã hết các từ điển trong /dict");
        } else if(hit(dict_manage_rect,x,y)) {
            book_dictionary_close(&s->dictionary);close_dirs();
            book_dict_list_load(&s->dicts,DICT_ROOT);s->dict_list_at=0;s->dicts_dirty=false;s->mode=DICTS;
        } else if(s->dictionary.state!=4&&!s->searching) {
            for(unsigned i=0;i<s->suggestion_count;i++)if(hit(suggestion_rect(i),x,y)) {
                // 用建议词重新查询全部词典。/ Search every dictionary again with the suggestion.
                snprintf(s->query,sizeof(s->query),"%s",s->suggestions[i]);begin_lookup();break;
            }
        }
    } else if(s->mode==LIST&&hit(list_tab_rect,x,y)) {
        if(s->list_book)book_list_free();
        else book_list_open();
        s->list_at=0;
    } else if(s->mode==LIST&&s->list_book) {
        book_notes_book_t *b=s->book;bool done=b&&b->state==BOOK_NOTES_BOOK_DONE;
        if(hit((EpdRect){42,844,180,58},x,y)&&s->list_at>=BOOK_ROWS)s->list_at-=BOOK_ROWS;
        else if(done&&hit((EpdRect){458,844,180,58},x,y)&&s->list_at+BOOK_ROWS<(int)b->count)s->list_at+=BOOK_ROWS;
        else if(done&&b->count&&hit(book_export_rect,x,y))book_list_export();
        else if(done)for(int row=0;row<BOOK_ROWS;row++) {
            int i=s->list_at+row;if(i>=(int)b->count)break;
            if(!hit(book_row_rect(row),x,y))continue;
            const book_note_ref_t *r=&b->items[i];
            if(r->chapter==s->chapter&&r->end<=s->length) {
                // 本章：与章内列表相同，选中该段。/ This chapter: select the passage as the chapter list does.
                s->start=r->start;s->end=r->end;snprintf(s->message,sizeof(s->message),"%s",r->note);
                book_list_free();s->mode=SELECT; // r 随列表释放 / r is freed with the list
                s->page=book_layout_page_for_offset(s->start);place_menu(s->page,body,px);
            } else {
                // 其他章：交给阅读页加载该章。/ Another chapter: the reader loads it.
                s->jump_pending=true;s->jump_chapter=r->chapter;s->jump_offset=r->start;
                book_list_free();s->mode=CLOSED;
            }
            break;
        }
    } else if(s->mode==LIST) {
        if(hit((EpdRect){42,844,180,58},x,y)&&s->list_at>=NOTES_LIST_ROWS)s->list_at-=NOTES_LIST_ROWS;
        else if(hit((EpdRect){458,844,180,58},x,y)&&s->list_at+NOTES_LIST_ROWS<(int)s->annotations.count)s->list_at+=NOTES_LIST_ROWS;
        else for(int row=0;row<NOTES_LIST_ROWS;row++) {
            int i=s->list_at+row;if(i>=(int)s->annotations.count)break;
            if(hit((EpdRect){560,260+row*NOTES_LIST_ROW_H,78,52},x,y)&&s->valid) {
                memmove(&s->annotations.items[i],&s->annotations.items[i+1],(s->annotations.count-(unsigned)i-1)*sizeof(s->annotations.items[0]));--s->annotations.count;persist();break;
            }
            if(hit((EpdRect){42,252+row*NOTES_LIST_ROW_H,500,72},x,y)) {
                s->start=s->annotations.items[i].start;s->end=s->annotations.items[i].end;s->mode=SELECT;
                s->page=book_layout_page_for_offset(s->start);place_menu(s->page,body,px);
                snprintf(s->message,sizeof(s->message),"%s",s->annotations.items[i].note);break;
            }
        }
    } else if(hit(body,x,y)) {
        size_t a,b;if(select_at(page,body,px,x,y,&a,&b)) {
            if(ev->type==UI_GESTURE_LONG_PRESS){s->anchor_start=a;s->anchor_end=b;s->dragging=true;ui_gesture_enable_held_drag(true);}
            s->start=a;s->end=b;place_menu(page,body,px);
        }
    }
    *redraw=APP_REDRAW_PAGE;return true;
}
