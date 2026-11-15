#!/usr/bin/env bash
# 在S31宿主视图运行网络/省电/日志回归。/ Run network, idle-power and diagnostic regressions in an S31 host view.
set -euo pipefail
cd "$(dirname "$0")/.."
for test in wifi_profiles_host_test.py cpu_policy_host_test.py cpu_policy_concurrency_host_test.py \
    ble_reconnect_backoff_host_test.py lcd_sleep_retention_host_test.py \
    metalio_light_transaction_host_test.py crash_note_host_test.py log_export_host_test.py \
    metalio_status_host_test.py reader_turn_antialias_host_test.py; do
    python3 "tools/$test"
done
