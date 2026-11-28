/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：S31 电量与延迟计数快照，只供日志导出；静态存储，不建任务、不分配堆。
 * 仅由 UI 任务写入（等待、刷屏、锁屏、前光均在 UI 任务中发生）。
 * English: S31 power and latency counters for log export only; static storage, no task
 * or heap. Written from the UI task only (waits, scans, lock and front light all run there).
 *
 * 冻结：计数不得改变任何调度、刷屏或省电决策。
 * Frozen: counters never influence scheduling, display or power decisions.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    METALIO_LATENCY_TURN = 0, ///< 阅读正文输入到显示完成 / Reader body input to presented frame
    METALIO_LATENCY_UI,       ///< 其他页面输入到显示完成 / Other pages: input to presented frame
    METALIO_LATENCY_UNLOCK,   ///< 解锁唤醒到首帧 / Unlock wake to first frame
    METALIO_LATENCY_BOOT,     ///< 开机到界面就绪 / Boot to UI ready
    METALIO_LATENCY_OPEN,     ///< 开书总耗时 / Whole book open
    METALIO_LATENCY_CHAPTER,  ///< 换章载入与排版 / Chapter load and layout on a chapter change
    METALIO_LATENCY_COUNT,
} metalio_latency_kind_t;

typedef enum {
    METALIO_SCAN_GC = 0, ///< GC16 / GC 全刷 / Full GC refresh
    METALIO_SCAN_GL,     ///< GL16 与其他灰阶 / GL16 and other gray modes
    METALIO_SCAN_DU,     ///< DU 快刷 / DU fast update
    METALIO_SCAN_KINDS,
} metalio_scan_kind_t;

/// 直方图上界（ms）；最后一格为更慢。/ Histogram upper bounds in ms; the last bucket is slower.
#define METALIO_LATENCY_BUCKETS 7
extern const uint16_t metalio_latency_bounds_ms[METALIO_LATENCY_BUCKETS - 1];

typedef struct {
    uint64_t wait_idle_ms;     ///< 允许降频的等待总时长 / Total waits allowed to lower the clock
    uint64_t wait_legacy_ms;   ///< 普通等待总时长 / Total ordinary waits
    uint32_t waits_idle, waits_legacy;
    uint32_t scans[METALIO_SCAN_KINDS];
    uint32_t light_sleeps;     ///< 锁屏浅睡次数 / Lock light-sleep entries
    uint32_t maintenance_wakes;///< 定时维护唤醒 / Timer maintenance wakes
    uint64_t light_sleep_ms;   ///< 浅睡总时长 / Total light-sleep time
    uint64_t lamp_on_ms;       ///< 前光点亮时长 / Front-light on time
    uint64_t lamp_level_ms;    ///< 亮度百分比×毫秒/100 / Brightness percent × ms / 100
    uint32_t locks;
    int16_t lock_soc, unlock_soc; ///< 最近一次锁屏/解锁电量，-1未知 / Last lock/unlock SOC, -1 unknown
    uint16_t lock_mv, unlock_mv;
    uint32_t latency[METALIO_LATENCY_COUNT][METALIO_LATENCY_BUCKETS];
    uint32_t last_ms[METALIO_LATENCY_COUNT]; ///< 最近一次耗时 / Most recent sample
    uint32_t max_ms[METALIO_LATENCY_COUNT];  ///< 最慢一次 / Slowest sample
} metalio_power_stats_t;

void metalio_power_note_wait(bool idle_allowed, uint32_t elapsed_ms);
void metalio_power_note_scan(metalio_scan_kind_t kind);
void metalio_power_note_light_sleep(uint32_t elapsed_ms, unsigned maintenance_wakes);
/// 前光输出变化时调用；0 表示熄灭。/ Call when the front-light output changes; 0 means off.
void metalio_power_note_lamp(unsigned brightness_percent, int64_t now_us);
void metalio_power_note_lock(bool locking, int soc, unsigned mv);
void metalio_power_note_latency(metalio_latency_kind_t kind, uint32_t elapsed_ms);
/// 标记解锁时刻，下一帧显示完成时记录解锁延迟。/ Mark an unlock; the next presented frame records its latency.
void metalio_power_mark_unlock(int64_t now_us);
/// 取走解锁标记；无标记返回 0。/ Take the unlock mark; 0 when none.
int64_t metalio_power_take_unlock_mark(void);
/// 快照；会结算当前前光区间。/ Snapshot, closing the current front-light interval.
metalio_power_stats_t metalio_power_stats(int64_t now_us);
/// 由直方图估计百分位（返回所在格上界，最慢格返回 UINT32_MAX，无样本返回 0）。
/// Percentile from a histogram: the bucket upper bound, UINT32_MAX for the slowest bucket, 0 without samples.
uint32_t metalio_power_percentile(const uint32_t buckets[METALIO_LATENCY_BUCKETS], unsigned percent);

#ifdef __cplusplus
}
#endif
