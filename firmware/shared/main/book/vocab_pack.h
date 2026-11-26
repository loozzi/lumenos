/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：用户自备的词汇包（/vocab 下的 .csv）。UTF-8 CSV，首行为列名，多余列忽略；列名与常见 TOEIC 600 数据集一致：
 * english,type,vietnamese,pronounce,explain,example,example_vietnamese，另可有 rank。english 与 vietnamese 必填。
 * 整包放进 PSRAM：偏移表加字符串池，最多 4000 词；过长字段在 UTF-8 边界截断并加“…”，坏行跳过并计数。
 * English: user-supplied vocabulary packs (.csv files in /vocab). UTF-8 CSV with a header row; extra columns are ignored.
 * Column names follow the common TOEIC 600 dataset: english,type,vietnamese,pronounce,explain,example,
 * example_vietnamese, plus optional rank. english and vietnamese are required. The pack lives in PSRAM as an
 * offset table plus a string pool, at most 4000 words; long fields are cut at a UTF-8 boundary with "…",
 * malformed rows are skipped and counted.
 *
 * 冻结：固件不内置词汇数据；不写卡。
 * Frozen: the firmware ships no vocabulary data; this module never writes the card.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VOCAB_MAX_WORDS 4000
#define VOCAB_FILE_MAX (2u * 1024u * 1024u)
#define VOCAB_ENGLISH_CAP 64
#define VOCAB_TYPE_CAP 16
#define VOCAB_MEANING_CAP 160
#define VOCAB_LONG_CAP 240

typedef enum {
    VOCAB_ENGLISH = 0, VOCAB_TYPE, VOCAB_VIETNAMESE, VOCAB_PRONOUNCE, VOCAB_EXPLAIN, VOCAB_EXAMPLE, VOCAB_EXAMPLE_VI,
    VOCAB_FIELDS,
} vocab_field_t;

typedef struct {
    uint32_t text[VOCAB_FIELDS]; ///< 字符串池偏移，0 为空串 / String-pool offsets; 0 is the empty string
    uint32_t rank;               ///< 无 rank 列时为行号 / The row number without a rank column
    uint32_t key;                ///< english 的哈希（ASCII 小写）/ Hash of english (ASCII lower case)
} vocab_word_t;

typedef struct {
    vocab_word_t *words;
    unsigned count;
    char *pool;
    size_t pool_len, pool_cap;
    unsigned skipped;    ///< 缺必填列或重复的行 / Rows missing a required field, or duplicates
    unsigned truncated;  ///< 被截断的字段数 / Fields that were cut
    bool limited;        ///< 超过 4000 词 / More than 4000 words
    char error[96];      ///< 加载失败原因 / Why loading failed
} vocab_pack_t;

/// 读入整包；失败时 error 说明原因。/ Load a whole pack; on failure error says why.
bool vocab_pack_load(vocab_pack_t *pack, const char *path);
void vocab_pack_free(vocab_pack_t *pack);
const char *vocab_text(const vocab_pack_t *pack, unsigned word, vocab_field_t field);
uint32_t vocab_key(const char *english);
/// 按 key 找词；没有返回 -1。/ Find a word by key; -1 when absent.
int vocab_find(const vocab_pack_t *pack, uint32_t key);
