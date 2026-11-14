/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define CARO_SIZE 15
#define CARO_CELLS (CARO_SIZE * CARO_SIZE)
typedef struct {
    uint8_t cells[CARO_CELLS];
    uint8_t history[CARO_CELLS];
    uint16_t moves;
    uint8_t turn, winner;
} caro_t;
void caro_reset(caro_t *game);
bool caro_play(caro_t *game, int row, int col);
bool caro_undo(caro_t *game);
bool caro_finished(const caro_t *game);
