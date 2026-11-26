/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：Full首页宠物“Cú Lém”：矢量灰阶猫头鹰、心情、等级与周阅读条。仅由 home.inc 引入。
 * English: Full Home pet "Cú Lém": a vector gray owl, moods, level and the weekly reading strip. Included only by home.inc.
 * 冻结：只画已缓存的阅读记录，不读卡、不写设置、不做持续动画。
 * Frozen: paint cached reading records only; no card reads, settings writes or continuous animation. */
/* User 2026-11-17: an owl sits in the resume-card corner quoting the excerpt, a dedicated pet card replaces the month grid, and the month collapses into a 7-day strip. */

typedef enum { PET_SLEEP, PET_MISS, PET_WAIT, PET_READING, PET_HAPPY, PET_PROUD } pet_mood_t;

/* ---- 绘制原语 / Drawing primitives ---- */
typedef struct { uint8_t *fb; int x, y, size; bool fast; } pet_canvas_t;
static int pet_x(const pet_canvas_t *c, float v) { return c->x + lroundf(v * c->size / 120.f); }
static int pet_y(const pet_canvas_t *c, float v) { return c->y + lroundf(v * c->size / 120.f); }
static int pet_len(const pet_canvas_t *c, float v) { int n = lroundf(v * c->size / 120.f); return n < 1 ? 1 : n; }
// 快刷模式只用黑白两色，避免灰阶抖动。/ Fast refresh uses black and white only to avoid dithered grays.
static uint8_t pet_ink(const pet_canvas_t *c, uint8_t gray) { return c->fast ? (gray < 0x90 ? 0x20 : 0xff) : gray; }
static void pet_ellipse(const pet_canvas_t *c, float cx, float cy, float rx, float ry, uint8_t gray) {
    int x0 = pet_x(c, cx), y0 = pet_y(c, cy), a = pet_len(c, rx), b = pet_len(c, ry);
    for (int dy = -b; dy <= b; ++dy) {
        float t = 1.f - (float)(dy * dy) / (float)(b * b);
        int half = lroundf(a * sqrtf(t > 0 ? t : 0));
        ui_fill_rect_fast((EpdRect){x0 - half, y0 + dy, 2 * half + 1, 1}, pet_ink(c, gray), c->fb);
    }
}
static void pet_dot(const pet_canvas_t *c, float cx, float cy, float r, uint8_t gray) {
    epd_fill_circle(pet_x(c, cx), pet_y(c, cy), pet_len(c, r), pet_ink(c, gray), c->fb);
}
static void pet_tri(const pet_canvas_t *c, float ax, float ay, float bx, float by, float qx, float qy, uint8_t gray) {
    epd_fill_triangle(pet_x(c, ax), pet_y(c, ay), pet_x(c, bx), pet_y(c, by), pet_x(c, qx), pet_y(c, qy), pet_ink(c, gray), c->fb);
}
// 二次曲线粗笔：沿曲线盖圆点。/ Thick quadratic stroke stamped with dots along the curve.
static void pet_curve(const pet_canvas_t *c, float ax, float ay, float qx, float qy, float bx, float by, float width, uint8_t gray) {
    int r = lroundf(width * c->size / 240.f); if (r < 1) r = 1;
    for (int i = 0; i <= 24; ++i) {
        float t = i / 24.f, u = 1.f - t;
        float px = u * u * ax + 2 * u * t * qx + t * t * bx, py = u * u * ay + 2 * u * t * qy + t * t * by;
        epd_fill_circle(pet_x(c, px), pet_y(c, py), r, pet_ink(c, gray), c->fb);
    }
}
static void pet_line(const pet_canvas_t *c, float ax, float ay, float bx, float by, float width, uint8_t gray) {
    pet_curve(c, ax, ay, (ax + bx) / 2, (ay + by) / 2, bx, by, width, gray);
}
static void pet_sparkle(const pet_canvas_t *c, float cx, float cy, float r) {
    pet_tri(c, cx, cy - r, cx + r * .3f, cy, cx - r * .3f, cy, 0x20); pet_tri(c, cx, cy + r, cx + r * .3f, cy, cx - r * .3f, cy, 0x20);
    pet_tri(c, cx - r, cy, cx, cy - r * .3f, cx, cy + r * .3f, 0x20); pet_tri(c, cx + r, cy, cx, cy - r * .3f, cx, cy + r * .3f, 0x20);
}

/* ---- 猫头鹰 / The owl ---- */
// 设计稿坐标 120×124；size 为宽度像素。/ Artwork space is 120×124; size is the drawn width in pixels.
static void pet_owl_draw(uint8_t *fb, int x, int y, int size, pet_mood_t mood, bool wink, bool fast) {
    const pet_canvas_t c = {fb, x, y, size, fast};
    const bool happy = mood == PET_HAPPY, book = mood == PET_READING || mood == PET_PROUD || mood == PET_HAPPY;
    pet_tri(&c, 26, 32, 32, 6, 50, 24, 0x50); pet_tri(&c, 94, 32, 88, 6, 70, 24, 0x50);
    if (happy) { pet_curve(&c, 24, 78, 4, 64, 10, 42, 12, 0x48); pet_curve(&c, 96, 78, 116, 64, 110, 42, 12, 0x48); }
    else { pet_ellipse(&c, 22, 80, 9, 22, 0x48); pet_ellipse(&c, 98, 80, 9, 22, 0x48); }
    pet_ellipse(&c, 60, 72, 40, 46, 0x68);
    pet_ellipse(&c, 60, 87, 26, 28, 0xd8);
    pet_curve(&c, 47, 82, 51, 86, 55, 82, 2.5f, 0x98); pet_curve(&c, 65, 82, 69, 86, 73, 82, 2.5f, 0x98); pet_curve(&c, 55, 93, 60, 97, 65, 93, 2.5f, 0x98);
    const float ey = mood == PET_MISS ? 56 : 54;
    pet_dot(&c, 44, ey, 15, 0xff); pet_dot(&c, 76, ey, 15, 0xff);
    switch (mood) {
    case PET_SLEEP:
        pet_curve(&c, 33, 56, 44, 64, 55, 56, 3.5f, 0x10); pet_curve(&c, 65, 56, 76, 64, 87, 56, 3.5f, 0x10);
        break;
    case PET_HAPPY:
        pet_curve(&c, 34, 59, 44, 46, 54, 59, 4, 0x10); pet_curve(&c, 66, 59, 76, 46, 86, 59, 4, 0x10);
        break;
    case PET_MISS:
        pet_dot(&c, 44, 61, 6, 0x10); pet_dot(&c, 76, 61, 6, 0x10);
        pet_line(&c, 30, 48, 46, 41, 3.5f, 0x10); pet_line(&c, 90, 48, 74, 41, 3.5f, 0x10);
        pet_ellipse(&c, 88, 76, 3, 5, 0x90);
        break;
    case PET_WAIT:
        // 抬眼望向右上，像在想今天读什么。/ Glancing up-right, wondering what to read today.
        pet_dot(&c, 49, 49, 7, 0x10); pet_dot(&c, 81, 49, 7, 0x10); pet_dot(&c, 51, 46, 2.2f, 0xff); pet_dot(&c, 83, 46, 2.2f, 0xff);
        break;
    default:
        pet_dot(&c, 46, 57, 7, 0x10); pet_dot(&c, 48, 54, 2.2f, 0xff);
        if (wink) pet_curve(&c, 66, 56, 76, 62, 86, 56, 3.5f, 0x10);
        else { pet_dot(&c, 78, 57, 7, 0x10); pet_dot(&c, 80, 54, 2.2f, 0xff); }
        break;
    }
    if (mood == PET_PROUD) {
        // 学者眼镜。/ Scholar's glasses.
        for (int i = 0; i < 2; ++i) {
            int cx = pet_x(&c, i ? 76 : 44), cy = pet_y(&c, ey), r = pet_len(&c, 17);
            for (int k = 0; k < (size >= 100 ? 3 : 2); ++k) epd_draw_circle(cx, cy, r - k, pet_ink(&c, 0x10), fb);
        }
        pet_line(&c, 56, 52, 64, 52, 3, 0x10);
    }
    pet_tri(&c, 55, 64, 65, 64, 60, 73, 0x20);
    if (book) {
        pet_tri(&c, 34, 92, 60, 98, 60, 118, 0xff); pet_tri(&c, 34, 92, 60, 118, 34, 112, 0xff);
        pet_tri(&c, 86, 92, 60, 98, 60, 118, 0xff); pet_tri(&c, 86, 92, 60, 118, 86, 112, 0xff);
        pet_line(&c, 34, 92, 60, 98, 3, 0x10); pet_line(&c, 60, 98, 86, 92, 3, 0x10); pet_line(&c, 86, 92, 86, 112, 3, 0x10);
        pet_line(&c, 86, 112, 60, 118, 3, 0x10); pet_line(&c, 60, 118, 34, 112, 3, 0x10); pet_line(&c, 34, 112, 34, 92, 3, 0x10);
        pet_line(&c, 60, 98, 60, 118, 2.5f, 0x10);
    }
    pet_line(&c, 45, 117, 41, 122, 3, 0x20); pet_line(&c, 49, 117, 49, 123, 3, 0x20);
    pet_line(&c, 71, 117, 71, 123, 3, 0x20); pet_line(&c, 75, 117, 79, 122, 3, 0x20);
    if (mood == PET_SLEEP && size >= 60) {
        int px = size >= 100 ? 22 : 16;
        label(fb, pet_x(&c, 104), pet_y(&c, 22), px, "z", EPD_DRAW_ALIGN_CENTER, 0x20);
        label(fb, pet_x(&c, 114), pet_y(&c, 8), px * 3 / 4, "z", EPD_DRAW_ALIGN_CENTER, 0x20);
    }
    if (happy || mood == PET_PROUD) { pet_sparkle(&c, 108, 12, 10); if (size >= 100) pet_sparkle(&c, 12, 22, 6); }
}

/* ---- 心情与文案 / Mood and words ---- */
static unsigned pet_recent_seconds(int offset) {
    // offset 0 为今天，负数为之前的天。/ Offset 0 is today; negative values are earlier days.
    if (offset > 0 || offset < -29) return 0;
    if (s_recent_days_valid) return s_recent_days[29 + offset];
    int day = s_date.tm_mday + offset;
    return s_reading_days_valid && day >= 1 ? s_reading_days[day - 1] : 0;
}
static pet_mood_t pet_mood(void) {
    if (!s_reading_days_valid) return PET_WAIT;
    const unsigned goal = app_settings_home_goal_minutes() * 60, today = today_seconds();
    if (goal && today >= goal) return home_streak_days(goal) >= 7 ? PET_PROUD : PET_HAPPY;
    if (s_date.tm_hour >= 22 || s_date.tm_hour < 6) return PET_SLEEP;
    if (today >= 60) return PET_READING;
    if (s_recent_days_valid && !pet_recent_seconds(-1) && !pet_recent_seconds(-2) && !pet_recent_seconds(-3)) return PET_MISS;
    return PET_WAIT;
}
// 用户2026-11-25：有词汇包时，偶数次点按让 Cú Lém 说今日单词。/ With a vocabulary pack, even taps make Cú Lém say today's word.
static bool pet_vocab_showing(void) {
    const vocab_today_t *t = vocab_today_cached();
    return t->valid && t->show_home && s_pet_tap % 2 == 0;
}
static EpdRect pet_bubble_rect(void) { return (EpdRect){236, 656, 388, 136}; }
// 用户2026-11-26：卡片左下角常驻“Học từ vựng ›”入口。/ User 2026-11-26: a permanent "Học từ vựng ›" link at the card's lower left.
static EpdRect pet_vocab_link_rect(void) { return (EpdRect){40, 842, 188, 44}; }
static void pet_message(char *out, size_t cap, pet_mood_t mood) {
    if (pet_vocab_showing()) {
        char line[240];
        vocab_today_line(vocab_today_cached(), line, sizeof(line));
        snprintf(out, cap, "Từ hôm nay: %s", line);
        // 截断时退回到完整的 UTF-8 字符。/ When cut, back off to a whole UTF-8 character.
        size_t n = strlen(out);
        if (n + 1 >= cap) { while (n && ((unsigned char)out[n - 1] & 0xc0) == 0x80) n--; if (n && (unsigned char)out[n - 1] >= 0xc0) n--; out[n] = 0; }
        return;
    }
    const unsigned goal = app_settings_home_goal_minutes(), today = today_seconds() / 60;
    const unsigned left = goal > today ? goal - today : 1, streak = home_streak_days(goal * 60);
    const unsigned pick = s_pet_tap % 3;
    switch (mood) {
    case PET_SLEEP:
        if (pick == 0) snprintf(out, cap, "Khuya rồi, đọc vài trang rồi ngủ ngon nhé.");
        else if (pick == 1) snprintf(out, cap, "Zzz… cú đêm cũng cần ngủ đấy.");
        else snprintf(out, cap, "Hôm nay còn %u phút nữa là đạt mục tiêu. Mai đọc tiếp cũng được!", left);
        break;
    case PET_MISS:
        if (pick == 0) snprintf(out, cap, "Lâu rồi không thấy bạn mở sách…");
        else if (pick == 1) snprintf(out, cap, "Mình nhớ những trang sách cùng bạn lắm.");
        else snprintf(out, cap, "Chỉ 5 phút thôi, đọc với mình nhé?");
        break;
    case PET_WAIT:
        if (!s_reading_days_valid) snprintf(out, cap, "Cài ngày giờ để mình theo dõi việc đọc cùng bạn nhé.");
        else if (pick == 0) snprintf(out, cap, "Hôm nay mình đọc gì nhỉ?");
        else if (pick == 1) snprintf(out, cap, "Mục tiêu hôm nay: %u phút. Bắt đầu thôi!", goal);
        else snprintf(out, cap, "Mỗi ngày vài trang, đầu óc vui cả tuần.");
        break;
    case PET_READING:
        if (pick == 0) snprintf(out, cap, "Thêm %u phút nữa là đạt mục tiêu!", left);
        else if (pick == 1) snprintf(out, cap, "Đã đọc %u phút hôm nay, giỏi lắm!", today);
        else snprintf(out, cap, "Đoạn này hay ghê, đọc tiếp đi!");
        break;
    case PET_HAPPY:
        if (pick == 0) snprintf(out, cap, "Đạt mục tiêu rồi! Hôm nay tuyệt quá!");
        else if (pick == 1 && streak > 1) snprintf(out, cap, "Chuỗi %u ngày rồi, giữ vững nhé!", streak);
        else snprintf(out, cap, "Đọc thêm nữa là phần thưởng đấy.");
        break;
    default:
        if (pick == 0) snprintf(out, cap, "Chuỗi %u ngày! Bạn là mọt sách thật thụ.", streak);
        else if (pick == 1) snprintf(out, cap, "Mình phải đeo kính mới theo kịp bạn.");
        else snprintf(out, cap, "Đọc đều thế này, cuốn nào cũng xong!");
        break;
    }
}
static const unsigned s_pet_levels[] = {0, 30, 120, 300, 600, 1200, 2400};
static const char *const s_pet_level_names[] = {"Cú con", "Mọt sách tập sự", "Độc giả chăm chỉ", "Mọt sách", "Học giả", "Hiền triết", "Đại hiền triết"};
#define PET_LEVELS (sizeof(s_pet_levels) / sizeof(s_pet_levels[0]))
static unsigned pet_minutes_30(void) {
    unsigned sum = 0;
    for (int i = 0; i < 30; ++i) sum += pet_recent_seconds(-i);
    return sum / 60;
}
static unsigned pet_level(unsigned minutes) {
    unsigned level = 0;
    while (level + 1 < PET_LEVELS && minutes >= s_pet_levels[level + 1]) ++level;
    return level;
}

/* ---- 排版 / Layout ---- */
// 按空格贪心折行，超出行数时末行加省略号。/ Greedy space wrapping; the last line gets an ellipsis on overflow.
static unsigned pet_wrap(const char *text, int px, int width, char lines[][128], unsigned max_lines) {
    unsigned row = 0;
    const char *p = text;
    while (*p && row < max_lines) {
        while (*p == ' ') ++p;
        if (!*p) break;
        char *line = lines[row]; line[0] = 0;
        size_t used = 0;
        while (*p) {
            const char *end = p; while (*end && *end != ' ') ++end;
            char trial[128];
            int n = snprintf(trial, sizeof(trial), "%s%s%.*s", line, used ? " " : "", (int)(end - p), p);
            if (n > 0 && n < 120 && ui_text_fixed_width_px(px, trial) <= width) {
                memcpy(line, trial, (size_t)n + 1); used = (size_t)n; p = end;
                while (*p == ' ') ++p;
                continue;
            }
            if (!used) {
                // 无空格长串（如中文）逐字截断。/ Cut space-free runs (such as CJK) character by character.
                while (*p && *p != ' ' && used < 116) {
                    size_t len = (uint8_t)*p < 0x80 ? 1 : (uint8_t)*p < 0xe0 ? 2 : (uint8_t)*p < 0xf0 ? 3 : 4;
                    if (strnlen(p, len) < len) { p += strnlen(p, len); break; }
                    memcpy(line + used, p, len); line[used + len] = 0;
                    if (ui_text_fixed_width_px(px, line) > width && used) { line[used] = 0; break; }
                    used += len; p += len;
                }
            }
            break;
        }
        ++row;
        while (*p == ' ') ++p;
        if (*p && row == max_lines) {
            while (line[0] && ui_text_fixed_width_px(px, line) + ui_text_fixed_width_px(px, "…") > width) {
                size_t len = strlen(line) - 1; while (len && ((uint8_t)line[len] & 0xc0) == 0x80) --len; line[len] = 0;
            }
            strlcat(line, "…", 128);
        }
    }
    return row;
}
static void pet_bubble(uint8_t *fb, EpdRect box, int tail_y, bool tail_right) {
    ui_fill_round_rect(fb, box, 20, 0xff);
    home_frame(fb, box, 20);
    int edge = tail_right ? box.x + box.width - 1 : box.x, tip = tail_right ? edge + 14 : edge - 14;
    epd_fill_triangle(edge, tail_y - 10, tip, tail_y + 4, edge, tail_y + 10, 0xff, fb);
    epd_draw_line(edge, tail_y - 10, tip, tail_y + 4, 0x20, fb); epd_draw_line(edge, tail_y - 9, tip - (tail_right ? 1 : -1), tail_y + 4, 0x20, fb);
    epd_draw_line(tip, tail_y + 4, edge, tail_y + 10, 0x20, fb); epd_draw_line(tip - (tail_right ? 1 : -1), tail_y + 4, edge, tail_y + 9, 0x20, fb);
}
// 续读卡右上角的小猫头鹰，泡泡里引用当前章节摘录。/ Small corner owl on the resume card quoting the current chapter excerpt.
static void pet_draw_corner(uint8_t *fb, const home_book_t *book) {
    const bool fast = app_settings_main_fast_refresh();
    const pet_mood_t mood = pet_mood();
    char text[BOOK_EXCERPT_CAP + 12];
    if (s_excerpt[0]) snprintf(text, sizeof(text), "“%s”", s_excerpt);
    else if (!book->path[0]) snprintf(text, sizeof(text), "Mở một cuốn sách, mình đọc cùng bạn!");
    else if (mood == PET_SLEEP) snprintf(text, sizeof(text), "Đọc thêm vài trang rồi ngủ nhé.");
    else snprintf(text, sizeof(text), "Đang dở %u%%, đọc tiếp nào!", book->has_progress ? book->progress.pct : 0);
    const EpdRect box = {192, 194, 354, 72};
    pet_bubble(fb, box, 230, true);
    char lines[2][128];
    unsigned rows = pet_wrap(text, 18, box.width - 28, lines, 2);
    for (unsigned i = 0; i < rows; ++i) label_on(fb, box.x + 14, rows == 1 ? 230 : 214 + 32 * i, 18, lines[i], EPD_DRAW_ALIGN_LEFT, 0x30, 0xff);
    pet_owl_draw(fb, 566, 192, 72, mood == PET_MISS ? PET_WAIT : mood, false, fast);
}
static void pet_draw_card(uint8_t *fb, uint8_t card_gray) {
    const EpdRect card = home_pet_card_rect();
    const bool fast = app_settings_main_fast_refresh();
    const pet_mood_t mood = pet_mood();
    ui_fill_round_rect(fb, card, 24, card_gray);
    home_bold_label(fb, 60, 626, 28, "Cú Lém", EPD_DRAW_ALIGN_LEFT, card_gray);
    const unsigned minutes = pet_minutes_30(), level = pet_level(minutes);
    char value[96];
    snprintf(value, sizeof(value), "Cấp %u · %s", level + 1, s_pet_level_names[level]);
    label_on(fb, 622, 626, 19, value, EPD_DRAW_ALIGN_RIGHT, 0x40, card_gray);
    pet_owl_draw(fb, 58, 686, 150, mood, (s_pet_tap % 3) == 1, fast);
    home_bold_label(fb, 60, 866, 18, "Học từ vựng ›", EPD_DRAW_ALIGN_LEFT, card_gray);
    const EpdRect bubble = pet_bubble_rect();
    pet_bubble(fb, bubble, 728, false);
    char message[256], lines[3][128];
    pet_message(message, sizeof(message), mood);
    unsigned rows = pet_wrap(message, 22, bubble.width - 36, lines, 3);
    int first = bubble.y + bubble.height / 2 - (int)(rows - 1) * 17;
    for (unsigned i = 0; i < rows; ++i) label_on(fb, bubble.x + 18, first + 34 * (int)i, 22, lines[i], EPD_DRAW_ALIGN_LEFT, 0x10, 0xff);
    if (!s_reading_days_valid) {
        label_on(fb, 236, 834, 18, "Chưa có ngày giờ nên cú chưa lớn được", EPD_DRAW_ALIGN_LEFT, 0x60, card_gray);
        return;
    }
    const bool maxed = level + 1 >= PET_LEVELS;
    const unsigned lo = s_pet_levels[level], hi = maxed ? lo : s_pet_levels[level + 1];
    label_on(fb, 236, 818, 17, "Kinh nghiệm 30 ngày", EPD_DRAW_ALIGN_LEFT, 0x40, card_gray);
    if (maxed) snprintf(value, sizeof(value), "%u phút · Tối đa", minutes);
    else snprintf(value, sizeof(value), "%u / %u", minutes, hi);
    label_on(fb, 624, 818, 17, value, EPD_DRAW_ALIGN_RIGHT, 0x30, card_gray);
    const EpdRect track = {236, 834, 388, 10};
    if (fast) { home_frame(fb, track, 5); }
    else ui_fill_round_rect(fb, track, 5, UI_GRAY_LIGHT);
    int filled = maxed ? track.width : track.width * (int)(minutes - lo) / (int)(hi - lo);
    if (filled > 0) ui_fill_round_rect(fb, (EpdRect){track.x, track.y, filled < 10 ? 10 : filled, track.height}, 5, UI_GRAY_BLACK);
    snprintf(value, sizeof(value), "1 phút đọc = 1 điểm · Chuỗi %u ngày", home_streak_days(app_settings_home_goal_minutes() * 60));
    label_on(fb, 236, 866, 16, value, EPD_DRAW_ALIGN_LEFT, 0x60, card_gray);
}
// 本周（周一起）七天条；点按进入整月记录页。/ This week's Monday-first seven-day strip; tapping opens the month record.
static void pet_draw_week(uint8_t *fb, uint8_t card_gray) {
    const EpdRect card = home_calendar_rect();
    ui_fill_round_rect(fb, card, 24, card_gray);
    home_bold_label(fb, 60, 944, 24, "Tuần này", EPD_DRAW_ALIGN_LEFT, card_gray);
    if (!s_reading_days_valid) {
        label_on(fb, 342, 1020, 22, "Vui lòng cài đặt ngày giờ trước", EPD_DRAW_ALIGN_CENTER, 0x60, card_gray);
        return;
    }
    static const char *const days[] = {"T2", "T3", "T4", "T5", "T6", "T7", "CN"};
    const unsigned goal = app_settings_home_goal_minutes() * 60;
    const int today = (s_date.tm_wday + 6) % 7;
    unsigned total = 0;
    for (int i = 0; i < 7; ++i) {
        const int offset = i - today, cx = 60 + (i * 564 + 282) / 7, cy = 1036;
        struct tm day = s_date; day.tm_hour = 12; day.tm_mday += offset; mktime(&day);
        char number[4]; snprintf(number, sizeof(number), "%d", day.tm_mday);
        label_on(fb, cx, 988, 18, days[i], EPD_DRAW_ALIGN_CENTER, 0x60, card_gray);
        if (offset > 0) { label_on(fb, cx, cy, 21, number, EPD_DRAW_ALIGN_CENTER, 0x90, card_gray); continue; }
        const unsigned read = pet_recent_seconds(offset);
        total += read;
        if (goal && read >= goal) { epd_fill_circle(cx, cy, 20, 0x20, fb); label(fb, cx, cy, 21, number, EPD_DRAW_ALIGN_CENTER, 0xff); }
        else {
            if (read >= 60) { epd_draw_circle(cx, cy, 20, 0x60, fb); epd_draw_circle(cx, cy, 19, 0x60, fb); }
            label_on(fb, cx, cy, 21, number, EPD_DRAW_ALIGN_CENTER, 0x20, card_gray);
        }
        if (!offset) circle_frame(fb, cx, cy, 25);
    }
    char value[48];
    snprintf(value, sizeof(value), "%u phút · Cả tháng ›", total / 60);
    label_on(fb, 622, 944, 18, value, EPD_DRAW_ALIGN_RIGHT, 0x40, card_gray);
}
