#!/usr/bin/env python3
"""Sparse StarDict index: indexed lookups must match the sequential oracle.
SPDX-License-Identifier: Apache-2.0

中文：用真实 book_dictionary.c 建 20 万词的普通/gzip 索引，随机查询（含大小写变体、越南语、
缺失词、首尾边界、同义词），首查顺序扫描建索引，再查必须与 Python 顺序参照一致且只需少量调用；
未排序、超限 gzip、文件变化与中途取消都退回旧路径且不泄漏。
English: build plain/gzip 200k-word indexes with the real C reader; random queries (case variants,
Vietnamese, missing words, first/last bounds, synonyms) must match a sequential Python oracle, and
indexed repeats need few calls. Unsorted, oversized gzip, changed files and cancellation fall back
safely without leaks.
"""
from pathlib import Path
import ctypes as C
import gzip
import os
import random
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BOOK = ROOT / 'main/book'
API = r'''
#include "book_dictionary.h"
#include <stdlib.h>
#include <string.h>
book_dictionary_t *probe_new(void){return calloc(1,sizeof(book_dictionary_t));}
void probe_delete(book_dictionary_t *d){book_dictionary_close(d);free(d);}
const char *probe_text(book_dictionary_t *d){return d->text;}
unsigned probe_suggestions(book_dictionary_t *d){return d->suggestion_count;}
const char *probe_suggestion(book_dictionary_t *d,unsigned i){return d->suggestions[i];}
int probe_handles(book_dictionary_t *d){return !!d->index+!!d->data+!!d->syn+!!d->cache+!!d->slice;}
'''


def lower(s):
    return bytes(b + 32 if 65 <= b <= 90 else b for b in s)


def stardict_key(word):
    return (lower(word), word)


def build_library(work):
    (work / 'api.c').write_text(API)
    lib = work / 'reader.so'
    subprocess.run(['cc', '-std=gnu11', '-O1', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC', '-I' + str(BOOK),
                    str(work / 'api.c'), str(BOOK / 'book_dictionary.c'), str(BOOK / 'book_dict_file.c'), '-lz',
                    '-o', str(lib)], check=True)
    lib = C.CDLL(str(lib))
    P = C.c_void_p
    lib.probe_new.restype = P
    lib.probe_delete.argtypes = [P]
    lib.probe_text.argtypes = [P]
    lib.probe_text.restype = C.c_char_p
    lib.probe_suggestions.argtypes = [P]
    lib.probe_suggestion.argtypes = [P, C.c_uint]
    lib.probe_suggestion.restype = C.c_char_p
    lib.probe_handles.argtypes = [P]
    lib.book_dictionary_open.argtypes = [P, C.c_char_p, C.c_char_p]
    lib.book_dictionary_open.restype = C.c_bool
    lib.book_dictionary_step.argtypes = [P]
    lib.book_dictionary_indexed.argtypes = [P]
    lib.book_dictionary_indexed.restype = C.c_bool
    lib.book_dictionary_cache_bytes.restype = C.c_size_t
    return lib


def write_dictionary(base, words, packed, synonyms=(), sort=True):
    """Write a StarDict set; returns the in-order record list [(key, definition)]."""
    if sort:
        words = sorted(words, key=stardict_key)
    data = bytearray()
    idx = bytearray()
    records = []
    for i, w in enumerate(words):
        definition = b'def:' + str(i).encode()
        idx += w + b'\0' + struct.pack('>II', len(data), len(definition))
        records.append((w, definition))
        data += definition
    for ext in ('.idx', '.idx.gz', '.syn'):
        Path(str(base) + ext).unlink(missing_ok=True)
    Path(str(base) + ('.idx.gz' if packed else '.idx')).write_bytes(gzip.compress(bytes(idx), 1) if packed else bytes(idx))
    Path(str(base) + '.dict').write_bytes(bytes(data))
    if synonyms:
        Path(str(base) + '.syn').write_bytes(b''.join(k + b'\0' + struct.pack('>I', t) for k, t in synonyms))
    Path(str(base) + '.ifo').write_text("StarDict's dict ifo file\nversion=3.0.0\nbookname=index fixture\nwordcount=" +
                                        str(len(words)) + "\nidxfilesize=" + str(len(idx)) + "\nsametypesequence=m\n")
    return records


_ORACLE = {}


def oracle(records, synonyms, query):
    """First record whose ASCII-lowercased key matches, else the first matching synonym."""
    key = (id(records), len(records), records[0] if records else None)
    table = _ORACLE.get(key)
    if table is None:
        table = {}
        for w, definition in records:
            table.setdefault(lower(w), definition)
        aliases = {}
        for k, t in synonyms:
            aliases.setdefault(lower(k), records[t][1])
        table = _ORACLE[key] = (table, aliases)
    q = lower(query)
    return table[0].get(q, table[1].get(q))


def lookup(lib, ifo, query):
    d = lib.probe_new()
    assert lib.book_dictionary_open(d, os.fsencode(ifo), query)
    calls = 0
    while True:
        state = lib.book_dictionary_step(d)
        calls += 1
        if state == 4 or state < 0:
            break
        assert calls < 20000, query
    text = lib.probe_text(d) if state == 4 else None
    suggestions = [lib.probe_suggestion(d, i) for i in range(lib.probe_suggestions(d))]
    indexed = lib.book_dictionary_indexed(d)
    assert state in (4, -2), (query, state)
    lib.probe_delete(d)
    return text, calls, suggestions, indexed


def vocabulary(rng, n):
    syllables = [b'ba', b'ch', b'do', b'gi', b'kh', b'ng', b'qu', b'th', b'tr', b'xa', b'an', b'em']
    vietnamese = ['nhà', 'đường', 'sách', 'Tiếng', 'học', 'ước', 'Đà']
    words = set()
    while len(words) < n:
        w = b''.join(rng.choice(syllables) for _ in range(rng.randint(1, 5))) + str(rng.randint(0, 999)).encode()
        r = rng.random()
        if r < 0.05:
            w = w.capitalize()
        elif r < 0.07:
            w = w.upper()
        elif r < 0.09:
            w = (rng.choice(vietnamese) + str(rng.randint(0, 9999))).encode()
        elif r < 0.0905:
            w = b'z' * rng.randint(48, 90) + str(rng.randint(0, 99)).encode()  # long keys compare as unknown
        words.add(w)
    words = list(words)
    # 大小写变体彼此相邻。/ Case variants sit next to each other.
    for w in rng.sample(words, 300):
        words.append(w.upper() if w != w.upper() else w.lower())
    return list(dict.fromkeys(words))


def main():
    rng = random.Random(20261125)
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        lib = build_library(work)
        words = vocabulary(rng, 200000)
        for packed in (False, True):
            base = work / ('packed' if packed else 'plain')
            synonyms = [(b'alias' + str(i).encode(), rng.randrange(len(words))) for i in range(50)]
            records = write_dictionary(base, words, packed, sorted(synonyms, key=lambda s: stardict_key(s[0])))
            synonyms = sorted(synonyms, key=lambda s: stardict_key(s[0]))
            ifo = Path(str(base) + '.ifo')
            keys = [w for w, _ in records]
            queries = [keys[0], keys[-1], b'!', b'\xff\xff', keys[0].upper(), keys[-1].lower(), b'zzzz', b'aaa']
            queries += [rng.choice(keys) for _ in range(1200)]
            queries += [rng.choice(keys).swapcase() for _ in range(300)]
            queries += [rng.choice(keys) + b'x' for _ in range(300)]
            queries += [s[0] for s in synonyms[:20]] + [s[0].upper() for s in synonyms[20:30]]
            queries += [b'z' * 60, 'đường'.encode(), 'ĐƯỜNG'.encode()]
            first_calls = []
            for q in queries:
                text, calls, _, _ = lookup(lib, ifo, q)
                assert text == oracle(records, synonyms, q), (packed, q, text)
                first_calls.append(calls)
            # 全部查询过后索引完整。/ After the pass, the index is complete.
            _, _, _, indexed = lookup(lib, ifo, b'missing-word')
            assert indexed, packed
            used = lib.book_dictionary_cache_bytes()
            assert 0 < used <= 256 * 1024 + 4 * 1024 * 1024, used
            worst = 0
            for q in queries:
                text, calls, suggestions, indexed = lookup(lib, ifo, q)
                assert indexed and text == oracle(records, synonyms, q), (packed, q, text)
                worst = max(worst, calls)
                if text is None and not q.startswith(b'alias'):
                    assert suggestions and len(suggestions) <= 5, q
                    assert all(s in keys for s in suggestions), suggestions
            assert worst <= 4, (packed, worst)
            late = keys[-1]
            print(f'{"gzip" if packed else "plain"}: 200k records, first pass max {max(first_calls)} calls, '
                  f'indexed max {worst} calls, index bytes {used}')
            _, calls, _, _ = lookup(lib, ifo, late)
            assert calls <= 4
        # 未命中时给出插入点附近的词。/ Misses suggest the neighbourhood of the insertion point.
        base = work / 'tiny'
        records = write_dictionary(base, [b'apple', b'apply', b'banana', b'band', b'bandage', b'cat'], False)
        ifo = Path(str(base) + '.ifo')
        lookup(lib, ifo, b'zzz')
        text, _, suggestions, indexed = lookup(lib, ifo, b'banx')
        assert text is None and indexed and suggestions == [b'bandage', b'cat'], suggestions
        text, _, suggestions, _ = lookup(lib, ifo, b'aardvark')
        assert text is None and suggestions == [b'apple', b'apply', b'banana', b'band', b'bandage'], suggestions
        # 文件改变后旧索引作废。/ A changed file invalidates the old index.
        records = write_dictionary(base, [b'apple', b'cherry', b'date', b'elder'], False)
        text, _, _, _ = lookup(lib, ifo, b'cherry')
        assert text == oracle(records, (), b'cherry')
        # 未排序的索引永远走顺序扫描，结果仍正确。/ An unsorted index always scans sequentially and stays correct.
        base = work / 'unsorted'
        shuffled = [w for w, _ in write_dictionary(base, words[:5000], False)]
        rng.shuffle(shuffled)
        records = write_dictionary(base, shuffled, False, sort=False)
        ifo = Path(str(base) + '.ifo')
        for q in [rng.choice(shuffled) for _ in range(30)] + [b'none']:
            for _ in range(2):
                text, _, _, indexed = lookup(lib, ifo, q)
                assert text == oracle(records, (), q) and not indexed, q
        # 超过 4 MiB 的 gzip 索引不建内存副本，仍可查。/ A gzip index over 4 MiB is never copied and still works.
        base = work / 'huge'
        long_words = [b'w' + str(i).zfill(7).encode() + b'-' + b'x' * 20 for i in range(160000)]
        records = write_dictionary(base, long_words, True)
        ifo = Path(str(base) + '.ifo')
        for q in [long_words[-1], long_words[0], b'nope']:
            for _ in range(2):
                text, _, _, indexed = lookup(lib, ifo, q)
                assert text == oracle(records, (), q) and not indexed, q
        # 两项 LRU：第三个词典挤出最旧的。/ Two-entry LRU: a third dictionary evicts the oldest.
        assert lib.book_dictionary_cache_bytes() <= 2 * (256 * 1024 + 4 * 1024 * 1024)
        lib.book_dictionary_cache_clear()
        assert lib.book_dictionary_cache_bytes() == 0
        # 中途取消与清理缓存，在 ASan 下不泄漏、不悬空。/ Cancellation and clearing under ASan neither leak nor dangle.
        (work / 'cancel.c').write_text(r'''
#include "book_dictionary.h"
#include <assert.h>
#include <string.h>
int main(int argc,char **argv){
 assert(argc==3);
 for(int round=0;round<3;round++){
  book_dictionary_t d={0};
  assert(book_dictionary_open(&d,argv[1],argv[2]));
  for(int i=0;i<round*40;i++){int s=book_dictionary_step(&d);if(s==4||s<0)break;}
  if(round==1)book_dictionary_cache_clear(); // 查询进行中清理 / clear while a lookup runs
  for(int i=0;i<2000;i++){int s=book_dictionary_step(&d);if(s==4||s<0)break;}
  book_dictionary_close(&d);
 }
 book_dictionary_t d={0};assert(book_dictionary_open(&d,argv[1],"zzzz"));book_dictionary_close(&d);
 book_dictionary_cache_clear();assert(!book_dictionary_cache_bytes());return 0;}
''')
        exe = work / 'cancel'
        subprocess.run(['cc', '-std=gnu11', '-g', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                        '-fno-sanitize-recover=all', '-I' + str(BOOK), str(work / 'cancel.c'),
                        str(BOOK / 'book_dictionary.c'), str(BOOK / 'book_dict_file.c'), '-lz', '-o', str(exe)], check=True)
        for name in ('plain', 'packed'):
            subprocess.run([str(exe), str(work / (name + '.ifo')), words[len(words) // 2].decode()], check=True)
    print('PASS: sparse StarDict index matches the sequential oracle for plain/gzip 200k words, case variants, '
          'Vietnamese, synonyms and bounds; indexed repeats need <=4 calls; neighbour suggestions; changed, '
          'unsorted and >4 MiB gzip indexes fall back; LRU bound; cancellation/clear are ASan-clean')


if __name__ == '__main__':
    main()
