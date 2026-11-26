/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BOOK_ANNOTATION_MAX 32
#define BOOK_NOTE_CAP 128
#define BOOK_QUOTE_CAP 256
typedef struct { uint32_t start,end; char note[BOOK_NOTE_CAP],quote[BOOK_QUOTE_CAP]; } book_annotation_t;
typedef struct {
    uint64_t text_hash, path_hash;
    uint32_t chapter, length, count, sequence;
    book_annotation_t items[BOOK_ANNOTATION_MAX];
} book_annotations_t;
/* Returns false on corrupt/unreadable data; missing file is an empty valid store.
 * Content identity is part of the filename, preserving older book revisions. */
bool book_annotations_load(book_annotations_t *a,const char *root,const char *path,
                            uint32_t chapter,const char *text,size_t length);
bool book_annotations_save(book_annotations_t *a,const char *root);
bool book_annotations_put(book_annotations_t *a,const char *text,size_t start,size_t end,const char *note);
/* 中文：整本书列表用：只按结构与校验和读一个快照槽；解析快照文件名。
 * English: for the whole-book list: read one snapshot slot by structure and checksum; parse snapshot names. */
bool book_annotations_read_file(book_annotations_t *a,const char *file);
bool book_annotations_parse_name(const char *name,uint64_t *path_hash,uint32_t *chapter,uint64_t *text_hash,unsigned *slot);
uint64_t book_annotations_text_hash(const char *text,size_t length);
