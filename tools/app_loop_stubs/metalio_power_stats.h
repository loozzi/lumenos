/* SPDX-License-Identifier: Apache-2.0 */
/* 中文：主机测试用的电量/延迟计数空桩；真实实现见 components/metalio_hw/metalio_power_stats.c，
 * 由 tools/power_stats_host_test.py 单独验证。
 * English: no-op power/latency counter stubs for host tests; the real implementation in
 * components/metalio_hw/metalio_power_stats.c is verified by tools/power_stats_host_test.py. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { METALIO_LATENCY_TURN = 0, METALIO_LATENCY_UI, METALIO_LATENCY_UNLOCK, METALIO_LATENCY_BOOT, METALIO_LATENCY_COUNT } metalio_latency_kind_t;
typedef enum { METALIO_SCAN_GC = 0, METALIO_SCAN_GL, METALIO_SCAN_DU, METALIO_SCAN_KINDS } metalio_scan_kind_t;
static inline void metalio_power_note_wait(bool idle_allowed, uint32_t elapsed_ms) { (void)idle_allowed; (void)elapsed_ms; }
static inline void metalio_power_note_scan(metalio_scan_kind_t kind) { (void)kind; }
static inline void metalio_power_note_light_sleep(uint32_t elapsed_ms, unsigned wakes) { (void)elapsed_ms; (void)wakes; }
static inline void metalio_power_note_lamp(unsigned brightness, int64_t now_us) { (void)brightness; (void)now_us; }
static inline void metalio_power_note_lock(bool locking, int soc, unsigned mv) { (void)locking; (void)soc; (void)mv; }
static inline void metalio_power_note_latency(metalio_latency_kind_t kind, uint32_t elapsed_ms) { (void)kind; (void)elapsed_ms; }
static inline void metalio_power_mark_unlock(int64_t now_us) { (void)now_us; }
static inline int64_t metalio_power_take_unlock_mark(void) { return 0; }
