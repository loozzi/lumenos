#!/usr/bin/env bash
# Run from a Metalio47 host view generated with board_source_view.py --tests.
set -euo pipefail
mkdir -p build/book-tests/settings-card
bash tools/run_app_loop_host_tests.sh
cc -std=gnu11 -Wall -Wextra -Werror \
    -Itools/settings_backup_stubs -Icomponents/read_pico_transfer/include \
    tools/settings_backup_host_test.c -o build/book-tests/settings-backup
build/book-tests/settings-backup
python3 tools/test_home_recent.py
python3 tools/test_home_minute_present.py
python3 tools/quick_layer_raster_host_test.py
python3 tools/reader_layout_choices_host_test.py
python3 tools/metalio_transfer_stream_host_test.py
