/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "book_dict_file.h"
#define BOOK_DICT_TEXT 8192
#define BOOK_DICT_PATH 512
/* One bounded, cancellable SD lookup. 32-bit StarDict plain/gzip/dictzip textual fields.
 * 中文：每个词典在 PSRAM 里保留一份稀疏索引（每 stride 条记一个检查点），
 * 首次顺序扫描时顺带建立；之后只读检查点之间的一小段索引。gzip 索引在 4 MiB 以内整份放入 PSRAM。
 * English: each dictionary keeps a sparse PSRAM index (one checkpoint per stride records) built during
 * the first sequential scan; later lookups read only the index slice between two checkpoints. A gzip
 * index up to 4 MiB is kept whole in PSRAM. Unsorted or oversized indexes fall back to the old scan. */
#define BOOK_DICT_SUGGESTIONS 5
#define BOOK_DICT_SUGGESTION_CAP 64
typedef struct book_dict_cache book_dict_cache_t;
typedef struct {
    book_dict_file_t *index, *data;
    FILE *syn;
    char base[BOOK_DICT_PATH], query[256], text[BOOK_DICT_TEXT], name[128];
    uint32_t words, scanned, target, data_size;
    uint32_t definition_length;
    size_t definition_bytes;
    char types[9];
    bool truncated;
    int state; /* 0 closed, 1 index, 2 synonyms, 3 alias index, 4 result, 5 reading definition,
                  6 indexed slice, 7 indexed alias slice, -1 error, -2 missing */
    book_dict_cache_t *cache;   ///< 共享稀疏索引，不归本查询所有 / Shared sparse index, not owned by the lookup
    uint32_t position;          ///< 当前索引流字节位置 / Current byte position in the index stream
    uint32_t skip_to;           ///< 顺序扫描前要跳到的位置 / Position to skip to before a sequential scan
    unsigned char *slice;       ///< 已读入的索引片段 / Loaded index slice
    bool slice_owned, slice_loading;
    bool indexed;               ///< 本次查询时索引已完整 / The index was complete for this lookup
    uint32_t slice_at, slice_len, slice_record;
    unsigned suggestion_count;  ///< 未命中时的相邻词 / Neighbouring keys after a miss
    char suggestions[BOOK_DICT_SUGGESTIONS][BOOK_DICT_SUGGESTION_CAP];
} book_dictionary_t;
bool book_dictionary_open(book_dictionary_t *d, const char *ifo, const char *query);
int book_dictionary_step(book_dictionary_t *d); /* <=4096 records/call; ESP yields after ~8ms between record groups */
void book_dictionary_close(book_dictionary_t *d);
/// 释放全部稀疏索引（拔卡、/dict 变化或内存紧张时）。/ Drop every sparse index (card removal, /dict change, low memory).
void book_dictionary_cache_clear(void);
/// 当前索引占用的字节数，供测试与日志。/ Bytes held by sparse indexes, for tests and logs.
size_t book_dictionary_cache_bytes(void);
/// 本次查询结束时该词典索引是否已完整。/ Whether the dictionary's index was complete for this lookup.
bool book_dictionary_indexed(const book_dictionary_t *d);
