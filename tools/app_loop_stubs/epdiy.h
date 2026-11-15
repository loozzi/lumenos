/* 中文：调度测试替身。/ English: Scheduler test shim. */
#pragma once
#include "common.h"
#ifndef APP_LOOP_STUB_ROTATION
#define APP_LOOP_STUB_ROTATION
enum EpdRotation { EPD_ROT_LANDSCAPE, EPD_ROT_PORTRAIT, EPD_ROT_INVERTED_LANDSCAPE, EPD_ROT_INVERTED_PORTRAIT };
static inline enum EpdRotation epd_get_rotation(void) { return EPD_ROT_INVERTED_PORTRAIT; }
static inline void epd_set_rotation(enum EpdRotation rotation) { (void)rotation; }
#endif
