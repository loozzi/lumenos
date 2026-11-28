/* SPDX-License-Identifier: Apache-2.0 */
/*
 * 中文：S31 电量与延迟计数；只做累加，不影响任何决策。
 * English: S31 power and latency counters; accumulation only, no decisions.
 */
#include "metalio_power_stats.h"
#include <string.h>

const uint16_t metalio_latency_bounds_ms[METALIO_LATENCY_BUCKETS - 1] = {50, 100, 200, 400, 800, 1600};

static metalio_power_stats_t s_stats = {.lock_soc = -1, .unlock_soc = -1};
static unsigned s_lamp_level;
static int64_t s_lamp_since_us;
static int64_t s_unlock_mark_us;

void metalio_power_note_wait(bool idle_allowed, uint32_t elapsed_ms) {
    if (idle_allowed) { s_stats.wait_idle_ms += elapsed_ms; ++s_stats.waits_idle; }
    else { s_stats.wait_legacy_ms += elapsed_ms; ++s_stats.waits_legacy; }
}

void metalio_power_note_scan(metalio_scan_kind_t kind) {
    if ((unsigned)kind < METALIO_SCAN_KINDS) ++s_stats.scans[kind];
}

void metalio_power_note_light_sleep(uint32_t elapsed_ms, unsigned maintenance_wakes) {
    ++s_stats.light_sleeps;
    s_stats.light_sleep_ms += elapsed_ms;
    s_stats.maintenance_wakes += maintenance_wakes;
}

// 结算到 now 为止的前光区间。/ Close the front-light interval up to now.
static void lamp_settle(int64_t now_us) {
    if (s_lamp_level && s_lamp_since_us && now_us > s_lamp_since_us) {
        const uint64_t ms = (uint64_t)(now_us - s_lamp_since_us) / 1000u;
        s_stats.lamp_on_ms += ms;
        s_stats.lamp_level_ms += ms * s_lamp_level / 100u;
    }
    s_lamp_since_us = now_us;
}

void metalio_power_note_lamp(unsigned brightness_percent, int64_t now_us) {
    if (brightness_percent > 100) brightness_percent = 100;
    lamp_settle(now_us);
    s_lamp_level = brightness_percent;
}

void metalio_power_note_lock(bool locking, int soc, unsigned mv) {
    if (soc < -1 || soc > 100) soc = -1;
    if (mv > 65535u) mv = 0;
    if (locking) { ++s_stats.locks; s_stats.lock_soc = (int16_t)soc; s_stats.lock_mv = (uint16_t)mv; }
    else { s_stats.unlock_soc = (int16_t)soc; s_stats.unlock_mv = (uint16_t)mv; }
}

void metalio_power_note_latency(metalio_latency_kind_t kind, uint32_t elapsed_ms) {
    if ((unsigned)kind >= METALIO_LATENCY_COUNT) return;
    unsigned bucket = 0;
    while (bucket < METALIO_LATENCY_BUCKETS - 1 && elapsed_ms >= metalio_latency_bounds_ms[bucket]) ++bucket;
    ++s_stats.latency[kind][bucket];
    s_stats.last_ms[kind] = elapsed_ms;
    if (elapsed_ms > s_stats.max_ms[kind]) s_stats.max_ms[kind] = elapsed_ms;
}

void metalio_power_mark_unlock(int64_t now_us) { s_unlock_mark_us = now_us ? now_us : 1; }

int64_t metalio_power_take_unlock_mark(void) {
    const int64_t mark = s_unlock_mark_us;
    s_unlock_mark_us = 0;
    return mark;
}

metalio_power_stats_t metalio_power_stats(int64_t now_us) {
    lamp_settle(now_us);
    return s_stats;
}

uint32_t metalio_power_percentile(const uint32_t buckets[METALIO_LATENCY_BUCKETS], unsigned percent) {
    uint64_t total = 0;
    for (unsigned i = 0; i < METALIO_LATENCY_BUCKETS; ++i) total += buckets[i];
    if (!total) return 0;
    if (percent > 100) percent = 100;
    // 取第 ceil(total*p/100) 个样本所在的格。/ Pick the bucket holding sample ceil(total*p/100).
    uint64_t rank = (total * percent + 99u) / 100u;
    if (!rank) rank = 1;
    uint64_t seen = 0;
    for (unsigned i = 0; i < METALIO_LATENCY_BUCKETS; ++i) {
        seen += buckets[i];
        if (seen >= rank) return i < METALIO_LATENCY_BUCKETS - 1 ? metalio_latency_bounds_ms[i] : UINT32_MAX;
    }
    return UINT32_MAX;
}
