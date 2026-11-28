/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：Leitner 复习逻辑与进度文件。
 * English: Leitner review logic and the progress file.
 */
#include "vocab_study.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

static void *psram_realloc(void *ptr, size_t n) {
#ifdef ESP_PLATFORM
    return heap_caps_realloc(ptr, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return realloc(ptr, n);
#endif
}

uint32_t vocab_interval(uint8_t box) {
    static const uint8_t days[VOCAB_BOXES] = {1, 2, 4, 8, 16};
    return box >= 1 && box <= VOCAB_BOXES ? days[box - 1] : 1;
}

uint32_t vocab_today(vocab_progress_t *p, int64_t now) {
    // 与首页相同：2024 年以后的时间视为已同步，按 UTC+7 计日。
    // As on Home: times after 2024 count as synced, with days in UTC+7.
    if (now >= 1704067200) return (uint32_t)((now + 7 * 3600) / 86400);
    // 学习日放在 1<<24 以上，不与真实日期混淆。/ Study days live above 1<<24, apart from real dates.
    if (p->cleared) { p->study_day++; p->cleared = false; }
    return (1u << 24) + p->study_day;
}

// 时钟从未同步变为已同步（或反过来）时，另一套日序号的词视为已到期。
// When the clock switches between synced and study days, words dated in the other scheme count as due.
static bool is_due(const vocab_state_t *st, uint32_t today) {
    const uint32_t study = 1u << 24;
    return st->due <= today || (today < study && st->due >= study);
}
static int by_key(const void *a, const void *b) {
    const vocab_state_t *x = a, *y = b;
    return x->key < y->key ? -1 : x->key > y->key;
}

vocab_state_t *vocab_state(vocab_progress_t *p, uint32_t key) {
    vocab_state_t probe = {.key = key};
    return p->count ? bsearch(&probe, p->states, p->count, sizeof(*p->states), by_key) : NULL;
}

static vocab_state_t *state_add(vocab_progress_t *p, uint32_t key) {
    vocab_state_t *s = psram_realloc(p->states, sizeof(*s) * (p->count + 1));
    if (!s) return NULL;
    p->states = s;
    unsigned at = 0;
    while (at < p->count && s[at].key < key) at++;
    memmove(&s[at + 1], &s[at], sizeof(*s) * (p->count - at));
    s[at] = (vocab_state_t){.key = key, .box = 1, .due = 0};
    p->count++;
    return &s[at];
}

static void day_roll(vocab_progress_t *p, uint32_t today) {
    if (p->day != today) { p->day = today; p->new_today = 0; }
}

void vocab_session_begin(vocab_session_t *s, vocab_progress_t *p, const vocab_pack_t *pack, uint32_t today) {
    memset(s, 0, sizeof(*s));
    day_roll(p, today);
    // 到期词：词包已按 rank 排序，按到期日稳定插入。/ Due words: the pack is in rank order; insert by due day, stably.
    for (unsigned i = 0; i < pack->count && s->count < VOCAB_QUEUE_MAX; i++) {
        const vocab_state_t *st = vocab_state(p, pack->words[i].key);
        if (!st || !is_due(st, today)) continue;
        unsigned at = s->count;
        while (at > 0) {
            const vocab_state_t *prev = vocab_state(p, pack->words[s->word[at - 1]].key);
            if (prev->due <= st->due || !is_due(prev, today)) break;
            s->word[at] = s->word[at - 1];
            at--;
        }
        s->word[at] = (uint16_t)i;
        s->count++;
    }
    unsigned budget = p->new_per_day > p->new_today ? p->new_per_day - p->new_today : 0;
    for (unsigned i = 0; i < pack->count && budget && s->count < VOCAB_QUEUE_MAX; i++) {
        if (vocab_state(p, pack->words[i].key)) continue;
        s->word[s->count++] = (uint16_t)i;
        budget--;
    }
}

int vocab_current(const vocab_session_t *s) { return s->at < s->count ? s->word[s->at] : -1; }

int vocab_answer(vocab_session_t *s, vocab_progress_t *p, const vocab_pack_t *pack, bool remembered, uint32_t today) {
    int word = vocab_current(s);
    if (word < 0) return -1;
    uint32_t key = pack->words[word].key;
    vocab_state_t *st = vocab_state(p, key);
    if (!st) {
        st = state_add(p, key);
        if (!st) return word; // 内存不足：停在当前词 / Out of memory: stay on this word
        p->new_today++;
    }
    if (remembered) {
        if (st->box < VOCAB_BOXES) st->box++;
        st->due = today + vocab_interval(st->box);
        s->remembered++;
    } else {
        st->box = 1;
        st->due = today; // 今天仍到期 / Still due today
        s->forgotten++;
        // 本轮末尾再来一次。/ Once more at the end of this session.
        if (s->seen[s->at] + 1 < VOCAB_REPEATS && s->count < VOCAB_QUEUE_MAX) {
            s->word[s->count] = (uint16_t)word;
            s->seen[s->count] = (uint8_t)(s->seen[s->at] + 1);
            s->count++;
        }
    }
    s->at++;
    if (s->at >= s->count) p->cleared = true;
    return vocab_current(s);
}

void vocab_counts(const vocab_progress_t *p, const vocab_pack_t *pack, uint32_t today, unsigned *learned, unsigned *due, unsigned *fresh) {
    *learned = *due = *fresh = 0;
    for (unsigned i = 0; i < pack->count; i++) {
        const vocab_state_t *st = vocab_state((vocab_progress_t *)p, pack->words[i].key);
        if (!st) (*fresh)++;
        else {
            (*learned)++;
            if (is_due(st, today)) (*due)++;
        }
    }
}

int vocab_featured(const vocab_progress_t *p, const vocab_pack_t *pack, uint32_t today) {
    int first_new = -1;
    for (unsigned i = 0; i < pack->count; i++) {
        const vocab_state_t *st = vocab_state((vocab_progress_t *)p, pack->words[i].key);
        if (st && is_due(st, today)) return (int)i;
        if (!st && first_new < 0) first_new = (int)i;
    }
    return first_new;
}

/* ---- 进度文件 / Progress file ---- */
// 头 32 字节：magic、版本、条数、序号、日、学习日、当天新词、每日新词、标志、校验和；随后每条 12 字节。
// 32-byte header: magic, version, count, sequence, day, study day, new today, per day, flags, checksum; then 12 bytes per entry.
#define HEADER 32
static void put32(unsigned char *b, uint32_t v) { for (int i = 0; i < 4; i++) b[i] = (unsigned char)(v >> (8 * i)); }
static uint32_t get32(const unsigned char *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
static uint32_t fnv(uint32_t h, const unsigned char *b, size_t n) { while (n--) h = (h ^ *b++) * 16777619u; return h; }

static bool slot_path(char *out, size_t cap, const char *root, const char *pack, unsigned slot) {
    char name[96];
    size_t n = strlen(pack);
    if (n > 4 && !strcmp(pack + n - 4, ".csv")) n -= 4;
    if (n >= sizeof(name)) n = sizeof(name) - 1;
    memcpy(name, pack, n);
    name[n] = 0;
    for (char *c = name; *c; c++) if (*c == '/' || *c == '\\') *c = '_';
    return snprintf(out, cap, "%s/%s.%u.prog", root, name, slot) < (int)cap;
}

void vocab_progress_free(vocab_progress_t *p) {
    free(p->states);
    p->states = NULL;
    p->count = 0;
}

static bool load_slot(vocab_progress_t *p, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char h[HEADER];
    bool ok = fread(h, 1, HEADER, f) == HEADER && !memcmp(h, "LVOC", 4) && h[4] == 1;
    uint32_t count = ok ? get32(h + 8) : 0;
    ok = ok && count <= VOCAB_MAX_WORDS;
    vocab_state_t *states = NULL;
    uint32_t sum = 2166136261u;
    if (ok && count) {
        states = malloc(sizeof(*states) * count);
        ok = states != NULL;
        for (uint32_t i = 0; ok && i < count; i++) {
            unsigned char e[12];
            ok = fread(e, 1, 12, f) == 12;
            if (!ok) break;
            sum = fnv(sum, e, 12);
            states[i] = (vocab_state_t){.key = get32(e), .box = e[4], .due = get32(e + 8)};
            ok = states[i].box >= 1 && states[i].box <= VOCAB_BOXES && (!i || states[i - 1].key < states[i].key);
        }
    }
    sum = fnv(sum, h, 28);
    ok = ok && fgetc(f) == EOF && get32(h + 28) == sum;
    fclose(f);
    if (!ok) { free(states); return false; }
    vocab_progress_free(p);
    vocab_state_t *moved = count ? psram_realloc(NULL, sizeof(*moved) * count) : NULL;
    if (count && !moved) { free(states); return false; }
    if (count) memcpy(moved, states, sizeof(*moved) * count);
    free(states);
    p->states = moved;
    p->count = count;
    p->sequence = get32(h + 12);
    p->day = get32(h + 16);
    p->study_day = get32(h + 20);
    p->new_today = (uint16_t)(h[24] | h[25] << 8);
    p->new_per_day = h[26];
    p->cleared = h[27] & 1;
    p->show_home = !(h[27] & 2);
    p->show_lock = !(h[27] & 4);
    if (p->new_per_day != 5 && p->new_per_day != 20) p->new_per_day = 10;
    return true;
}

bool vocab_progress_load(vocab_progress_t *p, const char *root, const char *pack) {
    vocab_progress_free(p);
    memset(p, 0, sizeof(*p));
    p->new_per_day = 10;
    p->show_home = p->show_lock = true;
    vocab_progress_t best = {0}, candidate = {0};
    bool found = false, exists = false;
    for (unsigned slot = 0; slot < 2; slot++) {
        char path[512];
        struct stat st;
        if (!slot_path(path, sizeof(path), root, pack, slot) || stat(path, &st)) continue;
        exists = true;
        if (load_slot(&candidate, path) && (!found || candidate.sequence > best.sequence)) {
            vocab_progress_free(&best);
            best = candidate;
            candidate = (vocab_progress_t){0};
            found = true;
        }
        vocab_progress_free(&candidate);
    }
    if (found) *p = best;
    // 两个槽都坏才算失败；没有文件是新的开始。/ Only two bad slots fail; no file is a fresh start.
    return found || !exists;
}

static bool make_dir(const char *root) {
    char path[512];
    if (snprintf(path, sizeof(path), "%s", root) >= (int)sizeof(path)) return false;
    for (char *c = path + 1;; c++) {
        if (*c && *c != '/') continue;
        char end = *c;
        *c = 0;
        // ESP-IDF 对挂载点本身（/sdcard）返回 EINVAL 而非 EEXIST：已是目录即可继续。
        // ESP-IDF returns EINVAL, not EEXIST, for the mount point itself (/sdcard): an existing directory is fine.
        struct stat st;
        if (mkdir(path, 0775) && errno != EEXIST && (stat(path, &st) || !S_ISDIR(st.st_mode))) return false;
        *c = end;
        if (!end) break;
    }
    return true;
}

bool vocab_progress_save(vocab_progress_t *p, const char *root, const char *pack) {
    if (!make_dir(root)) return false;
    char path[512], tmp[520];
    unsigned slot = (p->sequence + 1) & 1;
    if (!slot_path(path, sizeof(path), root, pack, slot) || snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) return false;
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    unsigned char h[HEADER] = {'L', 'V', 'O', 'C', 1};
    put32(h + 8, p->count);
    put32(h + 12, p->sequence + 1);
    put32(h + 16, p->day);
    put32(h + 20, p->study_day);
    h[24] = (unsigned char)p->new_today;
    h[25] = (unsigned char)(p->new_today >> 8);
    h[26] = p->new_per_day;
    h[27] = (unsigned char)((p->cleared ? 1 : 0) | (p->show_home ? 0 : 2) | (p->show_lock ? 0 : 4));
    uint32_t sum = 2166136261u;
    bool ok = fwrite(h, 1, HEADER, f) == HEADER;
    for (unsigned i = 0; ok && i < p->count; i++) {
        unsigned char e[12] = {0};
        put32(e, p->states[i].key);
        e[4] = p->states[i].box;
        put32(e + 8, p->states[i].due);
        sum = fnv(sum, e, 12);
        ok = fwrite(e, 1, 12, f) == 12;
    }
    sum = fnv(sum, h, 28);
    put32(h + 28, sum);
    ok = ok && !fseek(f, 0, SEEK_SET) && fwrite(h, 1, HEADER, f) == HEADER;
    ok = !fflush(f) && ok;
    ok = !fsync(fileno(f)) && ok;
    ok = !fclose(f) && ok;
    // 另一个槽保持完好，直到这一槽的替换写完。/ The other slot stays intact until this one is fully replaced.
    if (ok && unlink(path) && errno != ENOENT) ok = false;
    if (ok && rename(tmp, path)) ok = false;
    if (!ok) { unlink(tmp); return false; }
    p->sequence++;
    return true;
}
