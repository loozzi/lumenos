/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：整本书的笔记列表与 Markdown 导出。分步读取 snapshots/ 中各章快照（每步最多 16 个目录项或 4 个文件），
 * 每章取最新修改的内容版本和序号较大的槽；当前章使用正在阅读的内容版本。列表存放在 PSRAM，最多 512 条。
 * English: whole-book notes list and Markdown export. Read per-chapter snapshots in snapshots/ step by step
 * (at most 16 directory entries or 4 files per step); each chapter uses its most recently modified content
 * revision and the higher-sequence slot, the current chapter its live revision. The list lives in PSRAM, 512 max.
 *
 * 冻结：只读快照，不改存储格式、不改每章 32 条上限，导出不碰 KOReader JSON。
 * Frozen: read snapshots only; storage format, the 32-per-chapter limit and the KOReader JSON stay untouched.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <dirent.h>
#include "book_annotations.h"

#define BOOK_NOTES_BOOK_MAX 512
#define BOOK_NOTES_BOOK_CHAPTERS 1024

typedef struct {
    uint32_t chapter, start, end;
    char quote[BOOK_QUOTE_CAP];
    char note[BOOK_NOTE_CAP];
} book_note_ref_t;

typedef struct {
    uint32_t chapter;
    uint64_t path_hash, text_hash;
    int64_t mtime;
} book_notes_candidate_t;

typedef enum {
    BOOK_NOTES_BOOK_IDLE = 0,
    BOOK_NOTES_BOOK_LISTING,  ///< 枚举快照 / Listing snapshots
    BOOK_NOTES_BOOK_LOADING,  ///< 读取各章 / Reading chapters
    BOOK_NOTES_BOOK_DONE,
    BOOK_NOTES_BOOK_ERROR = -1,
} book_notes_book_state_t;

typedef struct {
    char root[512];
    DIR *dir;
    book_notes_candidate_t *candidates; ///< PSRAM
    unsigned candidate_count, load_at;
    book_note_ref_t *items;             ///< PSRAM
    unsigned count;
    unsigned errors;                    ///< 读不出的章数 / Chapters that could not be read
    bool truncated;                     ///< 超过上限 / Past the list limit
    int state;
    uint32_t current_chapter;
    uint64_t current_text_hash;
    bool has_current;
} book_notes_book_t;

/// 开始读取 root（…/snapshots）；当前章内容版本优先。/ Start reading root (…/snapshots); the current chapter revision wins.
bool book_notes_book_begin(book_notes_book_t *b, const char *root, bool has_current, uint32_t chapter, uint64_t text_hash);
/// 推进一步；返回状态。/ Advance one bounded step and return the state.
int book_notes_book_step(book_notes_book_t *b);
/// 释放目录与内存，可重复调用。/ Release the directory and memory; safe to call twice.
void book_notes_book_end(book_notes_book_t *b);

/// 章节标题回调；失败时导出写“Chương N”。/ Chapter-title callback; on failure the export writes "Chương N".
typedef int (*book_notes_title_fn)(size_t chapter, char *buf, size_t cap);
/// 把已完成的列表写成 Markdown：临时文件再改名，失败保留旧文件。
/// Write a finished list as Markdown via a temporary file and rename; a failure keeps the old file.
bool book_notes_book_export(const book_notes_book_t *b, const char *path, const char *title,
                            const char *date, book_notes_title_fn chapter_title);
