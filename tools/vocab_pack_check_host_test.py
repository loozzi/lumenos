#!/usr/bin/env python3
"""tools/vocab_pack_check.py follows the firmware pack rules on self-written samples.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import runpy
import tempfile

ROOT = Path(__file__).resolve().parents[1]
check = runpy.run_path(str(ROOT / 'tools/vocab_pack_check.py'))['check']

with tempfile.TemporaryDirectory() as d:
    d = Path(d)
    good = d / 'good.csv'
    good.write_text('﻿english,type,vietnamese,image_url\ninvoice,n.,hóa đơn,x\nInvoice,n.,trùng,x\n,n.,thiếu,x\n'
                    'budget,n.,"ngân sách, kinh phí",x\n\nagenda,n.,' + 'ư' * 80 + ',x\n', encoding='utf-8')
    errors, warnings, words = check(good)
    assert not errors and words == 3, (errors, words)
    assert any('image_url' in w for w in warnings)
    assert any('trùng dòng 2' in w for w in warnings)
    assert any('dòng 4' in w for w in warnings)
    assert any('vietnamese dài' in w for w in warnings)
    missing = d / 'missing.csv'
    missing.write_text('word,meaning\na,b\n')
    errors, _, words = check(missing)
    assert errors == ['thiếu cột english', 'thiếu cột vietnamese'] and not words
    latin = d / 'latin.csv'
    latin.write_bytes('english,vietnamese\nx,hóa\n'.encode('cp1258', errors='replace').replace(b'?', b'\xe9'))
    errors, _, _ = check(latin)
    assert errors and 'UTF-8' in errors[0]
    big = d / 'big.csv'
    big.write_text('english,vietnamese\n' + ''.join(f'w{i},n{i}\n' for i in range(4001)))
    errors, warnings, words = check(big)
    assert not errors and words == 4000 and any('4000' in w for w in warnings)
    empty = d / 'empty.csv'
    empty.write_text('english,vietnamese\n,\n')
    assert check(empty)[0] == ['không có dòng từ hợp lệ']
print('PASS vocab_pack_check: header, duplicates, missing fields, long fields, encoding and word-limit rules')
