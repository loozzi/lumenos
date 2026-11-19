/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t version, file_size;
    uint64_t reading_ms;
    uint32_t finished, reserved;
} book_reading_state_t;

/* Profile records use the checked store and existing history backup/forget paths.
 * Missing/replaced files start fresh; legacy 100% progress remains completed.
 * Corrupt/unknown records return an error and must never be overwritten. */
esp_err_t book_reading_state_load(const char *path, uint32_t size, unsigned legacy_pct,
                                 book_reading_state_t *out);
esp_err_t book_reading_state_save(const char *path, const book_reading_state_t *state);
