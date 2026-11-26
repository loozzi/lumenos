#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：页脚外观枚举迁移与可见性。/ English: Footer style enum migration and visibility."""
from pathlib import Path
import re, subprocess, tempfile

ROOT = Path(__file__).resolve().parents[1]
SETTINGS = ROOT / 'main/settings.c'
HEADER = ROOT / 'main/settings.h'


def function(name: str, path: Path) -> str:
    text = path.read_text()
    scan = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"',
                  lambda m: ' ' * len(m[0]), text)
    start = re.search(r'^(?:static )?(?:[A-Za-z_]\w*(?:\s+|\s*\*\s*))+' + name +
                      r'\([^;{}]*?\)\s*\{', scan, re.M)
    assert start, f'function not found: {name}'
    at, depth = start.end(), 1
    while depth:
        depth += (scan[at] == '{') - (scan[at] == '}')
        at += 1
    return text[start.start():at]


# Pull the real enum constants out of the header so the test cannot drift from them.
header = HEADER.read_text()
defs = dict(re.findall(r'#define (APP_BOOK_PROGRESS_STYLE_\w+)\s+(\d+)', header))
assert defs, 'progress style defines not found in settings.h'
define_block = '\n'.join(f'#define {k} {v}' for k, v in defs.items())

HARNESS = r'''
#include <stdio.h>
#include <stdint.h>

''' + define_block + r'''

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); ++failures; } } while (0)

''' + function('book_progress_style_from_legacy', SETTINGS) + r'''

int main(void) {
    // 旧存档 0..2 依次对应 细/粗/极细，迁移后必须整体 +1。
    // Legacy 0..2 meant thin/thick/hairline, so migration shifts each by one.
    CHECK(book_progress_style_from_legacy(0) == APP_BOOK_PROGRESS_STYLE_THIN,
          "legacy 0 (thin) -> %u", book_progress_style_from_legacy(0));
    CHECK(book_progress_style_from_legacy(1) == APP_BOOK_PROGRESS_STYLE_THICK,
          "legacy 1 (thick) -> %u", book_progress_style_from_legacy(1));
    CHECK(book_progress_style_from_legacy(2) == APP_BOOK_PROGRESS_STYLE_HAIRLINE,
          "legacy 2 (hairline) -> %u", book_progress_style_from_legacy(2));

    // 未知的旧值回落到细条，绝不能被读成“隐藏”。
    // Unknown legacy values fall back to thin and must never read as hidden.
    for (unsigned v = 3; v < 256; ++v) {
        uint8_t got = book_progress_style_from_legacy((uint8_t)v);
        CHECK(got == APP_BOOK_PROGRESS_STYLE_THIN, "legacy %u -> %u (expected thin)", v, got);
    }

    // 隐藏档是 0，迁移函数绝不产出它；否则老用户开机就会丢页脚。
    // Hidden is 0 and migration never produces it, or existing users would lose the footer.
    for (unsigned v = 0; v < 256; ++v)
        CHECK(book_progress_style_from_legacy((uint8_t)v) != APP_BOOK_PROGRESS_STYLE_HIDDEN,
              "migration produced hidden from legacy %u", v);

    // 新枚举顺序固定：隐藏在最前，其余依次变粗再变细；1.5.15 追加贴底条。
    // The new enum order is fixed: hidden first, then progressively thinner bars; 1.5.15 appends the bottom bar.
    CHECK(APP_BOOK_PROGRESS_STYLE_HIDDEN == 0, "hidden must be 0");
    CHECK(APP_BOOK_PROGRESS_STYLE_BOTTOM == 4, "bottom must be 4");
    CHECK(APP_BOOK_PROGRESS_STYLE_COUNT == 5, "count must be 5");

    if (failures) { printf("\n%d check(s) failed\n", failures); return 1; }
    puts("PASS: legacy footer styles shift to the new enum and never collapse to hidden");
    return 0;
}
'''

with tempfile.TemporaryDirectory() as folder:
    work = Path(folder)
    src = work / 'test.c'
    src.write_text(HARNESS)
    binary = work / 'test'
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    str(src), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

# 中文：直接执行真实页脚绘制，遍历所有开关和条宽，检查文字、对齐及正文边界。
# English: Execute the real footer for every toggle/style combination, checking labels, alignment and body bounds.
BOOK = ROOT / 'main/apps/app_book.c'
source = BOOK.read_text()
footer_defs = '\n'.join(re.findall(r'^#define READER_FOOTER_.*', source, re.M))
outer = function('draw_reader_outer', BOOK)
assert 'draw_reader_footer(fb, page);' in outer
assert 's_reader_fullscreen' not in source
assert 'toggle_reader_fullscreen' not in source
assert 'draw_reader_header' not in source
assert '(EpdRect){32, 75, 64, 64}' not in function('action_at', BOOK)
assert 'app_settings_reader_immersive' not in source
# 1.5.15 起页脚改为单行（章名 + 页码/时间/百分比），旧的多样式进度条绘制已删除；
# 这里只保留枚举迁移与“全屏/页眉已移除”的源码断言。
# Since 1.5.15 the footer is one row (chapter name + page/time/percent) and the old multi-style bar renderer
# was removed; only the enum migration and the removed full-screen/header source checks remain here.
print('PASS: footer style migration and removed full-screen/header hooks')
