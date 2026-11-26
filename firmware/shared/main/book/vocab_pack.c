/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：词汇包 CSV 解析（RFC 4180 引号、CRLF、BOM）。
 * English: vocabulary-pack CSV parser (RFC 4180 quotes, CRLF, BOM).
 */
#include "vocab_pack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

static void *psram_alloc(size_t n) {
#ifdef ESP_PLATFORM
    return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return malloc(n);
#endif
}

uint32_t vocab_key(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z') c += 32;
        h = (h ^ c) * 16777619u;
    }
    return h ? h : 1;
}

void vocab_pack_free(vocab_pack_t *p) {
    free(p->words);
    free(p->pool);
    memset(p, 0, sizeof(*p));
}

const char *vocab_text(const vocab_pack_t *p, unsigned word, vocab_field_t field) {
    if (!p->pool || word >= p->count || field >= VOCAB_FIELDS) return "";
    return p->pool + p->words[word].text[field];
}

int vocab_find(const vocab_pack_t *p, uint32_t key) {
    for (unsigned i = 0; i < p->count; i++) if (p->words[i].key == key) return (int)i;
    return -1;
}

/* ---- 带缓冲的读取 / Buffered reader ---- */
typedef struct { FILE *f; unsigned char buf[4096]; size_t at, len; bool eof; } reader_t;
static int next_byte(reader_t *r) {
    if (r->at == r->len) {
        if (r->eof) return EOF;
        r->len = fread(r->buf, 1, sizeof(r->buf), r->f);
        r->at = 0;
        if (!r->len) { r->eof = true; return EOF; }
    }
    return r->buf[r->at++];
}
static int peek_byte(reader_t *r) {
    int c = next_byte(r);
    if (c != EOF) r->at--;
    return c;
}

#define FIELD_SCRATCH 1024
#define MAX_COLUMNS 32
// 读一个字段到 out（超长部分丢弃并标记）；返回 1 行内还有字段，0 行结束，-1 文件结束。
// Read one field into out (excess is dropped and flagged); 1 more fields, 0 end of row, -1 end of file.
static int read_field(reader_t *r, char *out, size_t cap, size_t *len, bool *overflow) {
    *len = 0;
    *overflow = false;
    int c = next_byte(r);
    if (c == EOF) return -1;
    bool quoted = c == '"';
    if (!quoted) r->at--;
    for (;;) {
        c = next_byte(r);
        if (c == EOF) return 0;
        if (quoted) {
            if (c == '"') {
                if (peek_byte(r) == '"') { next_byte(r); }
                else { quoted = false; continue; }
            }
        } else {
            if (c == ',') return 1;
            if (c == '\n') return 0;
            if (c == '\r') { if (peek_byte(r) == '\n') next_byte(r); return 0; }
        }
        if (*len + 1 < cap) out[(*len)++] = (char)c;
        else *overflow = true;
    }
}

static void trim(char *s, size_t *len) {
    size_t a = 0;
    while (a < *len && (s[a] == ' ' || s[a] == '\t')) a++;
    size_t b = *len;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) b--;
    memmove(s, s + a, b - a);
    *len = b - a;
    s[*len] = 0;
}

// 在 UTF-8 边界截到 cap 字节以内（含“…”）。/ Cut to at most cap bytes at a UTF-8 boundary, with "…".
static bool fit(char *s, size_t *len, size_t cap, bool overflow) {
    if (*len < cap && !overflow) return false;
    size_t n = cap > 4 ? cap - 4 : 0;
    if (n > *len) n = *len;
    while (n && ((unsigned char)s[n] & 0xc0) == 0x80) n--;
    memcpy(s + n, "\xe2\x80\xa6", 3);
    *len = n + 3;
    s[*len] = 0;
    return true;
}

static bool valid_utf8(const char *s, size_t n) {
    for (size_t i = 0; i < n;) {
        unsigned char c = (unsigned char)s[i];
        unsigned extra = c < 0x80 ? 0 : c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : 9;
        if (extra == 9 || i + extra >= n + (extra ? 0 : 1)) return false;
        for (unsigned j = 1; j <= extra; j++) if (((unsigned char)s[i + j] & 0xc0) != 0x80) return false;
        i += extra + 1;
    }
    return true;
}

static uint32_t pool_add(vocab_pack_t *p, const char *s, size_t n) {
    if (!n) return 0;
    if (p->pool_len + n + 1 > p->pool_cap) return UINT32_MAX;
    uint32_t at = (uint32_t)p->pool_len;
    memcpy(p->pool + at, s, n);
    p->pool[at + n] = 0;
    p->pool_len += n + 1;
    return at;
}

static int by_rank(const void *a, const void *b) {
    const vocab_word_t *x = a, *y = b;
    return x->rank < y->rank ? -1 : x->rank > y->rank;
}

bool vocab_pack_load(vocab_pack_t *p, const char *path) {
    vocab_pack_free(p);
    reader_t *r = calloc(1, sizeof(*r));
    char *field = malloc(FIELD_SCRATCH);
    if (!r || !field) { free(r); free(field); snprintf(p->error, sizeof(p->error), "Không đủ bộ nhớ"); return false; }
    bool ok = false;
    r->f = fopen(path, "rb");
    if (!r->f) { snprintf(p->error, sizeof(p->error), "Không mở được tệp"); goto done; }
    if (fseek(r->f, 0, SEEK_END)) goto io;
    long size = ftell(r->f);
    if (size < 0 || fseek(r->f, 0, SEEK_SET)) goto io;
    if ((unsigned long)size > VOCAB_FILE_MAX) { snprintf(p->error, sizeof(p->error), "Tệp lớn hơn 2 MiB"); goto done; }
    p->pool_cap = (size_t)size + 2;
    p->pool = psram_alloc(p->pool_cap);
    p->words = psram_alloc(sizeof(vocab_word_t) * VOCAB_MAX_WORDS);
    if (!p->pool || !p->words) { snprintf(p->error, sizeof(p->error), "Không đủ bộ nhớ"); goto done; }
    p->pool[0] = 0;
    p->pool_len = 1;
    // 跳过 UTF-8 BOM。/ Skip a UTF-8 BOM.
    if (peek_byte(r) == 0xef) {
        next_byte(r);
        if (next_byte(r) != 0xbb || next_byte(r) != 0xbf) { snprintf(p->error, sizeof(p->error), "Tệp không phải UTF-8"); goto done; }
    }
    // 列名映射。/ Map the header columns.
    static const char *const names[VOCAB_FIELDS] = {"english", "type", "vietnamese", "pronounce", "explain", "example", "example_vietnamese"};
    int column_of[MAX_COLUMNS];
    int rank_column = -1, columns = 0, rc;
    bool have[VOCAB_FIELDS] = {0};
    do {
        size_t n;
        bool over;
        rc = read_field(r, field, FIELD_SCRATCH, &n, &over);
        if (rc < 0) break;
        trim(field, &n);
        for (size_t i = 0; i < n; i++) if (field[i] >= 'A' && field[i] <= 'Z') field[i] += 32;
        int id = -1;
        for (int f = 0; f < VOCAB_FIELDS; f++) if (!strcmp(field, names[f])) id = f;
        if (!strcmp(field, "rank") && columns < MAX_COLUMNS) rank_column = columns;
        if (columns < MAX_COLUMNS) column_of[columns] = id;
        if (id >= 0) have[id] = true;
        columns++;
    } while (rc > 0);
    if (!have[VOCAB_ENGLISH] || !have[VOCAB_VIETNAMESE]) {
        snprintf(p->error, sizeof(p->error), "Thiếu cột english hoặc vietnamese");
        goto done;
    }
    static const size_t caps[VOCAB_FIELDS] = {VOCAB_ENGLISH_CAP, VOCAB_TYPE_CAP, VOCAB_MEANING_CAP, VOCAB_ENGLISH_CAP,
                                              VOCAB_LONG_CAP, VOCAB_LONG_CAP, VOCAB_LONG_CAP};
    unsigned row = 0;
    for (;;) {
        vocab_word_t w = {0};
        bool any = false, bad = false;
        int column = 0;
        uint32_t rank = 0;
        bool rank_set = false;
        size_t pool_mark = p->pool_len;
        do {
            size_t n;
            bool over;
            rc = read_field(r, field, FIELD_SCRATCH, &n, &over);
            if (rc < 0) break;
            trim(field, &n);
            if (n) any = true;
            if (!valid_utf8(field, n)) bad = true;
            int id = column < MAX_COLUMNS ? column_of[column] : -1;
            if (column == rank_column) {
                char *end;
                unsigned long v = strtoul(field, &end, 10);
                if (n && !*end && v < UINT32_MAX) { rank = (uint32_t)v; rank_set = true; }
            } else if (id >= 0 && !bad) {
                // 换行与制表符统一为空格。/ Newlines and tabs become spaces.
                for (size_t i = 0; i < n; i++) if ((unsigned char)field[i] < 32) field[i] = ' ';
                if (fit(field, &n, caps[id], over)) p->truncated++;
                uint32_t at = pool_add(p, field, n);
                if (at == UINT32_MAX) bad = true;
                else w.text[id] = at;
            }
            column++;
        } while (rc > 0);
        if (rc < 0 && !any) break;
        row++;
        if (!any) continue; // 空行 / blank line
        if (bad || !w.text[VOCAB_ENGLISH] || !w.text[VOCAB_VIETNAMESE]) {
            p->pool_len = pool_mark;
            p->skipped++;
        } else if (p->count == VOCAB_MAX_WORDS) {
            p->pool_len = pool_mark;
            p->limited = true;
            break;
        } else {
            w.key = vocab_key(p->pool + w.text[VOCAB_ENGLISH]);
            w.rank = rank_set ? rank : row;
            if (vocab_find(p, w.key) >= 0) { p->pool_len = pool_mark; p->skipped++; }
            else p->words[p->count++] = w;
        }
        if (rc < 0) break;
    }
    if (ferror(r->f)) goto io;
    if (!p->count) { snprintf(p->error, sizeof(p->error), "Không có dòng từ hợp lệ"); goto done; }
    qsort(p->words, p->count, sizeof(*p->words), by_rank);
    ok = true;
    goto done;
io:
    snprintf(p->error, sizeof(p->error), "Lỗi đọc thẻ nhớ");
done:
    if (r->f) fclose(r->f);
    free(r);
    free(field);
    if (!ok) {
        char error[sizeof(p->error)];
        memcpy(error, p->error, sizeof(error));
        vocab_pack_free(p);
        memcpy(p->error, error, sizeof(error));
    }
    return ok;
}
