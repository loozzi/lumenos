#!/usr/bin/env python3
"""Check a LumenOS vocabulary CSV before copying it to /vocab on the card.
SPDX-License-Identifier: Apache-2.0

中文：在电脑上按固件同样的规则检查词汇包：UTF-8、表头必须有 english 与 vietnamese、字段长度上限、
重复（不分 ASCII 大小写）、缺必填字段的行、4000 词与 2 MiB 上限。只读，不改文件。
English: check a vocabulary pack on a computer with the firmware's rules: UTF-8, a header with english
and vietnamese, field byte limits, duplicates (ASCII case-insensitive), rows missing required fields,
the 4000-word and 2 MiB limits. Read-only; the file is never modified.

Usage: python3 tools/vocab_pack_check.py my_words.csv [more.csv ...]
"""
import csv
import io
import sys
from pathlib import Path

LIMITS = {'english': 64, 'type': 16, 'vietnamese': 160, 'pronounce': 64,
          'explain': 240, 'example': 240, 'example_vietnamese': 240}
MAX_WORDS = 4000
MAX_BYTES = 2 * 1024 * 1024


def check(path):
    """Return (errors, warnings, word_count)."""
    errors, warnings = [], []
    raw = Path(path).read_bytes()
    if len(raw) > MAX_BYTES:
        return [f'tệp {len(raw)} byte, quá giới hạn 2 MiB'], warnings, 0
    try:
        text = raw.decode('utf-8-sig')
    except UnicodeDecodeError as e:
        return [f'không phải UTF-8 (byte {e.start})'], warnings, 0
    rows = csv.reader(io.StringIO(text, newline=''))
    try:
        header = [h.strip().lower() for h in next(rows)]
    except StopIteration:
        return ['tệp trống'], warnings, 0
    for required in ('english', 'vietnamese'):
        if required not in header:
            errors.append(f'thiếu cột {required}')
    if errors:
        return errors, warnings, 0
    unknown = [h for h in header if h and h not in LIMITS and h not in ('rank',)]
    if unknown:
        warnings.append('bỏ qua các cột: ' + ', '.join(unknown))
    seen, words = {}, 0
    for line, row in enumerate(rows, start=2):
        if not any(c.strip() for c in row):
            continue
        record = {h: (row[i].strip() if i < len(row) else '') for i, h in enumerate(header)}
        if not record['english'] or not record['vietnamese']:
            warnings.append(f'dòng {line}: thiếu english hoặc vietnamese, sẽ bị bỏ qua')
            continue
        key = record['english'].encode().lower()
        if key in seen:
            warnings.append(f'dòng {line}: "{record["english"]}" trùng dòng {seen[key]}, sẽ bị bỏ qua')
            continue
        seen[key] = line
        for field, limit in LIMITS.items():
            value = ' '.join(record.get(field, '').split())
            if len(value.encode()) >= limit:
                warnings.append(f'dòng {line}: {field} dài {len(value.encode())} byte, sẽ bị cắt còn {limit} byte')
        words += 1
        if words == MAX_WORDS + 1:
            warnings.append(f'quá {MAX_WORDS} từ; chỉ dùng {MAX_WORDS} từ đầu')
    if not words:
        errors.append('không có dòng từ hợp lệ')
    return errors, warnings, min(words, MAX_WORDS)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    failed = False
    for path in argv[1:]:
        errors, warnings, words = check(path)
        status = 'LỖI' if errors else 'OK'
        print(f'{status} {path}: {words} từ')
        for e in errors:
            print('  lỗi: ' + e)
        for w in warnings[:50]:
            print('  lưu ý: ' + w)
        if len(warnings) > 50:
            print(f'  … và {len(warnings) - 50} lưu ý khác')
        failed |= bool(errors)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
