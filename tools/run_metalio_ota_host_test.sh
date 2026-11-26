#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
sdk_path="${METALIO_IDF_PATH:-${IDF_PATH:-$HOME/esp/v6.1}}"
mkdir -p build/metalio-tests
cc -std=gnu11 -D_POSIX_C_SOURCE=200809L -DIDF_VER=\"host\" -DCONFIG_ESP_EFUSE_BLOCK_REV_MIN_FULL=0 -DCONFIG_ESP_EFUSE_BLOCK_REV_MAX_FULL=0 -DCONFIG_MMU_PAGE_SIZE=0x10000 -DCONFIG_ESPTOOLPY_FLASHMODE_VAL=0 -DCONFIG_ESPTOOLPY_FLASHFREQ_VAL=0 -DCONFIG_ESPTOOLPY_FLASHSIZE_VAL=0 -Wall -Wextra -Werror -Wno-unused-variable -g \
    -fsanitize=address,undefined -Itools/metalio_stubs -Imain \
    -I"$sdk_path/components/esp_app_format/include" -I"$sdk_path/components/bootloader_support/include" \
    tools/metalio_ota_host_test.c main/ota_update.c \
    -o build/metalio-tests/ota-guard
build/metalio-tests/ota-guard
