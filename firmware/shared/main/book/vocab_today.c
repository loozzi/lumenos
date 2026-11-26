/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：“今日单词”摘要的读写。
 * English: reading and writing the "word of the day" summary.
 */
#include "vocab_today.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static vocab_today_t s_today;
static bool s_loaded;

void vocab_today_invalidate(void) { s_loaded = false; memset(&s_today, 0, sizeof(s_today)); }
const vocab_today_t *vocab_today_cached(void) { return &s_today; }

static void copy_value(char *dst, size_t cap, const char *src) {
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
        while (n && ((unsigned char)src[n] & 0xc0) == 0x80) n--;
    }
    memcpy(dst, src, n);
    dst[n] = 0;
}

const vocab_today_t *vocab_today_get(const char *root) {
    if (s_loaded) return &s_today;
    s_loaded = true;
    memset(&s_today, 0, sizeof(s_today));
    char path[300];
    if (snprintf(path, sizeof(path), "%s/%s", root, VOCAB_TODAY_FILE) >= (int)sizeof(path)) return &s_today;
    FILE *f = fopen(path, "r");
    if (!f) return &s_today;
    s_today.show_home = s_today.show_lock = true;
    char line[256];
    unsigned lines = 0;
    while (fgets(line, sizeof(line), f) && ++lines <= 8) {
        line[strcspn(line, "\r\n")] = 0;
        char *value = strchr(line, '=');
        if (!value) continue;
        *value++ = 0;
        if (!strcmp(line, "home")) s_today.show_home = *value == '1';
        else if (!strcmp(line, "lock")) s_today.show_lock = *value == '1';
        else if (!strcmp(line, "english")) copy_value(s_today.english, sizeof(s_today.english), value);
        else if (!strcmp(line, "type")) copy_value(s_today.type, sizeof(s_today.type), value);
        else if (!strcmp(line, "vietnamese")) copy_value(s_today.vietnamese, sizeof(s_today.vietnamese), value);
    }
    fclose(f);
    s_today.valid = s_today.english[0] && s_today.vietnamese[0];
    return &s_today;
}

bool vocab_today_set(const char *root, const vocab_today_t *t) {
    s_today = *t;
    s_loaded = true;
    char path[300], tmp[308];
    if (snprintf(path, sizeof(path), "%s/%s", root, VOCAB_TODAY_FILE) >= (int)sizeof(path) ||
        snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) return false;
    FILE *f = fopen(tmp, "w");
    if (!f) return false;
    bool ok = fprintf(f, "home=%d\nlock=%d\nenglish=%s\ntype=%s\nvietnamese=%s\n", t->show_home, t->show_lock,
                      t->valid ? t->english : "", t->valid ? t->type : "", t->valid ? t->vietnamese : "") > 0;
    ok = !fclose(f) && ok;
    if (ok && unlink(path) && errno != ENOENT) ok = false;
    if (ok && rename(tmp, path)) ok = false;
    if (!ok) unlink(tmp);
    return ok;
}

void vocab_today_line(const vocab_today_t *t, char *out, unsigned cap) {
    if (!t->valid) { if (cap) out[0] = 0; return; }
    if (t->type[0]) snprintf(out, cap, "%s (%s) — %s", t->english, t->type, t->vietnamese);
    else snprintf(out, cap, "%s — %s", t->english, t->vietnamese);
}
