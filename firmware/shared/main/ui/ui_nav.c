/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * User revision 2026-11-02: optional simple UI uses Home/Settings and a bounded book-file picker, bypassing the shelf and recent-cover work.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：绘制与命中四栏导航；细线图标分别表示首页、书架、文件和设置，图形来自 Lucide。
 * English: Draw and hit-test the four-tab navigation with thin-line home, books, files and settings icons from Lucide.
 * 用户增加点击反馈：共享返回键和底栏图标登记局部命中，主循环只在按下和复位推该控件。
 * User adds click feedback: register local hits for shared backs and navigation icons; the loop presents only that control on press and restoration.
 * 用户修订：所有模式把内容扫描截止到分隔线之间的8行固定为白色留白；滚动不能把列表画进这条扫描保护带。
 * User revision: every mode keeps the eight rows between content scans and the divider blank white; scrolling must not paint list content into this guard.
 * 用户2026-11-12修订：S31底栏使用浅色磨砂悬浮胶囊和独立选中底；导航灰阶差分替代旧五行横条，内容保护带保持白色。
 * User revision 2026-11-12: S31 uses a light frosted floating capsule with an inset selected pill; bounded gray navigation replaces the five-row marker, retaining the white content guard.
 * User revision 2026-11-13: match the soft-gray reference with a white bar and rounded selected tile; retain full-footer gray presentation and hit columns.
 * 用户修订：状态栏签名按系统字体和内建界面字体的共同安全宽度截断，阅读页复用小型字宽表，不重载系统字体。
 * User revision: fit status signatures to widths safe for the system and built-in UI faces; reading reuses a small metrics table without reloading the system font.
 */
#include "ui_nav.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "app_registry.h"
#include "read_pico_pmu.h"
#include "read_pico_pmu_protocol.h"
#include "read_pico_transfer.h"
#include "ui_kit.h"
#include "settings.h"
#include "ble_page_turner.h"
#include "ui_click_feedback.h"
#if DPREADER_BOARD_METALIO47
#include "metalio_hw.h"
#else
#endif
#include "ttf_font.h"
#include "ui_font.h"

// 底栏图标盒、返回键箭头、充电闪电的边长。
// Box sizes for the bar icons, the back chevron and the charging bolt.
#define UI_NAV_TAB_ICON_PX 44
#define UI_NAV_BACK_ICON_PX 48
#define UI_NAV_BOLT_ICON_PX 26
#define UI_NAV_NETWORK_ICON_PX 28
#define UI_STATUS_TIME_PX 40
#define UI_STATUS_PERCENT_PX 40
#define UI_STATUS_SIGNATURE_PX 40
// 用户修订：状态文字与40px阅读页脚一致，保持在原56px状态带内。
// User revision: status text matches the 40px reader footer within the existing 56px band.
#define UI_STATUS_TITLE_PX 40

static bool s_status_reading;
static bool s_status_compact;
// 阅读页把状态栏中段让给书名：中段原本显示用户签名，阅读时书名更有用。
// The reader lends the status bar's middle to the book title; that slot normally shows the
// user's signature, and while reading the title is more useful.
static char s_status_title[96];
static bool s_status_title_on;
static struct {
    bool ready;
    char signature[96], face[TTF_FONT_PATH_MAX], chosen[TTF_FONT_PATH_MAX];
    uint16_t widths[96], digits[10], colon, percent, dash;
} s_status_metrics;
_Static_assert(UI_STATUS_TIME_PX == UI_STATUS_PERCENT_PX, "status numbers share one native metrics cache");
_Static_assert(sizeof(s_status_metrics) <= 1024, "status metrics never retain a second font or bitmap");

static int status_safe_width(int px, const char *text, const char *sample) {
    int active = ui_text_fixed_context_width_px(px, text, sample);
    int builtin = ui_font_has_text(text) ? ui_font_text_width_px(px, text) : 0;
    return active > builtin ? active : builtin;
}

static void status_prepare_metrics(void) {
    if (s_status_reading) return;
    const char *signature = app_settings_status_signature();
    const char *face = ttf_font_path(), *chosen = app_settings_system_font_path();
    if (s_status_metrics.ready && !strcmp(signature, s_status_metrics.signature) &&
        !strcmp(face, s_status_metrics.face) && !strcmp(chosen, s_status_metrics.chosen)) return;
    memset(&s_status_metrics, 0, sizeof(s_status_metrics));
    snprintf(s_status_metrics.signature, sizeof(s_status_metrics.signature), "%s", signature);
    snprintf(s_status_metrics.face, sizeof(s_status_metrics.face), "%s", face);
    snprintf(s_status_metrics.chosen, sizeof(s_status_metrics.chosen), "%s", chosen);
    char prefix[96] = {0};
    const size_t length = strlen(s_status_metrics.signature);
    for (size_t n = 1; n <= length; ++n) {
        prefix[n - 1] = signature[n - 1];
        if (n < length && ((unsigned char)signature[n] & 0xc0) == 0x80) continue;
        s_status_metrics.widths[n] = status_safe_width(UI_STATUS_SIGNATURE_PX, prefix, signature);
    }
    for (int digit = 0; digit < 10; ++digit) {
        char text[] = {(char)('0' + digit), 0};
        s_status_metrics.digits[digit] = status_safe_width(UI_STATUS_TIME_PX, text, "0123456789:%-");
    }
    s_status_metrics.colon = status_safe_width(UI_STATUS_TIME_PX, ":", "0123456789:%-");
    s_status_metrics.percent = status_safe_width(UI_STATUS_PERCENT_PX, "%", "0123456789:%-");
    s_status_metrics.dash = status_safe_width(UI_STATUS_PERCENT_PX, "-", "0123456789:%-");
    s_status_metrics.ready = true;
}

void ui_nav_status_font_context(bool reading, bool face_changed) {
    s_status_reading = reading;
    if (!reading && face_changed) s_status_metrics.ready = false;
    status_prepare_metrics();
}

void ui_nav_status_set_title(const char *title) {
    const char *next = title ? title : "";
    if (!strcmp(next, s_status_title)) return;
    snprintf(s_status_title, sizeof(s_status_title), "%s", next);
}

const char *ui_nav_status_title(void) { return s_status_title; }

bool ui_nav_status_title_on(void) { return s_status_title_on; }
void ui_nav_status_set_title_on(bool on) {
    s_status_title_on = on;
    if (!on) s_status_title[0] = 0;
}

static int status_number_width(const char *text) {
    if (!s_status_metrics.ready) return ui_text_fixed_width_px(UI_STATUS_TIME_PX, text);
    int width = 0;
    for (; *text; ++text) width += *text >= '0' && *text <= '9' ? s_status_metrics.digits[*text - '0']
        : *text == ':' ? s_status_metrics.colon : *text == '%' ? s_status_metrics.percent : s_status_metrics.dash;
    return width;
}

static void status_fit_signature(char *text, int width) {
    bool cached = s_status_metrics.ready && !strcmp(text, s_status_metrics.signature);
    size_t n = strlen(text);
    while (n && (cached ? s_status_metrics.widths[n]
                       : ui_text_fixed_context_width_px(UI_STATUS_SIGNATURE_PX, text, app_settings_status_signature())) > width) {
        --n;
        while (n && ((unsigned char)text[n] & 0xc0) == 0x80) --n;
        text[n] = 0;
    }
}

// 按 UTF-8 边界截到缓冲区容量内，避免把多字节字符切成半个。
// Clip to the buffer capacity on a UTF-8 boundary so a multibyte glyph is never cut in half.
static void copy_utf8_boundary(char *dst, size_t cap, const char *src) {
    if (!cap) return;
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
        while (n && ((unsigned char)src[n] & 0xc0) == 0x80) --n;
    }
    memcpy(dst, src, n);
    dst[n] = 0;
}

// 书名截短：保持整行字体来源稳定，末字留省略号位置。
// Trim a title while keeping the face sample stable and reserving room for an ellipsis.
static void fit_status_title(char *text, int width) {
    if (!text || width <= 0) { if (text) text[0] = 0; return; }
    const int px = UI_STATUS_TITLE_PX;
    size_t len = strlen(text);
    bool trimmed = false;
    while (len && ui_text_fixed_context_width_px(px, text, text) > width) {
        do { --len; } while (len && ((unsigned char)text[len] & 0xc0) == 0x80);
        text[len] = 0;
        trimmed = true;
    }
    if (!trimmed || len < 2) return;
    while (len && ui_text_fixed_width_px(px, text) + ui_text_fixed_width_px(px, "…") > width) {
        do { --len; } while (len && ((unsigned char)text[len] & 0xc0) == 0x80);
        text[len] = 0;
    }
    if (len && len + sizeof("…") <= 96) strcat(text, "…");
}

static const char *const s_nav_labels[] = {"Trang chủ", "Kệ sách", "Tệp tin", "Cài đặt"};
static int s_power_signature=-1000;
static int power_signature(void) {
    const pmu_snapshot_t *pmu=read_pico_pmu_get();
    read_pico_transfer_status_t network={0};read_pico_transfer_get_status(&network);
    bool wifi=network.state!=READ_PICO_TRANSFER_STOPPED||read_pico_transfer_network_background_busy();
    return (pmu_battery_percent(pmu)+1)*8+(pmu_battery_charging(pmu)?4:0)+(wifi?2:0)+(ble_pt_running()?1:0);
}

// 状态栏按旋转后的屏宽排布：竖屏仍是684，阅读横屏时右侧图标贴右缘。
// Lay out the status strip on the rotated width: 684 in portrait; in reader landscape the right group hugs the right edge.
static int status_width(void) {
    int width = epd_rotated_display_width();
    return width > UI_LOCK_WIDTH ? width : UI_LOCK_WIDTH;
}
EpdRect ui_nav_status_area(void) {
    return s_status_compact ? (EpdRect){0,0,status_width(),UI_NAV_READER_STATUS_HEIGHT}
                            : (EpdRect){0,12,status_width(),56};
}
static void draw_status(uint8_t *fb, bool compact);

bool ui_nav_refresh_power(uint8_t *fb) {
    if (!ui_status_visible(fb) || s_power_signature==power_signature()) return false;
    epd_fill_rect(ui_nav_status_area(),UI_GRAY_WHITE,fb);
    draw_status(fb, s_status_compact);
    return true;
}

void ui_nav_back(uint8_t *fb, int x, int y) {
    // User revision2026-11-13: plain left arrow, retaining the original hit area.
    ui_click_feedback_register(fb,(EpdRect){x-10,y+3,64,64},UI_CLICK_BACK_SMALL,UI_ICON_ARROW_LEFT);
    ui_draw_icon(fb,x+22,y+35,UI_NAV_BACK_ICON_PX,UI_ICON_ARROW_LEFT,0x30);
}

void ui_nav_wifi_icon(uint8_t *fb, int cx, int cy, int size, uint8_t gray) {
    if (!fb || size < 16 || size > 40) return;
    ui_draw_icon(fb, cx, cy, size, UI_ICON_WIFI, gray);
}

#if !DPREADER_BOARD_METALIO47
static void icon(uint8_t *fb, int tab, int cx, int y) {
    // 四个底栏图标固定用 Lucide 细线原图，线宽与状态栏一致；y 是图标盒顶边。
    // The four bar icons stay on upstream Lucide line art at the status-bar stroke weight; y is the box top.
    static const ui_icon_t tabs[4] = {
        UI_ICON_HOUSE, UI_ICON_LIBRARY_BIG,
        UI_ICON_FOLDER_OPEN, UI_ICON_SLIDERS_HORIZONTAL,
    };
    if (tab < 0 || tab >= 4) return;
    ui_click_feedback_register(fb, (EpdRect){cx - 30, y - 12, 60, 60}, UI_CLICK_NAV, tabs[tab]);
    ui_draw_icon(fb, cx, y + 18, UI_NAV_TAB_ICON_PX, tabs[tab], UI_GRAY_BLACK);
}
#endif

#if DPREADER_BOARD_METALIO47
/* ---- S31 胶囊底栏 / S31 capsule navigation ---- */
// 用户2026-11-17修订：左侧描边胶囊放首页/书架/文件，选中项为带图标和文字的深色药丸；设置为右侧独立圆键。
// User revision 2026-11-17: an outlined capsule holds Home/Shelf/Files with the selection as a dark icon+label pill; Settings is a separate round button.
// 左右与页面卡片同为36列；上下与卡片底(1080)及屏幕下沿各留22行。
// Sides align with the 36-column page cards; 22 rows separate the bar from the card bottom (1080) and the screen edge.
#define NAV_BAR_Y (UI_NAV_TOP + 6)
#define NAV_BAR_H 92
#define NAV_MARGIN 36
#define NAV_GAP 14
#define NAV_BORDER 3
#define NAV_INSET 7
#define NAV_GEAR_X (UI_LOCK_WIDTH - NAV_MARGIN - NAV_BAR_H)
#define NAV_CAPSULE_W (NAV_GEAR_X - NAV_GAP - NAV_MARGIN)
#define NAV_ICON_PX 40
#define NAV_GEAR_ICON_PX 44
#define NAV_LABEL_PX 30
// 选中药丸内：左内边距、图标、图标与文字间距、右内边距。/ Selected pill: left pad, icon, icon-label gap, right pad.
#define NAV_PILL_PAD_L 26
#define NAV_PILL_GAP 12
#define NAV_PILL_PAD_R 30
#define NAV_IDLE_MIN_W 110

typedef struct { EpdRect rect; int tab; } nav_slot_t;
// 上次绘制的选中项与药丸宽度；命中与点击反馈沿用同一几何。/ Last painted selection and pill width; hits and feedback reuse that geometry.
static int s_nav_active = -1, s_nav_wide = 0;

static const char *nav_label(int tab) {
    return app_settings_simple_ui() && tab == 0 ? "Đọc tiếp" : s_nav_labels[tab];
}
static EpdRect nav_inner(void) {
    return (EpdRect){NAV_MARGIN + NAV_INSET, NAV_BAR_Y + NAV_INSET,
                     NAV_CAPSULE_W - 2 * NAV_INSET, NAV_BAR_H - 2 * NAV_INSET};
}
static int nav_wide_width(int active) {
    if (active < 0 || active > 2 || app_settings_simple_ui()) return 0;
    const int inner = nav_inner().width;
    int width = NAV_PILL_PAD_L + NAV_ICON_PX + NAV_PILL_GAP +
                ui_text_fixed_width_px(NAV_LABEL_PX, nav_label(active)) + NAV_PILL_PAD_R;
    const int max = inner - 2 * NAV_IDLE_MIN_W;
    return width < 200 ? 200 : width > max ? max : width;
}
static int nav_slots(nav_slot_t slots[4]) {
    const EpdRect inner = nav_inner();
    int n = 0;
    if (app_settings_simple_ui()) {
        slots[n++] = (nav_slot_t){inner, 0};
    } else {
        const int wide = s_nav_active >= 0 && s_nav_active <= 2 ? s_nav_wide : 0;
        const int idle = (inner.width - wide) / (wide ? 2 : 3);
        int x = inner.x;
        for (int tab = 0; tab < 3; ++tab) {
            int width = tab == 2 ? inner.x + inner.width - x : tab == s_nav_active && wide ? wide : idle;
            slots[n++] = (nav_slot_t){{x, inner.y, width, inner.height}, tab};
            x += width;
        }
    }
    slots[n++] = (nav_slot_t){{NAV_GEAR_X, NAV_BAR_Y, NAV_BAR_H, NAV_BAR_H}, 3};
    return n;
}
// 图标中心：选中药丸及精简首页靠左排文字，其余居中。/ Icon centre: left-aligned beside a label, otherwise centred.
static int nav_icon_cx(const nav_slot_t *slot, bool labelled) {
    if (!labelled) return slot->rect.x + slot->rect.width / 2;
    if (slot->tab == s_nav_active && !app_settings_simple_ui())
        return slot->rect.x + NAV_PILL_PAD_L + NAV_ICON_PX / 2;
    const int group = NAV_ICON_PX + NAV_PILL_GAP + ui_text_fixed_width_px(NAV_LABEL_PX, nav_label(slot->tab));
    return slot->rect.x + (slot->rect.width - group) / 2 + NAV_ICON_PX / 2;
}

EpdRect ui_nav_tab_rect(int tab) {
    nav_slot_t slots[4];
    const int n = nav_slots(slots);
    for (int i = 0; i < n; ++i) {
        if (slots[i].tab != tab) continue;
        const bool labelled = tab != 3 && (tab == s_nav_active || app_settings_simple_ui());
        const int cx = tab == 3 ? slots[i].rect.x + slots[i].rect.width / 2 : nav_icon_cx(&slots[i], labelled);
        return (EpdRect){cx - 30, NAV_BAR_Y + NAV_BAR_H / 2 - 30, 60, 60};
    }
    return (EpdRect){0};
}

void ui_nav_draw(uint8_t *fb, int active) {
    static const ui_icon_t tabs[4] = {UI_ICON_HOUSE, UI_ICON_LIBRARY_BIG, UI_ICON_FOLDER, UI_ICON_SETTINGS};
    s_nav_active = active;
    s_nav_wide = nav_wide_width(active);
    ui_fill_rect_fast((EpdRect){0,UI_NAV_REFRESH_END,UI_LOCK_WIDTH,UI_LOCK_HEIGHT-UI_NAV_REFRESH_END},UI_GRAY_WHITE,fb);
    // 深色外框用实心填充再内缩填白，保证黑白快刷后线宽一致。/ Fill dark then inset white so the frame survives BW refresh at a constant weight.
    const EpdRect capsule = {NAV_MARGIN, NAV_BAR_Y, NAV_CAPSULE_W, NAV_BAR_H};
    ui_fill_round_rect(fb, capsule, NAV_BAR_H / 2, UI_GRAY_BLACK);
    ui_fill_round_rect(fb, ui_inset_rect(capsule, NAV_BORDER), NAV_BAR_H / 2 - NAV_BORDER, UI_GRAY_WHITE);
    nav_slot_t slots[4];
    const int n = nav_slots(slots);
    const int cy = NAV_BAR_Y + NAV_BAR_H / 2;
    for (int i = 0; i < n; ++i) {
        const nav_slot_t *slot = &slots[i];
        const bool selected = slot->tab == active;
        if (slot->tab == 3) {
            ui_fill_round_rect(fb, slot->rect, NAV_BAR_H / 2, UI_GRAY_BLACK);
            if (!selected)
                ui_fill_round_rect(fb, ui_inset_rect(slot->rect, NAV_BORDER), NAV_BAR_H / 2 - NAV_BORDER, UI_GRAY_WHITE);
        } else if (selected) {
            ui_fill_round_rect(fb, slot->rect, slot->rect.height / 2, UI_GRAY_BLACK);
        }
        const bool labelled = slot->tab != 3 && (selected || app_settings_simple_ui());
        const int cx = slot->tab == 3 ? slot->rect.x + slot->rect.width / 2 : nav_icon_cx(slot, labelled);
        const int px = slot->tab == 3 ? NAV_GEAR_ICON_PX : NAV_ICON_PX;
        ui_click_feedback_register(fb, (EpdRect){cx - 30, cy - 30, 60, 60}, UI_CLICK_NAV, tabs[slot->tab]);
        ui_draw_icon(fb, cx, cy, px, tabs[slot->tab], selected ? UI_GRAY_WHITE : UI_GRAY_BLACK);
        if (labelled)
            ui_text_fixed_vc(fb, cx + NAV_ICON_PX / 2 + NAV_PILL_GAP, cy, NAV_LABEL_PX,
                             nav_label(slot->tab), EPD_DRAW_ALIGN_LEFT, selected);
    }
}
#else
EpdRect ui_nav_tab_rect(int tab) {
    if (tab < 0 || tab > 3) return (EpdRect){0};
    const int center = app_settings_simple_ui() ? (tab == 0 ? UI_LOCK_WIDTH / 4 : UI_LOCK_WIDTH * 3 / 4)
                                                : (tab * 2 + 1) * UI_LOCK_WIDTH / 8;
    return (EpdRect){center - 30, UI_NAV_TOP + 15, 60, 60};
}

void ui_nav_draw(uint8_t *fb, int active) {
    ui_fill_rect_fast((EpdRect){0, UI_NAV_TOP, UI_LOCK_WIDTH, UI_LOCK_HEIGHT - UI_NAV_TOP}, 0xe0, fb);
    ui_hairline(fb, UI_NAV_TOP, 0, UI_LOCK_WIDTH, UI_GRAY_LIGHT);
    ui_fill_rect_fast((EpdRect){0, UI_NAV_REFRESH_END, UI_LOCK_WIDTH, UI_NAV_TOP - UI_NAV_REFRESH_END}, 0xf0, fb);
    if (app_settings_main_refresh_mode() != APP_MAIN_REFRESH_NORMAL) {
        // 快刷内容边界预留空白；横条在白底上单独短刷，图标和文字保持固定。
        // Reserve a blank fast-refresh boundary; short-update the marker on white while retaining icons and text.
        ui_fill_rect_fast((EpdRect){0, UI_NAV_MARKER_TOP, UI_LOCK_WIDTH, UI_NAV_MARKER_BOTTOM - UI_NAV_MARKER_TOP}, UI_GRAY_WHITE, fb);
    }
    int count = app_settings_simple_ui() ? 2 : 4;
    for (int slot = 0; slot < count; ++slot) {
        int i = count == 2 ? (slot ? 3 : 0) : slot;
        int center = (slot * 2 + 1) * UI_LOCK_WIDTH / (count * 2);
        if (i == active) ui_fill_rect_fast((EpdRect){center - 28, UI_NAV_TOP + 1, 56, 5}, UI_GRAY_BLACK, fb);
        icon(fb, i, center, UI_NAV_TOP + 27);
        ui_text(fb, center, UI_NAV_TOP + 74, 17, app_settings_simple_ui() && i == 0 ? "Đọc tiếp" : s_nav_labels[i], EPD_DRAW_ALIGN_CENTER, false);
    }
}
#endif

static time_t status_time(void) {
    time_t now = time(NULL);
    const pmu_snapshot_t *pmu = read_pico_pmu_get();
    if (now < 1704067200 && pmu && pmu->time_synced && pmu->unix_sec >= 1704067200)
        now = pmu->unix_sec;
    return now;
}
int64_t ui_nav_clock_minute(void) {
    time_t now = status_time();
    return now >= 1704067200 ? (int64_t)now / 60 : -1;
}
static void clock_text(char *clock, size_t capacity) {
    time_t now = status_time();
    snprintf(clock,capacity,"--:--");
    if (now > 1704067200) {
        time_t vn_time = now + 7 * 3600;
        struct tm date;
        if (gmtime_r(&vn_time, &date)) snprintf(clock, capacity, "%02d:%02d", date.tm_hour, date.tm_min);
    }
}
void ui_nav_clock_draw(uint8_t *fb) {
    char clock[12]; clock_text(clock,sizeof(clock));
    ui_text_fixed_vc(fb,36,40,UI_STATUS_TIME_PX,clock,EPD_DRAW_ALIGN_LEFT,false);
}
static void draw_status(uint8_t *fb, bool compact) {
    s_status_compact = compact;
    const int screen_w = status_width(), right = screen_w - UI_LOCK_WIDTH;
    status_prepare_metrics();
    const pmu_snapshot_t *pmu = read_pico_pmu_get();
    char clock[12]; clock_text(clock,sizeof(clock));
    const int status_center_y = compact ? UI_NAV_READER_STATUS_HEIGHT / 2 : 40;
    // 固定状态栏字号，让系统字体缩放时文字仍与图标共用中心线。
    // Keep status type fixed so text and glyphs share one centerline at every system font scale.
    ui_text_fixed_vc(fb, 36, status_center_y, UI_STATUS_TIME_PX, clock, EPD_DRAW_ALIGN_LEFT, false);
    read_pico_transfer_status_t network = {0};
    read_pico_transfer_get_status(&network);
    const bool wifi_visible = network.state!=READ_PICO_TRANSFER_STOPPED||read_pico_transfer_network_background_busy();
    const bool bluetooth_visible = ble_pt_running();
    char percent[16] = "--%";
    int gauge_percent = pmu_battery_percent(pmu);
    if (gauge_percent >= 0) snprintf(percent, sizeof(percent), "%u%%", (unsigned)gauge_percent);
    // 按实际百分比宽度排列网络图标，留 10 像素间隔；墨色与电池相同。
    // Place the network marks 10 px before the measured percentage, using the battery ink.
    const int percent_left = right + 595 - status_number_width(percent);
    const int network_right = percent_left - 10;
    const int wifi_center = network_right - UI_NAV_NETWORK_ICON_PX / 2;
    const int bluetooth_center = wifi_center - (wifi_visible ? UI_NAV_NETWORK_ICON_PX + 8 : 0);
    int signature_width = 280;
    const int clock_right = 36 + status_number_width(clock);
    int available = 2 * (screen_w / 2 - clock_right - 12);
    if (available < signature_width) signature_width = available > 0 ? available : 0;
    int group_left = percent_left;
    if (wifi_visible || bluetooth_visible) {
        group_left = (bluetooth_visible ? bluetooth_center : wifi_center) - UI_NAV_NETWORK_ICON_PX / 2;
    }
    available = 2 * (group_left - 12 - screen_w / 2);
    if (available < signature_width) signature_width = available > 0 ? available : 0;
    char signature[96];
    // 阅读页优先显示书名：书名通常比用户签名长，因此单独按固定字号截短，不套用签名的字宽表。
    // The reader prefers the book title. Titles run longer than signatures, so they are trimmed
    // separately at a fixed size instead of reusing the signature's width table.
    const bool show_title = s_status_title_on && s_status_title[0];
    if (show_title) {
        copy_utf8_boundary(signature, sizeof(signature), s_status_title);
        fit_status_title(signature, signature_width);
        if (signature[0])
            ui_text_fixed_vc(fb, screen_w / 2, status_center_y, UI_STATUS_TITLE_PX,
                             signature, EPD_DRAW_ALIGN_CENTER, false);
    } else {
        snprintf(signature, sizeof(signature), "%s", app_settings_status_signature());
        status_fit_signature(signature, signature_width);
        if (signature[0]) ui_text_fixed_vc(fb, screen_w / 2, status_center_y, UI_STATUS_SIGNATURE_PX,
                                          signature, EPD_DRAW_ALIGN_CENTER, false);
    }
    if (wifi_visible) ui_nav_wifi_icon(fb, wifi_center, status_center_y, UI_NAV_NETWORK_ICON_PX, UI_GRAY_BLACK);
    if (bluetooth_visible)
        ui_draw_icon(fb, bluetooth_center, status_center_y, UI_NAV_NETWORK_ICON_PX,
                     UI_ICON_BLUETOOTH, UI_GRAY_BLACK);
    EpdRect battery = {right + 606, status_center_y - 11, 40, 22};
    ui_draw_round_rect(fb, battery, 6, UI_GRAY_BLACK);
    ui_fill_round_rect(fb, (EpdRect){right + 648, status_center_y - 4, 5, 8}, 2, UI_GRAY_BLACK);
    bool charging = pmu_battery_charging(pmu);
    if (gauge_percent >= 0) {
        int width = (31 * gauge_percent + 50) / 100;
        if (width > 0) ui_fill_round_rect(fb, (EpdRect){right + 610, status_center_y - 7, width, 14}, 4, UI_GRAY_BLACK);
    }
    if (charging) {
        // 固定黑底与白色闪电描边，0%时也有完整对比，不依赖电量填充宽度。
        // A fixed black well and white bolt outline retain contrast even at0%, independent of the charge fill.
        ui_fill_round_rect(fb,(EpdRect){right+614,status_center_y-10,26,20},3,UI_GRAY_BLACK);
        ui_draw_icon(fb, right + 627, status_center_y, 22, UI_ICON_ZAP, UI_GRAY_WHITE);

    }
    ui_text_fixed_vc(fb, right + 595, status_center_y, UI_STATUS_PERCENT_PX, percent, EPD_DRAW_ALIGN_RIGHT, false);
    ui_status_mark_visible(fb);
    s_power_signature=(gauge_percent+1)*8+(charging?4:0)+(wifi_visible?2:0)+(bluetooth_visible?1:0);
}

// 快捷层保留当前状态栏位置及隐藏状态。/ Quick overlays retain the current status position and hidden state.
void ui_nav_redraw_status(uint8_t *fb) {
    if(!ui_status_visible(fb))return;
    epd_fill_rect(ui_nav_status_area(),UI_GRAY_WHITE,fb);
    draw_status(fb,s_status_compact);
}

void ui_nav_status(uint8_t *fb) { draw_status(fb, false); }
void ui_nav_reader_status(uint8_t *fb) { draw_status(fb, true); }

int ui_nav_hit(uint16_t x, uint16_t y) {
    if (x >= UI_LOCK_WIDTH || y < UI_NAV_TOUCH_TOP || y >= UI_LOCK_HEIGHT) return -1;
#if DPREADER_BOARD_METALIO47
    // 整列命中：胶囊内各段延伸到屏幕左缘，最后一段与设置圆键在间隙中点分界。
    // Full-height columns: capsule slots reach the left edge, and the last one meets the round Settings key mid-gap.
    nav_slot_t slots[4];
    const int n = nav_slots(slots);
    for (int i = 0; i < n - 1; ++i) {
        int right = slots[i].rect.x + slots[i].rect.width;
        if (i == n - 2) right += (NAV_GEAR_X - right) / 2;
        if (x < right) return slots[i].tab;
    }
    return 3;
#else
    if (app_settings_simple_ui()) return x < UI_LOCK_WIDTH / 2 ? 0 : 3;
    int tab = x * 4 / UI_LOCK_WIDTH;
    return tab < 4 ? tab : -1;
#endif
}

void ui_nav_request(app_ctx_t *ctx, int index) {
    if (index >= 0 && index < 4) ctx->request_app = app_at(app_settings_simple_ui() && index == 2 ? 5 : index);
}
