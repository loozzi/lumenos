/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：低电量保护运行状态；只在S31启用，Pico保持原行为。
 *
 * English: runtime low-battery guard; enabled on S31 only, Pico keeps its behavior.
 */
#include "battery_guard.h"
#include "dpreader_board_config.h"
#include "read_pico_pmu.h"
#if DPREADER_BOARD_METALIO47
#include "metalio_hw.h"
#endif

static battery_guard_state_t s_guard = {.base_pct = -1, .last_pct = -1};
static int64_t s_shutdown_retry_at_ms;
static bool s_cap_applied;

void battery_guard_poll(int64_t now_ms, int64_t awake_ms) {
#if DPREADER_BOARD_METALIO47
    const pmu_snapshot_t *pmu = read_pico_pmu_get();
    const int pct = pmu_battery_percent(pmu);
    const bool charging = pmu_battery_charging(pmu);
    (void)battery_guard_update(&s_guard, pct, charging, now_ms, awake_ms);
    // 只在等级跨越省电线时改上限，避免每轮重写LEDC。/ Change the cap only when crossing the saver line, never rewriting LEDC each pass.
    const bool cap = s_guard.level >= BATTERY_LEVEL_SAVER;
    if (cap != s_cap_applied) {
        s_cap_applied = cap;
        metalio_frontlight_set_cap(cap ? BATTERY_GUARD_SAVER_LIGHT_CAP : 100);
    }
#else
    (void)now_ms;
    (void)awake_ms;
#endif
}

battery_level_t battery_guard_level(void) { return s_guard.level; }

bool battery_guard_shutdown_due(int64_t now_ms) {
    return s_guard.shutdown_due && now_ms >= s_shutdown_retry_at_ms;
}

void battery_guard_shutdown_deferred(int64_t now_ms) { s_shutdown_retry_at_ms = now_ms + 60000; }

unsigned battery_guard_auto_lock_cap(void) { return s_guard.level >= BATTERY_LEVEL_SAVER ? 1u : 0u; }

int battery_guard_remaining_minutes(void) {
#if DPREADER_BOARD_METALIO47
    const pmu_snapshot_t *pmu = read_pico_pmu_get();
    return battery_guard_remaining_minutes_from(&s_guard, pmu_battery_percent(pmu), pmu_battery_charging(pmu));
#else
    return -1;
#endif
}

bool battery_guard_update_ok(void) {
#if DPREADER_BOARD_METALIO47
    const pmu_snapshot_t *pmu = read_pico_pmu_get();
    return battery_guard_update_allowed(pmu_battery_percent(pmu), pmu_battery_charging(pmu));
#else
    return true;
#endif
}

int64_t battery_guard_lock_off_cap_ms(void) {
    // 危险级锁屏后尽快走既有关机；省电级最多浅睡10分钟。
    // Critical locks hand off to the existing power-off promptly; saver light-sleeps at most ten minutes.
    if (s_guard.level >= BATTERY_LEVEL_CRITICAL) return 5000;
    if (s_guard.level >= BATTERY_LEVEL_SAVER) return 10LL * 60 * 1000;
    return 0;
}
