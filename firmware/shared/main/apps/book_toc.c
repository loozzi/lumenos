/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：目录只呈现章节导航，和书架列表、旧示例菜单完全分离。
 * English: The directory only navigates chapters and shares no shelf or demo-menu rendering.
 * User revision2026-11-14: white backing,flat gray chapter/paging cards and darker current selection; retain all navigation/jump geometry.
 * 用户修复：书名与章名统一使用当前系统字体的标题路径；原生字号、整行字体来源和灰阶抗锯齿保持一致，不放大补充点阵。
 * User fix: book/chapter names share the active system title path with native sizes, stable whole-line font selection and gray antialiasing, without enlarged bitmap supplements.
 */
#include "book_toc.h"

#include <stdio.h>
#include <string.h>
#include "book_source.h"
#include "ui_kit.h"
#include "ui_nav.h"
#if defined(ESP_PLATFORM)
#include "esp_attr.h"
#define TOC_STATE_RAM EXT_RAM_BSS_ATTR
#else
#define TOC_STATE_RAM
#endif

enum { TOC_LEFT = 36, TOC_RIGHT = 648, TOC_ROW_TOP = 252,
       TOC_ROW_STEP = 108, TOC_ROW_HEIGHT = 94 };

/* ---- 折叠 / Collapse ---- */
// 位图按目录项记录“已折叠”；s_total 不符时视为全部展开。
// One collapsed bit per entry; a mismatched s_total means everything is expanded.
#define TOC_MAX_ENTRIES 8192
static TOC_STATE_RAM uint8_t s_collapsed[TOC_MAX_ENTRIES / 8];
static size_t s_total;

static bool toc_active(size_t chapters) { return chapters && chapters == s_total; }
static bool is_collapsed(size_t i) { return i < TOC_MAX_ENTRIES && (s_collapsed[i >> 3] >> (i & 7)) & 1; }
static void set_collapsed(size_t i, bool on) {
    if (i >= TOC_MAX_ENTRIES) return;
    if (on) s_collapsed[i >> 3] |= (uint8_t)(1u << (i & 7));
    else s_collapsed[i >> 3] &= (uint8_t)~(1u << (i & 7));
}
static bool has_children(size_t chapters, size_t i) {
    return i + 1 < chapters && book_navigation_level(i + 1) > book_navigation_level(i);
}
static size_t child_count(size_t chapters, size_t i) {
    uint8_t level = book_navigation_level(i);
    size_t n = 0;
    while (i + 1 + n < chapters && book_navigation_level(i + 1 + n) > level) ++n;
    return n;
}

// 顺序遍历：visit 返回 false 时停止；返回值为可见行数。
// Walk visible rows in order; stop when visit returns false; returns the rows walked.
typedef bool (*toc_visit_t)(size_t row, size_t entry, void *arg);
static size_t toc_walk(size_t chapters, toc_visit_t visit, void *arg) {
    if (!toc_active(chapters)) {
        for (size_t i = 0; i < chapters; ++i) if (visit && !visit(i, i, arg)) return i + 1;
        return chapters;
    }
    size_t row = 0;
    bool hiding = false;
    uint8_t hide_level = 0;
    for (size_t i = 0; i < chapters; ++i) {
        uint8_t level = book_navigation_level(i);
        if (hiding && level > hide_level) continue;
        hiding = false;
        if (visit && !visit(row, i, arg)) return row + 1;
        ++row;
        if (is_collapsed(i) && has_children(chapters, i)) { hiding = true; hide_level = level; }
    }
    return row;
}

void book_toc_view_open(size_t chapters, size_t current) {
    memset(s_collapsed, 0, sizeof(s_collapsed));
    s_total = chapters > TOC_MAX_ENTRIES ? 0 : chapters;
    if (!s_total) return;
    for (size_t i = 0; i < chapters; ++i) if (has_children(chapters, i)) set_collapsed(i, true);
    if (current >= chapters) return;
    // 展开当前项的全部上级。/ Expand every ancestor of the current entry.
    uint8_t level = book_navigation_level(current);
    for (size_t i = current; i-- > 0 && level;) {
        uint8_t l = book_navigation_level(i);
        if (l < level) { set_collapsed(i, false); level = l; }
    }
}

void book_toc_toggle(size_t chapters, size_t entry) {
    if (!toc_active(chapters) || !has_children(chapters, entry)) return;
    set_collapsed(entry, !is_collapsed(entry));
}

size_t book_toc_visible_count(size_t chapters) { return toc_walk(chapters, NULL, NULL); }

typedef struct { size_t entry, row, best; } toc_find_t;
static bool find_visit(size_t row, size_t entry, void *arg) {
    toc_find_t *f = arg;
    if (entry > f->entry) return false;
    f->best = row;
    return true;
}
static size_t row_of(size_t chapters, size_t entry) {
    toc_find_t f = {entry, 0, 0};
    toc_walk(chapters, find_visit, &f);
    return f.best;
}
int book_toc_page_of(size_t chapters, size_t entry) {
    if (!chapters || entry == SIZE_MAX) return 0;
    return (int)(row_of(chapters, entry) / BOOK_TOC_ROWS);
}

typedef struct { size_t first, count; size_t out[BOOK_TOC_ROWS]; } toc_page_t;
static bool page_visit(size_t row, size_t entry, void *arg) {
    toc_page_t *p = arg;
    if (row < p->first) return true;
    p->out[p->count++] = entry;
    return p->count < BOOK_TOC_ROWS;
}
static size_t page_entries(size_t chapters, int page, size_t out[BOOK_TOC_ROWS]) {
    toc_page_t p = {.first = (size_t)(page < 0 ? 0 : page) * BOOK_TOC_ROWS};
    toc_walk(chapters, page_visit, &p);
    memcpy(out, p.out, sizeof(p.out));
    return p.count;
}

int book_toc_pages(size_t chapters) {
    size_t rows = book_toc_visible_count(chapters);
    return rows ? (int)(1 + (rows - 1) / BOOK_TOC_ROWS) : 1;
}

EpdRect book_toc_row_rect(int row) {
    return (EpdRect){TOC_LEFT, TOC_ROW_TOP + row * TOC_ROW_STEP,
                     TOC_RIGHT - TOC_LEFT, TOC_ROW_HEIGHT};
}

// 把 EPUB/TXT 中的换行、制表符和控制符收敛为一个空格，防止绘制越过本行。
// Collapse line breaks, tabs and controls from EPUB/TXT to one space so glyphs stay in one row.
static void one_line(char *dst, size_t cap, const char *src) {
    if (!cap) return;
    size_t out = 0;
    bool gap = false;
    for (const unsigned char *p = (const unsigned char *)src; *p && out + 1 < cap;) {
        unsigned char c = *p;
        if (c <= 0x20 || c == 0x7f) { gap = out != 0; ++p; continue; }
        if (gap && out + 2 < cap) dst[out++] = ' ';
        gap = false;
        int bytes = c < 0x80 ? 1 : c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc0 ? 2 : 1;
        if (out + (size_t)bytes >= cap) break;
        for (int i = 0; i < bytes && *p; ++i) dst[out++] = (char)*p++;
    }
    while (out && dst[out - 1] == ' ') --out;
    dst[out] = 0;
}

static void fit_line(char *text, int px, int width) {
    size_t len = strlen(text);
    bool trimmed = false;
    while (len && (ui_text_fixed_width_px(px, text) > width || len > 96)) {
        do { --len; } while (len && ((unsigned char)text[len] & 0xc0) == 0x80);
        text[len] = 0;
        trimmed = true;
    }
    if (trimmed && len > 3) {
        // Cut a little more before adding the ellipsis so its glyph cannot cross the status area.
        // 添加省略号前预留字形宽度，避免跨过右侧当前章节标记。
        while (len && ui_text_fixed_width_px(px, text) + ui_text_fixed_width_px(px, "…") > width) {
            do { --len; } while (len && ((unsigned char)text[len] & 0xc0) == 0x80);
            text[len] = 0;
        }
        if (len + sizeof("…") <= 160) strcat(text, "…");
    }
}

// 用完整标题固定字体来源，截短后也不因缺字判断改变粗细。/ Keep the full title as the font sample so truncation cannot change the face or weight.
static void toc_title(uint8_t *fb, EpdRect box, int preferred_px, const char *text, uint8_t background) {
    char sample[160], shown[160];
    one_line(sample, sizeof(sample), text);
    memcpy(shown, sample, strlen(sample) + 1);
    int px = ui_text_effective_px(preferred_px);
    if (px > box.height - 2) px = box.height - 2;
    px = ui_text_title_fit(shown, px, box.width, sample);
    ui_text_title_background_vc(fb, box.x, box.y + box.height / 2, px, shown, sample,
                                EPD_DRAW_ALIGN_LEFT, background);
}

int book_toc_hit(uint16_t x, uint16_t y, int page, size_t chapters) {
    if (x < 100 && y >= 76 && y < 151) return BOOK_TOC_BACK;
    if (y >= 1018 && y < 1086) {
        if (x >= 36 && x < 136) return BOOK_TOC_SKIP_PREV;
        if (x >= 146 && x < 258) return BOOK_TOC_PREV;
        if (x >= 426 && x < 538) return BOOK_TOC_NEXT;
        if (x >= 548 && x < 648) return BOOK_TOC_SKIP_NEXT;
    }
    if (x >= 36 && x < 648 && y >= 1100 && y < 1174) return BOOK_TOC_JUMP;
    size_t entries[BOOK_TOC_ROWS];
    size_t shown = page_entries(chapters, page, entries);
    for (size_t row = 0; row < shown; ++row) {
        EpdRect r = book_toc_row_rect((int)row);
        if (!ui_rect_hit(r, x, y)) continue;
        size_t index = entries[row];
        if (toc_active(chapters) && has_children(chapters, index) &&
            x < r.x + 79 + 28 * (int)book_navigation_level(index))
            return BOOK_TOC_TOGGLE(index);
        return (int)index;
    }
    return -1;
}

int book_toc_jump_hit(uint16_t x, uint16_t y) {
    if (x >= 75 && x < 330 && y >= 850 && y < 907) return BOOK_TOC_JUMP_CANCEL;
    if (x >= 354 && x < 609 && y >= 850 && y < 907) return BOOK_TOC_JUMP_CONFIRM;
    if (x >= 75 && x < 609 && y >= 600 && y < 711) return BOOK_TOC_JUMP_TRACK;
    return -1;
}

int book_toc_jump_percent(uint16_t x) {
    if (x <= 92) return 0;
    if (x >= 592) return 100;
    return ((int)x - 92) * 100 / 500;
}

int book_toc_page_from_percent(int percent, int pages) {
    if (pages <= 1 || percent <= 0) return 0;
    if (percent >= 100) return pages - 1;
    int page = (percent * pages + 99) / 100 - 1;
    return page < pages ? page : pages - 1;
}

// Chapter rows, page buttons and the jump dialog share the current card material.
static void toc_border(uint8_t *fb,EpdRect rect,int radius,bool selected) {
    ui_draw_soft_card(fb,rect,radius,selected);
}

static void toc_pill(uint8_t *fb, EpdRect rect, const char *text, int px, bool selected) {
    toc_border(fb, rect, 20, selected);
    char label[160];
    one_line(label, sizeof(label), text);
    fit_line(label, px, rect.width - 16);
    ui_text_box(fb,ui_inset_rect(rect,8),px,label,EPD_DRAW_ALIGN_CENTER,false);
}

void book_toc_render(uint8_t *fb, const char *book_title, size_t chapters,
                     size_t current, int page, const char *message) {
    ui_clear_page(fb);
    ui_fill_rect_fast((EpdRect){0, 0, UI_LOCK_WIDTH, UI_LOCK_HEIGHT}, UI_GRAY_WHITE, fb);
    ui_nav_status(fb);
    ui_nav_back(fb, TOC_LEFT, 79);
    ui_text_box(fb,(EpdRect){120,79,444,72},34,"Mục lục",EPD_DRAW_ALIGN_CENTER,false);

    char title[160];
    one_line(title, sizeof(title), book_title ? book_title : "");
    if (!title[0]) strcpy(title, "Sách đang đọc");
    toc_title(fb, (EpdRect){TOC_LEFT,160,420,48}, 23, title, UI_GRAY_WHITE);
    char total[48];
    snprintf(total, sizeof(total), "%u mục", (unsigned)chapters);
    ui_text_box(fb,(EpdRect){482,160,166,48},23,total,EPD_DRAW_ALIGN_RIGHT,false);
    ui_fill_rect_fast((EpdRect){TOC_LEFT, 213, TOC_RIGHT - TOC_LEFT, 2}, 0x68, fb);

    if (message && *message) {
        char info[160];one_line(info,sizeof(info),message);fit_line(info,ui_text_effective_px(23),540);
        ui_text_box(fb,(EpdRect){72,408,540,72},23,info,EPD_DRAW_ALIGN_CENTER,false);
    } else if (!chapters) {
        ui_text_box(fb,(EpdRect){72,408,540,72},25,"Sách này không có mục lục",EPD_DRAW_ALIGN_CENTER,false);
    } else {
        size_t entries[BOOK_TOC_ROWS];
        size_t shown = page_entries(chapters, page, entries);
        for (size_t row = 0; row < shown; ++row) {
            size_t index = entries[row];
            size_t source_index = book_navigation_chapter(index);
            if (source_index == SIZE_MAX) break;
            EpdRect r = book_toc_row_rect((int)row);
            bool selected = index == current;
            toc_border(fb, r, 20, selected);
            int indent = 28 * (int)book_navigation_level(index);
            bool parent = toc_active(chapters) && has_children(chapters, index);
            bool folded = parent && is_collapsed(index);
            if (parent) {
                // 折叠显示“+”，展开显示“–”。/ "+" when folded, "–" when expanded.
                ui_text_box(fb,(EpdRect){r.x+14+indent,r.y+11,58,72},30,folded?"+":"–",EPD_DRAW_ALIGN_LEFT,false);
            } else {
                char number[16]; snprintf(number, sizeof(number), "%02u", (unsigned)index + 1);
                ui_text_box(fb,(EpdRect){r.x+14,r.y+11,58,72},21,number,EPD_DRAW_ALIGN_LEFT,false);
            }
            char raw[160] = "", label[160];
            if (book_navigation_title(index, raw, sizeof(raw)) != ESP_OK || !raw[0])
                snprintf(raw, sizeof(raw), "Mục %u", (unsigned)index + 1);
            char spine_default[32];
            snprintf(spine_default, sizeof(spine_default), "Mục %u", (unsigned)source_index + 1);
            if (strcmp(raw, spine_default) == 0)
                snprintf(raw, sizeof(raw), "Mục %u", (unsigned)index + 1);
            one_line(label, sizeof(label), raw);
            if (!label[0]) snprintf(label, sizeof(label), "Mục %u", (unsigned)index + 1);
            // 子目录每级缩进 28px，标题宽度相应收窄。/ Each TOC level indents 28 px and narrows the title.
            toc_title(fb, (EpdRect){r.x+79+indent,r.y+9,(selected||folded?420:495)-indent,76}, 30, label,
                       ui_soft_card_color(selected));
            if (selected)
                ui_text_box(fb,(EpdRect){r.x+r.width-93,r.y+11,71,72},24,"Hiện tại",EPD_DRAW_ALIGN_RIGHT,false);
            else if (folded) {
                char more[24]; snprintf(more, sizeof(more), "%u mục", (unsigned)child_count(chapters, index));
                ui_text_box(fb,(EpdRect){r.x+r.width-93,r.y+11,71,72},24,more,EPD_DRAW_ALIGN_RIGHT,false);
            }
        }
    }
    const int pages = book_toc_pages(chapters);
    toc_pill(fb, (EpdRect){36, 1018, 100, 68}, "« 10 tr", 24, false);
    toc_pill(fb, (EpdRect){146, 1018, 112, 68}, "‹ Trước", 26, false);
    char position[32]; snprintf(position, sizeof(position), "%02d / %02d", page + 1, pages);
    toc_pill(fb, (EpdRect){268, 1018, 148, 68}, position, 25, true);
    toc_pill(fb, (EpdRect){426, 1018, 112, 68}, "Sau ›", 26, false);
    toc_pill(fb, (EpdRect){548, 1018, 100, 68}, "10 tr »", 24, false);
    EpdRect jump = {36, 1100, 612, 74};
    toc_border(fb, jump, 21, false);
    ui_text_box(fb,(EpdRect){58,1109,388,56},28,"Chuyển theo phần trăm (%)",EPD_DRAW_ALIGN_LEFT,false);
    char percent[24];
    snprintf(percent, sizeof(percent), "%d%%  ›", (page + 1) * 100 / pages);
    ui_text_box(fb,(EpdRect){468,1109,158,56},28,percent,EPD_DRAW_ALIGN_RIGHT,false);
}

void book_toc_render_jump(uint8_t *fb, size_t chapters, int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    EpdRect card = {44, 356, 596, 572};
    ui_fill_round_rect(fb, card, 30, UI_GRAY_WHITE);
    ui_text_box(fb,(EpdRect){(342)-250,393,500,44},32,"Chuyển tới mục lục",EPD_DRAW_ALIGN_CENTER,false);
    ui_text_box(fb,(EpdRect){(342)-250,450,500,44},24,"Chọn phần trăm để xem trước vị trí",
                  EPD_DRAW_ALIGN_CENTER, false);
    char value[24]; snprintf(value, sizeof(value), "%d%%", percent);
    ui_text_fixed(fb, 342, 515, 55, value,EPD_DRAW_ALIGN_CENTER,false);
    ui_fill_round_rect(fb, (EpdRect){92, 637, 500, 8}, 4, 0x80);
    int width = 500 * percent / 100;
    if (width) ui_fill_round_rect(fb, (EpdRect){92, 637, width, 8}, 4, 0x40);
    int knob = 92 + width;
    epd_fill_circle(knob, 641, 25, UI_GRAY_WHITE, fb);
    epd_draw_circle(knob, 641, 25, 0x40, fb);
    static const char *ticks[] = {"0", "25", "50", "75", "100%"};
    for (int i = 0; i < 5; ++i)
        ui_text_box(fb,(EpdRect){(92 + i * 125)-250,678,500,44},24,ticks[i],EPD_DRAW_ALIGN_CENTER,false);
    EpdRect target_card = {75, 740, 534, 84};
    toc_border(fb, target_card, 19, false);
    ui_text_box(fb,(EpdRect){99,747,486,30},23,"Vị trí đích",EPD_DRAW_ALIGN_LEFT,false);
    int pages = book_toc_pages(chapters);
    int target = book_toc_page_from_percent(percent, pages);
    size_t entries[BOOK_TOC_ROWS];
    size_t shown = page_entries(chapters, target, entries);
    size_t first = shown ? entries[0] + 1 : 0;
    size_t last = shown ? entries[shown - 1] + 1 : 0;
    char summary[160];
    if (chapters)
        snprintf(summary, sizeof(summary), "Khoảng tr. %d / %d · Mục %u–%u",
                 target + 1, pages, (unsigned)first, (unsigned)last);
    else snprintf(summary, sizeof(summary), "Sách này không có mục lục");
    fit_line(summary, 25, 486);
    ui_text_box(fb,(EpdRect){99,783,486,34},25,summary,EPD_DRAW_ALIGN_LEFT,false);
    toc_pill(fb, (EpdRect){75, 850, 255, 57}, "Hủy", 26, false);
    ui_draw_soft_card(fb,(EpdRect){354,850,255,57},20,true);
    ui_text_box(fb,(EpdRect){362,858,239,41},26,"Chuyển",EPD_DRAW_ALIGN_CENTER,false);
}
