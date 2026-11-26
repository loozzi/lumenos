/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：Full 的“Từ vựng”页：从 /vocab 选词汇包，翻卡学习（正面单词，点按或中键翻面看释义），左/右键或按钮回答
 * “Quên/Nhớ”，按 Leitner 五盒安排复习。每轮结束、每 20 张、离页与锁屏前保存进度，并更新“今日单词”摘要。
 * English: Full's "Từ vựng" page: pick a pack in /vocab and study flash cards (front shows the word; a tap or the
 * middle key flips to the meaning); answer "Quên/Nhớ" with the left/right keys or buttons, scheduled by five
 * Leitner boxes. Progress is saved at the end of a session, every 20 cards, on exit and before lock, and the
 * "word of the day" summary is refreshed.
 *
 * 冻结：不内置词汇数据；绘制不读卡；Love Book 不注册本页。
 * Frozen: no built-in vocabulary data; drawing never touches the card; Love Book does not register this page.
 */
#include "app.h"
#include "app_registry.h"
#include "app_content_open.h"
#include "ui_gesture.h"
#include "ui_kit.h"
#include "ui_menu.h"
#include "ui_nav.h"
#include "ui_font.h"
#include "vocab_pack.h"
#include "vocab_study.h"
#include "vocab_today.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#ifndef VOCAB_ROOT
#define VOCAB_ROOT "/sdcard/vocab"
#endif
#define VOCAB_STATE_ROOT VOCAB_ROOT "/.lumen"
#define VOCAB_PACKS 16

enum { VIEW_MAIN, VIEW_CARD, VIEW_DONE };
static int s_view;
static char s_packs[VOCAB_PACKS][96];
static unsigned s_pack_count, s_pack_at;
static vocab_pack_t s_pack;
static vocab_progress_t s_progress;
static vocab_session_t *s_session;
static bool s_loaded, s_flipped, s_dirty;
static unsigned s_answers_since_save;
static uint32_t s_today;
static char s_message[128];

/* ---- 布局 / Layout ---- */
static const EpdRect k_back = {26, 82, 64, 64};
static const EpdRect k_pack = {36, 210, 612, 150};
static const EpdRect k_study = {36, 390, 612, 96};
static const EpdRect k_new_per_day = {36, 520, 612, 76};
static const EpdRect k_home = {36, 612, 612, 76};
static const EpdRect k_lock = {36, 704, 612, 76};
static const EpdRect k_card = {36, 200, 612, 700};
static const EpdRect k_forgot = {36, 930, 296, 96};
static const EpdRect k_remember = {352, 930, 296, 96};

static char s_requested[96];
static int64_t now_unix(void) { return (int64_t)time(NULL); }

bool app_vocab_request_open(const char *path) {
    const size_t root = strlen(VOCAB_ROOT);
    if (strncmp(path, VOCAB_ROOT "/", root + 1) || strchr(path + root + 1, '/')) return false;
    return snprintf(s_requested, sizeof(s_requested), "%s", path + root + 1) < (int)sizeof(s_requested);
}

/* ---- 文字换行 / Wrapping ---- */
// 按空格换行，最多 lines 行；超出时最后一行以“…”结束。/ Wrap at spaces into at most `lines` rows, ending with "…" on overflow.
static int wrap_draw(uint8_t *fb, int x, int y, int width, int px, int line_h, int lines, const char *text, bool draw) {
    int used = 0;
    const int real_px = ui_text_effective_px(px);
    while (*text && used < lines) {
        while (*text == ' ') text++;
        char row[192];
        size_t n = 0, cut = 0;
        const char *rest = text;
        while (rest[n] && n + 4 < sizeof(row)) {
            size_t step = 1;
            while (((unsigned char)rest[n + step] & 0xc0) == 0x80) step++;
            memcpy(row, rest, n + step);
            row[n + step] = 0;
            if (ui_text_fixed_width_px(real_px, row) > width && n) break;
            n += step;
            if (rest[n] == ' ' || !rest[n]) cut = n;
        }
        if (rest[n] && cut) n = cut;
        memcpy(row, rest, n);
        row[n] = 0;
        text = rest + n;
        if (used + 1 == lines && *text) {
            size_t m = n;
            while (m && ui_text_fixed_width_px(real_px, row) + ui_text_fixed_width_px(real_px, "…") > width) {
                m--;
                while (m && ((unsigned char)row[m] & 0xc0) == 0x80) m--;
                row[m] = 0;
            }
            snprintf(row + m, sizeof(row) - m, "…");
        }
        if (draw) ui_text_box(fb, (EpdRect){x, y + used * line_h, width, line_h}, px, row, EPD_DRAW_ALIGN_LEFT, false);
        used++;
    }
    return used;
}

static void button(uint8_t *fb, EpdRect r, const char *label, bool dark) {
    ui_fill_round_rect(fb, r, 20, dark ? UI_GRAY_BLACK : 0xf0);
    if (!dark) ui_draw_round_rect(fb, r, 20, 0x38);
    ui_text_box(fb, (EpdRect){r.x + 16, r.y, r.width - 32, r.height}, 28, label, EPD_DRAW_ALIGN_CENTER, dark);
}

static void row(uint8_t *fb, EpdRect r, const char *label, const char *value) {
    ui_fill_round_rect(fb, r, 18, 0xf0);
    ui_text_box(fb, (EpdRect){r.x + 24, r.y, 400, r.height}, 26, label, EPD_DRAW_ALIGN_LEFT, false);
    ui_text_box(fb, (EpdRect){r.x + 400, r.y, r.width - 424, r.height}, 26, value, EPD_DRAW_ALIGN_RIGHT, false);
}

/* ---- 数据 / Data ---- */
static void scan_packs(void) {
    s_pack_count = 0;
    DIR *dir = opendir(VOCAB_ROOT);
    if (!dir) return;
    struct dirent *e;
    while ((e = readdir(dir)) && s_pack_count < VOCAB_PACKS) {
        size_t n = strlen(e->d_name);
        if (e->d_name[0] == '.' || n < 5 || n >= sizeof(s_packs[0]) || strcasecmp(e->d_name + n - 4, ".csv")) continue;
        memcpy(s_packs[s_pack_count++], e->d_name, n + 1); // n 已小于缓冲 / n already fits
    }
    closedir(dir);
    // 名字排序，顺序稳定。/ Sort by name for a stable order.
    for (unsigned i = 1; i < s_pack_count; i++)
        for (unsigned j = i; j && strcmp(s_packs[j - 1], s_packs[j]) > 0; j--) {
            char t[sizeof(s_packs[0])];
            memcpy(t, s_packs[j], sizeof(t));
            memcpy(s_packs[j], s_packs[j - 1], sizeof(t));
            memcpy(s_packs[j - 1], t, sizeof(t));
        }
}

static void remember_active(void) {
    mkdir(VOCAB_ROOT, 0775);
    mkdir(VOCAB_STATE_ROOT, 0775);
    FILE *f = fopen(VOCAB_STATE_ROOT "/active.txt", "w");
    if (f) { fprintf(f, "%s\n", s_packs[s_pack_at]); fclose(f); }
}

static void pick_active(void) {
    s_pack_at = 0;
    FILE *f = fopen(VOCAB_STATE_ROOT "/active.txt", "r");
    if (!f) return;
    char name[128] = {0};
    if (fgets(name, sizeof(name), f)) {
        name[strcspn(name, "\r\n")] = 0;
        for (unsigned i = 0; i < s_pack_count; i++) if (!strcmp(s_packs[i], name)) s_pack_at = i;
    }
    fclose(f);
}

static void refresh_today(void) {
    vocab_today_t t = {.show_home = s_progress.show_home, .show_lock = s_progress.show_lock};
    int w = s_loaded ? vocab_featured(&s_progress, &s_pack, s_today) : -1;
    if (w >= 0) {
        t.valid = true;
        snprintf(t.english, sizeof(t.english), "%s", vocab_text(&s_pack, (unsigned)w, VOCAB_ENGLISH));
        snprintf(t.type, sizeof(t.type), "%s", vocab_text(&s_pack, (unsigned)w, VOCAB_TYPE));
        snprintf(t.vietnamese, sizeof(t.vietnamese), "%s", vocab_text(&s_pack, (unsigned)w, VOCAB_VIETNAMESE));
    }
    mkdir(VOCAB_STATE_ROOT, 0775);
    (void)vocab_today_set(VOCAB_STATE_ROOT, &t);
}

static void save(void) {
    if (!s_loaded || !s_dirty) return;
    if (vocab_progress_save(&s_progress, VOCAB_STATE_ROOT, s_packs[s_pack_at])) s_dirty = false;
    else snprintf(s_message, sizeof(s_message), "Không ghi được tiến độ lên thẻ nhớ");
    s_answers_since_save = 0;
    refresh_today();
}

static void unload(void) {
    save();
    vocab_pack_free(&s_pack);
    vocab_progress_free(&s_progress);
    free(s_session);
    s_session = NULL;
    s_loaded = false;
}

static void load_pack(void) {
    unload();
    s_message[0] = 0;
    if (!s_pack_count) return;
    char path[200];
    snprintf(path, sizeof(path), "%s/%s", VOCAB_ROOT, s_packs[s_pack_at]);
    if (!vocab_pack_load(&s_pack, path)) {
        snprintf(s_message, sizeof(s_message), "%.48s: %.72s", s_packs[s_pack_at], s_pack.error);
        return;
    }
    if (!vocab_progress_load(&s_progress, VOCAB_STATE_ROOT, s_packs[s_pack_at]))
        snprintf(s_message, sizeof(s_message), "Tệp tiến độ hỏng; bắt đầu lại từ đầu");
    s_loaded = true;
    s_today = vocab_today(&s_progress, now_unix());
    if (s_pack.skipped || s_pack.limited)
        snprintf(s_message, sizeof(s_message), "Bỏ qua %u dòng lỗi/trùng%s", s_pack.skipped, s_pack.limited ? "; chỉ dùng 4000 từ đầu" : "");
    refresh_today();
}

/* ---- 绘制 / Drawing ---- */
static void header(uint8_t *fb, const char *title) {
    ui_clear_page(fb);
    ui_nav_status(fb);
    ui_nav_back(fb, 58, 117);
    ui_text_vc(fb, 342, 117, 36, title, EPD_DRAW_ALIGN_CENTER, false);
}

static void render_main(uint8_t *fb) {
    header(fb, "Từ vựng");
    if (!s_pack_count) {
        wrap_draw(fb, 48, 220, 588, 26, 44, 8,
                  "Chép tệp CSV vào thư mục /vocab trên thẻ nhớ. Dòng đầu là tên cột; bắt buộc có english và "
                  "vietnamese, có thể thêm type, pronounce, explain, example, example_vietnamese.", true);
        ui_nav_draw(fb, 0);
        return;
    }
    ui_fill_round_rect(fb, k_pack, 24, 0xf0);
    char line[160];
    snprintf(line, sizeof(line), "%s", s_packs[s_pack_at]);
    ui_text_box(fb, (EpdRect){60, 222, 420, 48}, 28, line, EPD_DRAW_ALIGN_LEFT, false);
    if (s_pack_count > 1) ui_text_box(fb, (EpdRect){480, 222, 144, 48}, 24, "Đổi bộ ›", EPD_DRAW_ALIGN_RIGHT, false);
    unsigned learned = 0, due = 0, fresh = 0;
    if (s_loaded) {
        vocab_counts(&s_progress, &s_pack, s_today, &learned, &due, &fresh);
        snprintf(line, sizeof(line), "%u từ · đã học %u · đến hạn %u · mới %u", s_pack.count, learned, due, fresh);
    } else snprintf(line, sizeof(line), "Không đọc được bộ từ này");
    ui_text_box(fb, (EpdRect){60, 290, 564, 44}, 24, line, EPD_DRAW_ALIGN_LEFT, false);
    if (s_loaded) {
        unsigned budget = s_progress.new_per_day > s_progress.new_today ? s_progress.new_per_day - s_progress.new_today : 0;
        unsigned today = due + (fresh < budget ? fresh : budget);
        if (today) { snprintf(line, sizeof(line), "Học %u thẻ hôm nay", today); button(fb, k_study, line, true); }
        else button(fb, k_study, "Hôm nay đã học xong", false);
        snprintf(line, sizeof(line), "%u", s_progress.new_per_day);
        row(fb, k_new_per_day, "Từ mới mỗi ngày", line);
        row(fb, k_home, "Cú Lém nhắc từ trên Trang chủ", s_progress.show_home ? "Bật" : "Tắt");
        row(fb, k_lock, "Hiện từ trên màn khóa", s_progress.show_lock ? "Bật" : "Tắt");
    }
    if (s_message[0]) wrap_draw(fb, 48, 810, 588, 22, 36, 3, s_message, true);
    ui_nav_draw(fb, 0);
}

static void render_card(uint8_t *fb) {
    int w = s_session ? vocab_current(s_session) : -1;
    char title[48];
    snprintf(title, sizeof(title), "Thẻ %u/%u", s_session ? s_session->at + 1 : 0, s_session ? s_session->count : 0);
    header(fb, title);
    if (w < 0) return;
    ui_fill_round_rect(fb, k_card, 24, 0xf8);
    ui_draw_round_rect(fb, k_card, 24, 0x38);
    const char *english = vocab_text(&s_pack, (unsigned)w, VOCAB_ENGLISH);
    ui_text_box(fb, (EpdRect){60, 236, 564, 80}, 48, english, EPD_DRAW_ALIGN_CENTER, false);
    char meta[96];
    const char *type = vocab_text(&s_pack, (unsigned)w, VOCAB_TYPE), *ipa = vocab_text(&s_pack, (unsigned)w, VOCAB_PRONOUNCE);
    // 字体缺音标字形时不画音标。/ Skip IPA the UI font cannot draw.
    bool show_ipa = ipa[0] && ui_font_has_text(ipa);
    snprintf(meta, sizeof(meta), "%s%s%s", type, type[0] && show_ipa ? "  " : "", show_ipa ? ipa : "");
    if (meta[0]) ui_text_box(fb, (EpdRect){60, 320, 564, 44}, 26, meta, EPD_DRAW_ALIGN_CENTER, false);
    if (!s_flipped) {
        ui_text_box(fb, (EpdRect){60, 820, 564, 48}, 22, "Chạm thẻ hoặc phím giữa để xem nghĩa", EPD_DRAW_ALIGN_CENTER, false);
    } else {
        ui_fill_rect_fast((EpdRect){84, 384, 516, 2}, 0x80, fb);
        int y = 404;
        y += 46 * wrap_draw(fb, 60, y, 564, 32, 46, 3, vocab_text(&s_pack, (unsigned)w, VOCAB_VIETNAMESE), true) + 14;
        const char *explain = vocab_text(&s_pack, (unsigned)w, VOCAB_EXPLAIN);
        if (explain[0]) y += 38 * wrap_draw(fb, 60, y, 564, 24, 38, 3, explain, true) + 14;
        const char *example = vocab_text(&s_pack, (unsigned)w, VOCAB_EXAMPLE);
        if (example[0]) y += 38 * wrap_draw(fb, 60, y, 564, 24, 38, 3, example, true) + 6;
        const char *example_vi = vocab_text(&s_pack, (unsigned)w, VOCAB_EXAMPLE_VI);
        if (example_vi[0] && y < 860) wrap_draw(fb, 60, y, 564, 22, 36, (900 - y) / 36, example_vi, true);
        button(fb, k_forgot, "Quên", false);
        button(fb, k_remember, "Nhớ", true);
    }
    ui_nav_draw(fb, 0);
}

static void render_done(uint8_t *fb) {
    header(fb, "Xong phần hôm nay");
    char line[96];
    snprintf(line, sizeof(line), "Nhớ %u · Quên %u", s_session ? s_session->remembered : 0, s_session ? s_session->forgotten : 0);
    ui_text_box(fb, (EpdRect){48, 300, 588, 60}, 32, line, EPD_DRAW_ALIGN_CENTER, false);
    if (s_message[0]) wrap_draw(fb, 48, 400, 588, 22, 36, 3, s_message, true);
    button(fb, k_study, "Về Từ vựng", true);
    ui_nav_draw(fb, 0);
}

static void render(app_ctx_t *ctx, uint8_t *fb) {
    (void)ctx;
    if (s_view == VIEW_CARD) render_card(fb);
    else if (s_view == VIEW_DONE) render_done(fb);
    else render_main(fb);
}

/* ---- 操作 / Actions ---- */
static app_redraw_t start_session(void) {
    if (!s_loaded) return APP_REDRAW_NONE;
    if (!s_session) s_session = calloc(1, sizeof(*s_session));
    if (!s_session) { snprintf(s_message, sizeof(s_message), "Không đủ bộ nhớ"); return APP_REDRAW_PAGE; }
    s_today = vocab_today(&s_progress, now_unix());
    vocab_session_begin(s_session, &s_progress, &s_pack, s_today);
    if (!s_session->count) return APP_REDRAW_PAGE;
    s_flipped = false;
    s_view = VIEW_CARD;
    return APP_REDRAW_PAGE;
}

static app_redraw_t answer(bool remembered) {
    if (s_view != VIEW_CARD || !s_flipped || !s_session) return APP_REDRAW_NONE;
    int next = vocab_answer(s_session, &s_progress, &s_pack, remembered, s_today);
    s_dirty = true;
    if (++s_answers_since_save >= 20) save();
    s_flipped = false;
    if (next < 0) { s_message[0] = 0; save(); s_view = VIEW_DONE; }
    return APP_REDRAW_PAGE;
}

static app_redraw_t back(app_ctx_t *ctx) {
    if (s_view != VIEW_MAIN) { save(); s_view = VIEW_MAIN; return APP_REDRAW_PAGE; }
    ctx->request_app = app_home_page();
    return APP_REDRAW_NONE;
}

static app_redraw_t vocab_gesture(app_ctx_t *ctx, const ui_gesture_event_t *ev) {
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    const int x = ev->x0, y = ev->y0;
    int tab = ui_nav_hit(x, y);
    if (tab >= 0) { ui_nav_request(ctx, tab); return APP_REDRAW_NONE; }
    if (ui_rect_hit(k_back, x, y)) return back(ctx);
    if (s_view == VIEW_CARD) {
        if (s_flipped && ui_rect_hit(k_forgot, x, y)) return answer(false);
        if (s_flipped && ui_rect_hit(k_remember, x, y)) return answer(true);
        if (ui_rect_hit(k_card, x, y)) { s_flipped = !s_flipped; return APP_REDRAW_PAGE; }
        return APP_REDRAW_NONE;
    }
    if (s_view == VIEW_DONE) {
        if (ui_rect_hit(k_study, x, y)) { s_view = VIEW_MAIN; return APP_REDRAW_PAGE; }
        return APP_REDRAW_NONE;
    }
    if (ui_rect_hit(k_pack, x, y) && s_pack_count > 1) {
        save();
        s_pack_at = (s_pack_at + 1) % s_pack_count;
        remember_active();
        load_pack();
        return APP_REDRAW_PAGE;
    }
    if (!s_loaded) return APP_REDRAW_NONE;
    if (ui_rect_hit(k_study, x, y)) return start_session();
    if (ui_rect_hit(k_new_per_day, x, y)) {
        s_progress.new_per_day = s_progress.new_per_day == 5 ? 10 : s_progress.new_per_day == 10 ? 20 : 5;
        s_dirty = true; save(); return APP_REDRAW_PAGE;
    }
    if (ui_rect_hit(k_home, x, y)) { s_progress.show_home = !s_progress.show_home; s_dirty = true; save(); return APP_REDRAW_PAGE; }
    if (ui_rect_hit(k_lock, x, y)) { s_progress.show_lock = !s_progress.show_lock; s_dirty = true; save(); return APP_REDRAW_PAGE; }
    return APP_REDRAW_NONE;
}

static app_redraw_t vocab_key_event(app_ctx_t *ctx, int key) {
    if (s_view == VIEW_CARD) {
        if (key == UI_KEY_2) { s_flipped = !s_flipped; return APP_REDRAW_PAGE; }
        if (!s_flipped) { s_flipped = true; return APP_REDRAW_PAGE; }
        return answer(key == UI_KEY_3);
    }
    if (s_view == VIEW_MAIN && key == UI_KEY_3 && s_loaded) return start_session();
    return back(ctx);
}

static void vocab_enter(app_ctx_t *ctx) {
    (void)ctx;
    s_view = VIEW_MAIN;
    scan_packs();
    pick_active();
    // 从文件页打开的包优先并记为当前包。/ A pack opened from Files wins and becomes the active one.
    if (s_requested[0]) {
        for (unsigned i = 0; i < s_pack_count; i++) if (!strcmp(s_packs[i], s_requested)) { s_pack_at = i; remember_active(); }
        s_requested[0] = 0;
    }
    load_pack();
}

static void vocab_exit(app_ctx_t *ctx) { (void)ctx; unload(); }
static void before_lock(app_ctx_t *ctx) { (void)ctx; save(); }
static void media_lost(app_ctx_t *ctx) {
    (void)ctx;
    // 卡已不在：放弃未保存的部分，不再写卡。/ The card is gone: drop unsaved work and never write.
    s_dirty = false;
    vocab_pack_free(&s_pack);
    vocab_progress_free(&s_progress);
    free(s_session);
    s_session = NULL;
    s_loaded = false;
    s_pack_count = 0;
    s_view = VIEW_MAIN;
    vocab_today_invalidate();
}
static void media_ready(app_ctx_t *ctx) { vocab_today_invalidate(); vocab_enter(ctx); }
static int64_t next_tick_ms(app_ctx_t *ctx) { return ctx->now_ms + 1000; }

const app_desc_t app_vocab = {
    .title = "Từ vựng", .detail = "Học từ bằng thẻ", .owns_keys = true, .persistent_navigation = true,
    .on_enter = vocab_enter, .on_exit = vocab_exit, .on_before_lock = before_lock,
    .on_media_lost = media_lost, .on_media_ready = media_ready,
    .render = render, .on_gesture = vocab_gesture, .on_key = vocab_key_event, .next_tick_ms = next_tick_ms,
};
