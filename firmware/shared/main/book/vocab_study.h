/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：Leitner 五盒复习与卡上进度。盒 1..5 的间隔为 1/2/4/8/16 天；记得升一盒，忘了回到盒 1 并在本轮末尾再出现
 * （每词每轮最多三次）。每天按 rank 顺序加入新词（5/10/20，默认 10）。进度按 english 的哈希保存，增删 CSV 行不会错位。
 * 进度写在 /vocab/.lumen/<包名>.prog 的两个轮换槽（版本、序号、校验和）；坏槽读旧槽。时钟未同步时用“学习日”：
 * 上一轮把到期词学完后，下一次开始算新的一天。
 * English: Leitner five-box review with progress on the card. Boxes 1..5 wait 1/2/4/8/16 days; remembering moves
 * up one box, forgetting returns to box 1 and repeats at the end of the session (at most three times per word).
 * New words join each day in rank order (5/10/20, default 10). Progress is keyed by a hash of english, so adding
 * or removing CSV rows never shifts it. It is written to two alternating slots of /vocab/.lumen/<pack>.prog
 * (version, sequence, checksum); a bad slot falls back to the other. Without a synced clock a "study day" is
 * used: once a session has cleared every due word, the next session counts as a new day.
 *
 * 冻结：不写 NVS；不在每次翻卡时写卡。
 * Frozen: nothing in NVS; no card write per flip.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "vocab_pack.h"

#define VOCAB_BOXES 5
#define VOCAB_QUEUE_MAX 256
#define VOCAB_REPEATS 3

typedef struct {
    uint32_t key;
    uint8_t box;   ///< 1..5
    uint32_t due;  ///< 到期日 / Day the word is due
} vocab_state_t;

typedef struct {
    vocab_state_t *states; ///< PSRAM，按 key 排序 / PSRAM, sorted by key
    unsigned count;
    uint32_t sequence;
    uint32_t day;          ///< 上次学习的日子 / Day of the last session
    uint32_t study_day;    ///< 未同步时钟时的学习日 / Study day without a synced clock
    uint16_t new_today;    ///< 当天已加入的新词 / New words already added that day
    uint8_t new_per_day;   ///< 5、10 或 20 / 5, 10 or 20
    bool cleared;          ///< 上一轮已学完到期词 / The last session cleared every due word
    bool show_home, show_lock;
} vocab_progress_t;

typedef struct {
    uint16_t word[VOCAB_QUEUE_MAX]; ///< 词包下标 / Pack indexes
    uint8_t seen[VOCAB_QUEUE_MAX];
    unsigned count, at;
    unsigned remembered, forgotten;
} vocab_session_t;

/// 盒 box 的间隔天数。/ Interval in days for a box.
uint32_t vocab_interval(uint8_t box);
/// 今天：时钟已同步时为 UTC+7 的日序号，否则为学习日。/ Today: the UTC+7 day number with a synced clock, else the study day.
uint32_t vocab_today(vocab_progress_t *p, int64_t unix_seconds);
vocab_state_t *vocab_state(vocab_progress_t *p, uint32_t key);
/// 建本轮队列：到期词（按到期日、rank）再加新词。/ Build the session queue: due words (by due day, rank), then new words.
void vocab_session_begin(vocab_session_t *s, vocab_progress_t *p, const vocab_pack_t *pack, uint32_t today);
/// 回答当前词；返回下一个词的下标或 -1（本轮结束）。/ Answer the current word; next pack index or -1 at the end.
int vocab_answer(vocab_session_t *s, vocab_progress_t *p, const vocab_pack_t *pack, bool remembered, uint32_t today);
int vocab_current(const vocab_session_t *s);
/// 统计：已学、今天到期、剩余新词。/ Counts: learned, due today, new words left.
void vocab_counts(const vocab_progress_t *p, const vocab_pack_t *pack, uint32_t today, unsigned *learned, unsigned *due, unsigned *fresh);
/// 今天先展示的词（到期优先，否则下一个新词）；没有返回 -1。/ Today's featured word; -1 when none.
int vocab_featured(const vocab_progress_t *p, const vocab_pack_t *pack, uint32_t today);

bool vocab_progress_load(vocab_progress_t *p, const char *root, const char *pack_name);
bool vocab_progress_save(vocab_progress_t *p, const char *root, const char *pack_name);
void vocab_progress_free(vocab_progress_t *p);
