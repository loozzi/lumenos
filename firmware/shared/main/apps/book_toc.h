/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：阅读目录独立页面的布局、绘制与命中定义。
 * English: Layout, rendering and hit testing for the standalone reading directory.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "epdiy.h"

#define BOOK_TOC_ROWS 7
#define BOOK_TOC_BACK (-2)
#define BOOK_TOC_PREV (-3)
#define BOOK_TOC_NEXT (-4)
#define BOOK_TOC_SKIP_PREV (-5)
#define BOOK_TOC_SKIP_NEXT (-6)
#define BOOK_TOC_JUMP (-7)
#define BOOK_TOC_JUMP_CANCEL (-8)
#define BOOK_TOC_JUMP_CONFIRM (-9)
#define BOOK_TOC_JUMP_TRACK (-10)
/// 点按父级目录项左侧箭头：折叠/展开该项。/ Tap on a parent's left marker: collapse or expand it.
#define BOOK_TOC_TOGGLE_BASE (-1000)
#define BOOK_TOC_TOGGLE(nav) (BOOK_TOC_TOGGLE_BASE - (int)(nav))
#define BOOK_TOC_IS_TOGGLE(hit) ((hit) <= BOOK_TOC_TOGGLE_BASE)
#define BOOK_TOC_TOGGLE_ENTRY(hit) ((size_t)(BOOK_TOC_TOGGLE_BASE - (hit)))

/// 打开目录：除当前项的上级外折叠所有父级；未调用时全部展开。
/// Open the TOC: collapse every parent except the current entry's ancestors; without a call all rows show.
void book_toc_view_open(size_t chapters, size_t current);
/// 切换父级项的折叠状态；非父级忽略。/ Toggle a parent's collapsed state; non-parents are ignored.
void book_toc_toggle(size_t chapters, size_t entry);
/// 可见行数与目录项所在页（隐藏项取最近可见上级）。/ Visible rows and the page of an entry (a hidden entry maps to its nearest visible ancestor).
size_t book_toc_visible_count(size_t chapters);
int book_toc_page_of(size_t chapters, size_t entry);
int book_toc_pages(size_t chapters);
int book_toc_hit(uint16_t x, uint16_t y, int page, size_t chapters);
int book_toc_jump_hit(uint16_t x, uint16_t y);
int book_toc_jump_percent(uint16_t x);
int book_toc_page_from_percent(int percent, int pages);
EpdRect book_toc_row_rect(int row);
void book_toc_render(uint8_t *fb, const char *book_title, size_t chapters,
                     size_t current, int page, const char *message);
void book_toc_render_jump(uint8_t *fb, size_t chapters, int percent);
