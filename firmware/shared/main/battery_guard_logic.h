/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：低电量分级与续航估算的纯逻辑；只看输入快照，不读硬件、不画屏、不写设置。
 *
 * English: pure low-battery levels and runtime estimate; reads only the given
 * snapshot, never touches hardware, drawing or settings.
 *
 * 冻结：充电或电量无效时不得进入危险级；危险级必须持续确认后才触发关机。
 * Frozen: never enter the critical level while charging or with invalid data;
 * critical shutdown requires a sustained confirmation window.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/// 用户2026-11-25确认的阈值（百分比）。/ Thresholds confirmed by the user on 2026-11-25 (percent).
#define BATTERY_GUARD_WARN_PCT 15
#define BATTERY_GUARD_SAVER_PCT 8
#define BATTERY_GUARD_CRITICAL_PCT 3
/// 退出某级需高出阈值的回差。/ Hysteresis required above a threshold to leave a level.
#define BATTERY_GUARD_HYSTERESIS_PCT 2
/// 危险级需持续的时间，覆盖低电量时至少一次新的电量计采样（15秒）。
/// Critical must persist this long, spanning at least one fresh 15 s gauge sample.
#define BATTERY_GUARD_CRITICAL_CONFIRM_MS 20000
/// 省电级前光上限（百分比）。/ Front-light cap in the saver level (percent).
#define BATTERY_GUARD_SAVER_LIGHT_CAP 40
/// 低电量时禁止开始固件升级。/ Refuse to start a firmware update below this level.
#define BATTERY_GUARD_UPDATE_MIN_PCT 30
/// 续航估算：至少下降的百分点与清醒时长。/ Estimate needs this drop and awake time.
#define BATTERY_GUARD_ESTIMATE_MIN_DROP 3
#define BATTERY_GUARD_ESTIMATE_MIN_AWAKE_MS (20LL * 60 * 1000)

typedef enum {
    BATTERY_LEVEL_NORMAL = 0, ///< 正常 / Normal
    BATTERY_LEVEL_WARN,       ///< 提示 / Warning
    BATTERY_LEVEL_SAVER,      ///< 省电 / Saver
    BATTERY_LEVEL_CRITICAL,   ///< 危险，需关机 / Critical, power off
} battery_level_t;

typedef struct {
    battery_level_t level;      ///< 当前等级 / Current level
    int64_t critical_since_ms;  ///< 连续处于危险值的起点，0为无 / Start of the critical run, 0 when none
    bool shutdown_due;          ///< 已确认需要关机 / Confirmed shutdown request
    // 续航估算：基准电量和基准清醒时间。/ Estimate baseline SOC and awake time.
    int base_pct;
    int64_t base_awake_ms;
    int last_pct;
    int64_t last_awake_ms;
    bool estimate_valid;
    uint32_t minutes_per_pct_x10;  ///< 每百分点清醒分钟×10 / Awake minutes per percent ×10
} battery_guard_state_t;

static inline void battery_guard_reset(battery_guard_state_t *s) {
    s->level = BATTERY_LEVEL_NORMAL;
    s->critical_since_ms = 0;
    s->shutdown_due = false;
    s->base_pct = -1;
    s->base_awake_ms = 0;
    s->last_pct = -1;
    s->last_awake_ms = 0;
    s->estimate_valid = false;
    s->minutes_per_pct_x10 = 0;
}

// 按回差决定等级：进入用阈值，退出需高出回差。/ Enter at the threshold, leave only above it plus hysteresis.
static inline battery_level_t battery_guard_classify(battery_level_t previous, int pct) {
    const int h = BATTERY_GUARD_HYSTERESIS_PCT;
    if (pct <= BATTERY_GUARD_CRITICAL_PCT) return BATTERY_LEVEL_CRITICAL;
    if (pct <= BATTERY_GUARD_SAVER_PCT) return BATTERY_LEVEL_SAVER;
    if (previous >= BATTERY_LEVEL_SAVER && pct < BATTERY_GUARD_SAVER_PCT + h) return BATTERY_LEVEL_SAVER;
    if (pct <= BATTERY_GUARD_WARN_PCT) return BATTERY_LEVEL_WARN;
    if (previous >= BATTERY_LEVEL_WARN && pct < BATTERY_GUARD_WARN_PCT + h) return BATTERY_LEVEL_WARN;
    return BATTERY_LEVEL_NORMAL;
}

/// 以一次快照更新状态；pct<0表示无效。awake_ms为不含锁屏睡眠的单调清醒时间。
/// Update from one snapshot; pct<0 means invalid. awake_ms is monotonic awake time excluding lock sleep.
static inline battery_level_t battery_guard_update(battery_guard_state_t *s, int pct, bool charging,
                                                   int64_t now_ms, int64_t awake_ms) {
    if (charging) {
        // 充电立即解除所有限制并作废估算基准。/ Charging clears every restriction and the estimate baseline.
        bool keep_estimate = s->estimate_valid;
        uint32_t keep_rate = s->minutes_per_pct_x10;
        battery_guard_reset(s);
        s->estimate_valid = keep_estimate;
        s->minutes_per_pct_x10 = keep_rate;
        return s->level;
    }
    if (pct < 0 || pct > 100) {
        // 无效数据不升级等级，并取消危险确认。/ Invalid data never escalates and cancels critical confirmation.
        s->critical_since_ms = 0;
        if (s->level == BATTERY_LEVEL_CRITICAL) s->level = BATTERY_LEVEL_SAVER;
        return s->level;
    }
    battery_level_t next = battery_guard_classify(s->level, pct);
    if (next == BATTERY_LEVEL_CRITICAL) {
        if (!s->critical_since_ms) s->critical_since_ms = now_ms ? now_ms : 1;
        if (now_ms - s->critical_since_ms >= BATTERY_GUARD_CRITICAL_CONFIRM_MS) s->shutdown_due = true;
    } else {
        s->critical_since_ms = 0;
        s->shutdown_due = false;
    }
    s->level = next;
    // 续航：电量回升（例如校准跳变）时重设基准。/ Runtime: reset the baseline when SOC rises (gauge jumps).
    if (s->base_pct < 0 || pct > s->base_pct) {
        s->base_pct = pct;
        s->base_awake_ms = awake_ms;
    }
    if (pct != s->last_pct) {
        int drop = s->base_pct - pct;
        int64_t spent = awake_ms - s->base_awake_ms;
        if (drop >= BATTERY_GUARD_ESTIMATE_MIN_DROP && spent >= BATTERY_GUARD_ESTIMATE_MIN_AWAKE_MS) {
            s->minutes_per_pct_x10 = (uint32_t)(spent * 10 / 60000 / drop);
            s->estimate_valid = s->minutes_per_pct_x10 > 0;
        }
        s->last_pct = pct;
        s->last_awake_ms = awake_ms;
    }
    return s->level;
}

/// 剩余清醒阅读分钟；无足够样本或无效时返回-1。/ Remaining awake reading minutes; -1 without enough samples.
static inline int battery_guard_remaining_minutes_from(const battery_guard_state_t *s, int pct, bool charging) {
    if (charging || !s->estimate_valid || pct < 0 || pct > 100) return -1;
    return (int)((uint64_t)s->minutes_per_pct_x10 * (uint64_t)pct / 10u);
}

/// 是否允许开始固件升级（充电或电量无效时不阻止，交由原有流程）。
/// Whether a firmware update may start; charging or unknown data does not block.
static inline bool battery_guard_update_allowed(int pct, bool charging) {
    return charging || pct < 0 || pct >= BATTERY_GUARD_UPDATE_MIN_PCT;
}
