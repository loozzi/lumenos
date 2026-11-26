/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：整本书笔记列表与 Markdown 导出的实现。
 * English: whole-book notes list and Markdown export.
 */
#include "book_notes_book.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

static void *psram_calloc(size_t n, size_t size) {
#ifdef ESP_PLATFORM
    return heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return calloc(n, size);
#endif
}

void book_notes_book_end(book_notes_book_t *b) {
    if (!b) return;
    if (b->dir) closedir(b->dir);
    free(b->candidates);
    free(b->items);
    b->dir = NULL;
    b->candidates = NULL;
    b->items = NULL;
    b->candidate_count = b->load_at = b->count = 0;
    b->state = BOOK_NOTES_BOOK_IDLE;
}

bool book_notes_book_begin(book_notes_book_t *b, const char *root, bool has_current, uint32_t chapter, uint64_t text_hash) {
    book_notes_book_end(b);
    memset(b, 0, sizeof(*b));
    if (snprintf(b->root, sizeof(b->root), "%s", root) >= (int)sizeof(b->root)) { b->state = BOOK_NOTES_BOOK_ERROR; return false; }
    b->has_current = has_current;
    b->current_chapter = chapter;
    b->current_text_hash = text_hash;
    b->candidates = psram_calloc(BOOK_NOTES_BOOK_CHAPTERS, sizeof(*b->candidates));
    b->items = psram_calloc(BOOK_NOTES_BOOK_MAX, sizeof(*b->items));
    if (!b->candidates || !b->items) { book_notes_book_end(b); b->state = BOOK_NOTES_BOOK_ERROR; return false; }
    b->dir = opendir(root);
    // 没有目录就是没有笔记。/ No directory simply means no notes.
    b->state = b->dir ? BOOK_NOTES_BOOK_LISTING : BOOK_NOTES_BOOK_DONE;
    return true;
}

// 每章保留一个内容版本：当前章用正在读的版本，其余取最近修改的。
// Keep one content revision per chapter: the live one for the current chapter, else the latest modified.
static void offer(book_notes_book_t *b, uint32_t chapter, uint64_t path_hash, uint64_t text_hash, int64_t mtime) {
    bool current = b->has_current && chapter == b->current_chapter;
    for (unsigned i = 0; i < b->candidate_count; i++) {
        book_notes_candidate_t *c = &b->candidates[i];
        if (c->chapter != chapter) continue;
        if (current) {
            if (text_hash == b->current_text_hash) { c->text_hash = text_hash; c->path_hash = path_hash; c->mtime = INT64_MAX; }
        } else if (mtime > c->mtime || (mtime == c->mtime && text_hash == c->text_hash)) {
            c->text_hash = text_hash; c->path_hash = path_hash; c->mtime = mtime;
        }
        return;
    }
    if (current && text_hash != b->current_text_hash) mtime = INT64_MIN; // 旧版本只在没有现行版本时使用 / Old revisions only as a fallback
    else if (current) mtime = INT64_MAX;
    if (b->candidate_count == BOOK_NOTES_BOOK_CHAPTERS) { b->truncated = true; return; }
    b->candidates[b->candidate_count++] = (book_notes_candidate_t){chapter, path_hash, text_hash, mtime};
}

static int by_chapter(const void *x, const void *y) {
    const book_notes_candidate_t *a = x, *b = y;
    return a->chapter < b->chapter ? -1 : a->chapter > b->chapter;
}

static int by_position(const void *x, const void *y) {
    const book_note_ref_t *a = x, *b = y;
    if (a->chapter != b->chapter) return a->chapter < b->chapter ? -1 : 1;
    if (a->start != b->start) return a->start < b->start ? -1 : 1;
    return a->end < b->end ? -1 : a->end > b->end;
}

// 读一章：两个槽取有效且序号大的。/ Load one chapter: the valid slot with the higher sequence.
static void load_chapter(book_notes_book_t *b, const book_notes_candidate_t *c, book_annotations_t *scratch, book_annotations_t *best) {
    bool valid = false, seen = false;
    for (unsigned slot = 0; slot < 2; slot++) {
        char file[640];
        if (snprintf(file, sizeof(file), "%s/%016llx-%u-%016llx.%u.ann", b->root, (unsigned long long)c->path_hash,
                     (unsigned)c->chapter, (unsigned long long)c->text_hash, slot) >= (int)sizeof(file)) continue;
        struct stat st;
        if (stat(file, &st)) continue;
        seen = true;
        if (book_annotations_read_file(scratch, file) && (!valid || scratch->sequence > best->sequence)) {
            *best = *scratch;
            valid = true;
        }
    }
    if (seen && !valid) { b->errors++; return; }
    for (uint32_t i = 0; valid && i < best->count; i++) {
        if (b->count == BOOK_NOTES_BOOK_MAX) { b->truncated = true; return; }
        book_note_ref_t *r = &b->items[b->count++];
        r->chapter = c->chapter;
        r->start = best->items[i].start;
        r->end = best->items[i].end;
        memcpy(r->quote, best->items[i].quote, sizeof(r->quote));
        memcpy(r->note, best->items[i].note, sizeof(r->note));
    }
}

int book_notes_book_step(book_notes_book_t *b) {
    if (b->state == BOOK_NOTES_BOOK_LISTING) {
        for (unsigned n = 0; n < 16; n++) {
            struct dirent *e = readdir(b->dir);
            if (!e) {
                closedir(b->dir);
                b->dir = NULL;
                qsort(b->candidates, b->candidate_count, sizeof(*b->candidates), by_chapter);
                b->state = BOOK_NOTES_BOOK_LOADING;
                return b->state;
            }
            uint64_t path_hash, text_hash;
            uint32_t chapter;
            unsigned slot;
            if (!book_annotations_parse_name(e->d_name, &path_hash, &chapter, &text_hash, &slot)) continue;
            char file[640];
            struct stat st;
            if (snprintf(file, sizeof(file), "%s/%s", b->root, e->d_name) >= (int)sizeof(file) || stat(file, &st)) continue;
            offer(b, chapter, path_hash, text_hash, (int64_t)st.st_mtime);
        }
        return b->state;
    }
    if (b->state == BOOK_NOTES_BOOK_LOADING) {
        book_annotations_t *scratch = psram_calloc(2, sizeof(book_annotations_t));
        if (!scratch) { b->state = BOOK_NOTES_BOOK_ERROR; return b->state; }
        for (unsigned n = 0; n < 4 && b->load_at < b->candidate_count; n++)
            load_chapter(b, &b->candidates[b->load_at++], &scratch[0], &scratch[1]);
        free(scratch);
        if (b->load_at >= b->candidate_count) {
            qsort(b->items, b->count, sizeof(*b->items), by_position);
            b->state = BOOK_NOTES_BOOK_DONE;
        }
    }
    return b->state;
}

/* ---- Markdown ---- */
// 写一行：换行与控制字符变空格；行首的 Markdown 标记加反斜杠。
// Write one line: newlines and controls become spaces; a leading Markdown marker gets a backslash.
static bool write_text(FILE *f, const char *prefix, const char *text) {
    if (fputs(prefix, f) < 0) return false;
    while (*text == ' ') ++text;
    const char *markers = "#>-*+=|`~_[!";
    if (*text && strchr(markers, *text)) { if (fputc('\\', f) == EOF) return false; }
    else {
        const char *d = text;
        while (*d >= '0' && *d <= '9') ++d;
        if (d != text && (*d == '.' || *d == ')')) {
            if (fwrite(text, 1, (size_t)(d - text), f) != (size_t)(d - text) || fputc('\\', f) == EOF) return false;
            text = d;
        }
    }
    for (; *text; ++text) {
        unsigned char c = (unsigned char)*text;
        if (fputc(c < 32 ? ' ' : c, f) == EOF) return false;
    }
    return fputc('\n', f) != EOF;
}

bool book_notes_book_export(const book_notes_book_t *b, const char *path, const char *title,
                            const char *date, book_notes_title_fn chapter_title) {
    if (b->state != BOOK_NOTES_BOOK_DONE) { errno = EINVAL; return false; }
    char tmp[600];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) { errno = ENAMETOOLONG; return false; }
    FILE *f = fopen(tmp, "w");
    if (!f) return false;
    bool ok = write_text(f, "# ", title && *title ? title : "Ghi chú") && fputc('\n', f) != EOF;
    if (ok && date && *date) ok = fprintf(f, "Xuất ngày %s · %u ghi chú\n\n", date, b->count) > 0;
    else if (ok) ok = fprintf(f, "%u ghi chú\n\n", b->count) > 0;
    uint32_t chapter = UINT32_MAX;
    for (unsigned i = 0; ok && i < b->count; i++) {
        const book_note_ref_t *r = &b->items[i];
        if (r->chapter != chapter) {
            chapter = r->chapter;
            char name[160] = {0}, heading[180];
            if (!chapter_title || chapter_title(chapter, name, sizeof(name)) != 0 || !name[0])
                snprintf(name, sizeof(name), "Chương %u", (unsigned)chapter + 1);
            snprintf(heading, sizeof(heading), "%s", name);
            ok = write_text(f, "## ", heading) && fputc('\n', f) != EOF;
        }
        if (ok) ok = write_text(f, "> ", r->quote);
        if (ok && r->note[0]) ok = fputc('\n', f) != EOF && write_text(f, "", r->note);
        if (ok) ok = fputc('\n', f) != EOF;
    }
    if (ok && b->truncated) ok = fputs("_Danh sách đã bị cắt ở 512 ghi chú._\n", f) >= 0;
    ok = !fflush(f) && ok;
    ok = !fsync(fileno(f)) && ok;
    ok = !fclose(f) && ok;
    if (!ok) { int e = errno; unlink(tmp); errno = e ? e : EIO; return false; }
    // FAT 改名不会覆盖：先删旧文件。改名失败时保留完整的 .tmp，不丢内容。
    // FAT rename does not replace: remove the old file first. If the rename fails the complete .tmp stays.
    if (unlink(path) && errno != ENOENT) { int e = errno; unlink(tmp); errno = e; return false; }
    if (rename(tmp, path)) return false;
    return true;
}
