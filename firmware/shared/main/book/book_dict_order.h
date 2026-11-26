/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：/dict 下的词典清单、启用状态与查询顺序。顺序保存在卡上 /dict/.lumen_order：
 * 每行一个相对 .ifo 路径，行首 '-' 表示停用；不写 NVS。新词典按名字排在末尾并默认启用。
 * English: dictionaries under /dict with their enabled state and lookup order, saved on the card as
 * /dict/.lumen_order: one relative .ifo path per line, a leading '-' disables it; nothing in NVS.
 * New dictionaries are appended by name and enabled.
 *
 * 冻结：只扫描根目录与一层子目录，最多 BOOK_DICT_MAX 个。
 * Frozen: scan the root and one subdirectory level only, at most BOOK_DICT_MAX entries.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>

#define BOOK_DICT_MAX 32
#define BOOK_DICT_REL 192
#define BOOK_DICT_ORDER_FILE ".lumen_order"

typedef struct {
    char rel[BOOK_DICT_REL]; ///< 相对 /dict 的 .ifo 路径 / .ifo path relative to /dict
    bool enabled;
} book_dict_entry_t;

typedef struct {
    book_dict_entry_t items[BOOK_DICT_MAX];
    unsigned count;
    bool truncated; ///< 超过上限的词典被忽略 / Dictionaries past the limit were ignored
} book_dict_list_t;

/// 扫描 root 并套用顺序文件；root 不存在时返回 false。/ Scan root and apply the order file; false without root.
bool book_dict_list_load(book_dict_list_t *list, const char *root);
/// 原子写顺序文件（临时文件再改名）。/ Write the order file atomically (temporary file, then rename).
bool book_dict_list_save(const book_dict_list_t *list, const char *root);
/// 把第 index 项上移(-1)或下移(+1)；越界时不变。/ Move entry up (-1) or down (+1); no-op at the ends.
bool book_dict_list_move(book_dict_list_t *list, unsigned index, int delta);
/// 完整路径；缓冲不足返回 false。/ Full path of an entry; false when out does not fit.
bool book_dict_list_path(const book_dict_list_t *list, const char *root, unsigned index, char *out, size_t cap);
