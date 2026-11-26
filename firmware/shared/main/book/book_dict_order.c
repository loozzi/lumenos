/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：词典清单与顺序文件。只读目录与一个小文本文件，不分配堆。
 * English: dictionary list and order file. Reads directories and one small text file; no heap.
 */
#include "book_dict_order.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool is_ifo(const char *name) {
    size_t n = strlen(name);
    return n > 4 && !strcmp(name + n - 4, ".ifo") && name[0] != '.';
}

// 收集 dir（相对 prefix）中的 .ifo；depth 为 0 时进入一层子目录。
// Collect .ifo files in dir (relative to prefix); depth 0 descends one subdirectory level.
static void collect(book_dict_list_t *list, const char *root, const char *prefix, int depth) {
    char path[BOOK_DICT_REL + 256];
    if (snprintf(path, sizeof(path), "%s%s%s", root, *prefix ? "/" : "", prefix) >= (int)sizeof(path)) return;
    DIR *dir = opendir(path);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;
        char rel[BOOK_DICT_REL], full[BOOK_DICT_REL + 256];
        if (snprintf(rel, sizeof(rel), "%s%s%s", prefix, *prefix ? "/" : "", entry->d_name) >= (int)sizeof(rel)) continue;
        if (snprintf(full, sizeof(full), "%s/%s", root, rel) >= (int)sizeof(full)) continue;
        struct stat st;
        if (stat(full, &st)) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!depth) collect(list, root, rel, 1);
        } else if (S_ISREG(st.st_mode) && is_ifo(entry->d_name)) {
            if (list->count == BOOK_DICT_MAX) { list->truncated = true; continue; }
            book_dict_entry_t *e = &list->items[list->count++];
            snprintf(e->rel, sizeof(e->rel), "%s", rel);
            e->enabled = true;
        }
    }
    closedir(dir);
}

static int by_name(const void *a, const void *b) {
    return strcmp(((const book_dict_entry_t *)a)->rel, ((const book_dict_entry_t *)b)->rel);
}

bool book_dict_list_load(book_dict_list_t *list, const char *root) {
    memset(list, 0, sizeof(*list));
    struct stat st;
    if (stat(root, &st) || !S_ISDIR(st.st_mode)) return false;
    collect(list, root, "", 0);
    qsort(list->items, list->count, sizeof(list->items[0]), by_name);
    char path[BOOK_DICT_REL + 64];
    if (snprintf(path, sizeof(path), "%s/%s", root, BOOK_DICT_ORDER_FILE) >= (int)sizeof(path)) return true;
    FILE *f = fopen(path, "r");
    if (!f) return true;
    // 依文件顺序把已知项移到前面；未知行（已删除的词典）忽略。
    // Move known entries to the front in file order; unknown lines (removed dictionaries) are ignored.
    unsigned placed = 0, lines = 0;
    char line[BOOK_DICT_REL + 8];
    while (fgets(line, sizeof(line), f) && ++lines <= 256) {
        if (!strchr(line, '\n') && !feof(f)) {
            int c;
            while ((c = fgetc(f)) != EOF && c != '\n') {}
            continue; // 过长的行不可能匹配 / An overlong line cannot match
        }
        line[strcspn(line, "\r\n")] = 0;
        bool enabled = line[0] != '-';
        const char *rel = enabled ? line : line + 1;
        if (!*rel) continue;
        for (unsigned i = placed; i < list->count; i++) {
            if (strcmp(list->items[i].rel, rel)) continue;
            book_dict_entry_t e = list->items[i];
            memmove(&list->items[placed + 1], &list->items[placed], (i - placed) * sizeof(e));
            e.enabled = enabled;
            list->items[placed++] = e;
            break;
        }
    }
    fclose(f);
    return true;
}

bool book_dict_list_save(const book_dict_list_t *list, const char *root) {
    char path[BOOK_DICT_REL + 64], tmp[BOOK_DICT_REL + 72];
    if (snprintf(path, sizeof(path), "%s/%s", root, BOOK_DICT_ORDER_FILE) >= (int)sizeof(path) ||
        snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) return false;
    FILE *f = fopen(tmp, "w");
    if (!f) return false;
    bool ok = true;
    for (unsigned i = 0; i < list->count && ok; i++)
        ok = fprintf(f, "%s%s\n", list->items[i].enabled ? "" : "-", list->items[i].rel) > 0;
    ok = !fclose(f) && ok;
    if (!ok) { remove(tmp); return false; }
    // FAT 的 rename 不覆盖已有文件。/ FAT rename does not replace an existing file.
    remove(path);
    if (rename(tmp, path)) { remove(tmp); return false; }
    return true;
}

bool book_dict_list_move(book_dict_list_t *list, unsigned index, int delta) {
    if (index >= list->count || (delta != -1 && delta != 1)) return false;
    if ((delta < 0 && !index) || (delta > 0 && index + 1 >= list->count)) return false;
    unsigned other = index + (unsigned)delta;
    book_dict_entry_t e = list->items[index];
    list->items[index] = list->items[other];
    list->items[other] = e;
    return true;
}

bool book_dict_list_path(const book_dict_list_t *list, const char *root, unsigned index, char *out, size_t cap) {
    return index < list->count && snprintf(out, cap, "%s/%s", root, list->items[index].rel) < (int)cap;
}
