/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：低电量保护的运行状态：主循环定期喂入电量快照，其他模块只读取等级。
 *
 * English: runtime low-battery guard. The UI loop feeds gauge snapshots; other
 * modules only read the level.
 *
 * 冻结：本模块不自行关机、不画屏；关机由主循环按既有关机流程执行。
 * Frozen: this module never powers off or draws; the loop runs the existing shutdown path.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "battery_guard_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 主循环调用：读取PMU快照并更新等级与省电前光上限。awake_ms不含锁屏睡眠。
/// Called by the UI loop: read the PMU snapshot, update the level and saver light cap. awake_ms excludes lock sleep.
void battery_guard_poll(int64_t now_ms, int64_t awake_ms);
/// 当前等级。/ Current level.
battery_level_t battery_guard_level(void);
/// 已确认需要低电量关机且未在退避期。/ Confirmed low-battery shutdown, outside the retry backoff.
bool battery_guard_shutdown_due(int64_t now_ms);
/// 关机交接失败后退避再试。/ Back off after a failed shutdown handoff.
void battery_guard_shutdown_deferred(int64_t now_ms);
/// 省电级时自动锁屏分钟数的上限；0表示不限制。/ Auto-lock cap in minutes while saving; 0 means no cap.
unsigned battery_guard_auto_lock_cap(void);
/// 估算剩余阅读分钟；样本不足返回-1。/ Estimated remaining reading minutes; -1 without enough samples.
int battery_guard_remaining_minutes(void);
/// 是否允许开始固件升级。/ Whether a firmware update may start.
bool battery_guard_update_ok(void);
/// 待提示的低电量等级（提示/省电各一次，充电或回升后重置）；无则为 NORMAL。
/// Low-battery level awaiting a one-time notice (warning and saver once each, reset on charge or recovery); NORMAL when none.
battery_level_t battery_guard_pending_notice(void);
/// 确认已显示该等级的提示。/ Acknowledge that the notice for this level was shown.
void battery_guard_ack_notice(battery_level_t level);
/// 锁屏时关机延迟的上限（毫秒）；0表示沿用用户设定。
/// Lock power-off delay cap in milliseconds; 0 keeps the user's setting.
int64_t battery_guard_lock_off_cap_ms(void);

#ifdef __cplusplus
}
#endif
