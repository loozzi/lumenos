/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 离线书名匹配边界与性能测试。/ Offline filename boundary and performance tests.
 */
#include "read_pico_search.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static void match(const char* name, const char* q) { assert(read_pico_search_match(name, q)); }
static void miss(const char* name, const char* q) { assert(!read_pico_search_match(name, q)); }
int main(void) {
    // S31 书名搜索按越南语去声调折叠，不再做汉字拼音。/ S31 title search folds Vietnamese tones instead of Chinese pinyin.
    match("Truyện Kiều.txt", "truyen kieu"); match("Truyện Kiều.TXT", "tk"); match("Truyện Kiều.txt", "KIEU");
    match("Đắc Nhân Tâm.epub", "dac nhan tam"); match("Đắc Nhân Tâm.epub", "dnt"); match("Đắc Nhân Tâm.epub", "đắc");
    match("三体.txt", "体"); match("三体.txt", ""); match("三体.epub", "   ");
    miss("Đắc Nhân Tâm.txt", "txt"); miss("Đắc Nhân Tâm.EPUB", "epub"); miss("Đắc Nhân Tâm.txt", "dacx");
    match("The Art of War.txt", "TAOW"); match("The Art of War.txt", "ART OF");
    match("The Art of War.txt", "theartofwar");
    match("Số Đỏ Three Body 2026.epub", "sdtb2026");
    match("Số Đỏ Three Body 2026.epub", "sodothreebody2026");
    match("ABC Ánh.txt", "bcanh"); match("ABC Ánh.txt", "aa");
    miss("甲🙂乙.txt", "jiayi"); match("甲🙂乙.txt", "🙂");
    miss("甲\xc0\xaf.txt", "a"); miss("甲.txt", "\xed\xa0\x80");
    miss("甲\xf0\x9f.txt", "a"); miss(NULL, ""); miss("a.txt", NULL);
    // S31 输入法候选是 Telex 缩表：a 给出带声调的 a 系列。/ S31 IME candidates are the reduced Telex table.
    uint32_t candidates[18] = {0};
    size_t found = read_pico_search_candidates("a", candidates, 18, 0);
    bool saw_acute = false;
    for (size_t i = 0; i < found; ++i) if (candidates[i] == 0x00E1) saw_acute = true;
    assert(found == 18 && saw_acute);
    assert(read_pico_search_candidates("aa", candidates, 18, 0) == 6 && candidates[0] == 0x00E2);
    assert(read_pico_search_candidates("a1", candidates, 18, 0) == 0);
    assert(read_pico_search_candidates("hai", candidates, 18, 0) == 0);
    uint32_t next[5] = {0};
    assert(read_pico_search_candidates("a", candidates, 5, 0) == 5);
    assert(read_pico_search_candidates("a", next, 5, 5) == 5);
    assert(candidates[0] != next[0]);
    assert(read_pico_search_candidates("a", next, 5, 18) == 0);
    char query[66]; memset(query, 'a', 64); query[64] = 0;
    char title[257]; memset(title, 'a', 251); memcpy(title + 251, ".txt", 5);
    match(title, query); query[64] = 'a'; query[65] = 0; miss(title, query);
    memset(title, 'a', 252); memcpy(title + 252, ".txt", 5); miss(title, "a");
    // 长越南语书名不得退化；1000个255字节书名。/ Long Vietnamese names stay linear; 1000 names of 255 bytes.
    for (int i = 0; i < 83; ++i) memcpy(title + i * 3, "ệ", 3);
    memcpy(title + 249, "AA.txt", 7);
    clock_t start = clock();
    for (int i = 0; i < 1000; ++i) miss(title, "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeexyz");
    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("search: PASS; 1000 x 255-byte Vietnamese names: %.3f seconds\n", elapsed);
    return 0;
}
