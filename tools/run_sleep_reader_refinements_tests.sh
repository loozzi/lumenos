#!/usr/bin/env bash
# Run from a metalio47 host view generated with board_source_view.py --tests.
set -euo pipefail
mkdir -p build
for test in \
    sleep_reader_refinements_host_test.py \
    wake_preferences_host_test.py \
    lock_ticket_layout_host_test.py \
    ui_hanzi_host_test.py \
    font_ink_host_test.py \
    font_mono_cache_host_test.py \
    metalio_lock_key_host_test.py \
    metalio_sleep_host_test.py \
    metalio_deep_sleep_host_test.py \
    metalio_sleep_io_host_test.py \
    sleep_only_settings_host_test.py \
    wallpaper_cache_host_test.py \
    book_ticket_host_test.py \
    log_export_host_test.py; do
    python3 "tools/$test"
done
