/* SPDX-License-Identifier: Apache-2.0 */
#include "book_reading_state.h"
#include "book_state_store.h"
#include <string.h>

#define READING_STATE_VERSION UINT32_C(0x3153524c)

static bool valid(const book_reading_state_t *state) {
    return state->version == READING_STATE_VERSION && state->finished <= 1 && !state->reserved;
}
esp_err_t book_reading_state_load(const char *path, uint32_t size, unsigned legacy_pct,
                                 book_reading_state_t *out) {
    if (!path || !out) return ESP_ERR_INVALID_ARG;
    *out = (book_reading_state_t){.version=READING_STATE_VERSION, .file_size=size,
                                 .finished=legacy_pct >= 100};
    book_reading_state_t stored;
    size_t length = sizeof(stored);
    esp_err_t err = book_state_read(book_state_root(path), path, "profile", &stored, &length);
    if (err == ESP_ERR_NOT_FOUND || (err == ESP_OK && !length)) return ESP_OK;
    if (err != ESP_OK) return err;
    if (length != sizeof(stored) || !valid(&stored)) return ESP_ERR_INVALID_RESPONSE;
    if (stored.file_size == size) *out = stored;
    return ESP_OK;
}
esp_err_t book_reading_state_save(const char *path, const book_reading_state_t *state) {
    if (!path || !state || !valid(state)) return ESP_ERR_INVALID_ARG;
    return book_state_write(book_state_root(path), path, "profile", state, sizeof(*state));
}
