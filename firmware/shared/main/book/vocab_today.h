/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：“今日单词”摘要（/vocab/.lumen/today.txt，≤512 字节）。词汇页在学习结束或换日时写入；首页与锁屏只读 RAM 副本，
 * 每次插卡后最多读卡一次。
 * English: the "word of the day" summary (/vocab/.lumen/today.txt, ≤512 bytes). The vocabulary page writes it at the
 * end of a session or on a new day; Home and the lock screen read the RAM copy, loading it at most once per card mount.
 *
 * 冻结：画首页/锁屏时不读卡、不分配内存。
 * Frozen: drawing Home or the lock screen never reads the card or allocates.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define VOCAB_TODAY_FILE "today.txt"

typedef struct {
    bool valid;
    bool show_home, show_lock;
    char english[64], type[16], vietnamese[160];
} vocab_today_t;

/// 读 root 下的摘要（每次挂卡最多一次）；没有时 valid 为 false。/ Load the summary under root once per mount.
const vocab_today_t *vocab_today_get(const char *root);
/// 只返回已在 RAM 中的摘要，不读卡。/ The summary already in RAM, without touching the card.
const vocab_today_t *vocab_today_cached(void);
/// 更新 RAM 副本并写卡。/ Update the RAM copy and write the card.
bool vocab_today_set(const char *root, const vocab_today_t *today);
/// 拔卡或换卡后下次重新读取。/ Reload after the card is removed or replaced.
void vocab_today_invalidate(void);
/// 一行“word (type) — nghĩa”，供首页与锁屏。/ One "word (type) — meaning" line for Home and the lock screen.
void vocab_today_line(const vocab_today_t *today, char *out, unsigned cap);
