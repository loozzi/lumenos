#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""中文：越南语章名识别，以及正文章名并入目录（层级、排序、去重）。
English: Vietnamese heading recognition and merging body headings into the TOC (levels, order, dedupe)."""
from pathlib import Path
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include "book_epub.c"
#include <assert.h>
int ttf_font_open_mem(uint8_t *data, size_t len, const char *label) { (void)len; (void)label; free(data); return -1; }
void ttf_font_close_embedded(void) {}
int main(int argc, char **argv) {
    if (argc > 2 && !strcmp(argv[1], "title")) {
        for (int i = 2; i < argc; ++i) {
            uint8_t kind = 9;
            bool heading = argv[i][0] == 'H';
            bool ok = vi_chapter_title(argv[i] + 2, heading, &kind);
            printf("%d %d\n", ok, ok ? kind : -1);
        }
        return 0;
    }
    book_epub_t *book = NULL;
    assert(book_epub_open(argv[1], &book) == ESP_OK && book);
    size_t n = book_epub_navigation_count(book);
    for (size_t i = 0; i < n; ++i) {
        char title[256];
        assert(book_epub_navigation_title(book, i, title, sizeof(title)) == ESP_OK);
        printf("%u|%u|%u|%s\n", (unsigned)book_epub_navigation_level(book, i),
               (unsigned)book_epub_navigation_chapter(book, i),
               book_epub_navigation_source_offset(book, i) != SIZE_MAX, title);
    }
    book_epub_close(book);
    return 0;
}
'''


def epub(path, chapters, ncx=None, nav=None):
    items = ''.join(f'<item id="c{i}" href="t/c{i}.xhtml" media-type="application/xhtml+xml"/>'
                    for i in range(len(chapters)))
    spine = ''.join(f'<itemref idref="c{i}"/>' for i in range(len(chapters)))
    extra, toc = '', ''
    if ncx is not None:
        extra += '<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>'
        toc = ' toc="ncx"'
    if nav is not None:
        extra += '<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>'
    with zipfile.ZipFile(path, 'w') as z:
        z.writestr('mimetype', 'application/epub+zip')
        z.writestr('META-INF/container.xml', '<container><rootfiles><rootfile full-path="O/b.opf" '
                   'media-type="application/oebps-package+xml"/></rootfiles></container>')
        z.writestr('O/b.opf', f'<package><manifest>{items}{extra}</manifest><spine{toc}>{spine}</spine></package>')
        if ncx is not None:
            z.writestr('O/toc.ncx', f'<ncx><navMap>{ncx}</navMap></ncx>')
        if nav is not None:
            z.writestr('O/nav.xhtml', f'<html><body><nav epub:type="toc">{nav}</nav></body></html>')
        for i, body in enumerate(chapters):
            z.writestr(f'O/t/c{i}.xhtml', f'<html><body>{body}</body></html>')


def chapters_html(titles):
    return ''.join(f'<h2>{t}</h2><p>Nội dung của {t} kéo dài vài dòng.</p>' for t in titles)


def point(label, src, children=''):
    return f'<navPoint><navLabel><text>{label}</text></navLabel><content src="{src}"/>{children}</navPoint>'


def main():
    with tempfile.TemporaryDirectory(prefix='toc-vi-') as temp:
        work = Path(temp)
        (work / 'h.c').write_text(HARNESS)
        exe = work / 'toc'
        subprocess.run(['cc', '-std=gnu11', '-D_DEFAULT_SOURCE', '-Wall', '-Wextra', '-Werror', '-g',
                        '-Wno-unused-function', '-fsanitize=address,undefined',
                        '-I' + str(ROOT / 'tools/book_epub_stubs'), '-I' + str(ROOT / 'tools/zip_host_stubs'),
                        '-I' + str(ROOT / 'main/book'), str(work / 'h.c'),
                        str(ROOT / 'main/book/zip_reader.c'), str(ROOT / 'main/book/html_text.c'),
                        str(ROOT / 'main/book/book_index_cache.c'), str(ROOT / 'main/book/book_image_header.c'),
                        '-lz', '-o', str(exe)], check=True)

        def run(*args):
            return subprocess.run([str(exe), *map(str, args)], check=True, capture_output=True,
                                  text=True).stdout.splitlines()

        # Nhận dạng tiêu đề: H = thẻ tiêu đề, P = đoạn thường.
        accept = {
            'H Chương 1': 1, 'H Chương 12: Người lạ': 1, 'H CHƯƠNG 3': 1, 'H Chuong 4 - Mua dong': 1,
            'H Chương mười hai': 1, 'H Chương Một': 1, 'H Hồi 5': 1, 'H Chapter 7': 1, 'H Chương IV': 1,
            'H Phần 1': 0, 'H Phần II': 0, 'H Phần thứ nhất': 0, 'H Quyển 3: Mùa thu': 0, 'H Tập 2': 0,
            'H Part 1': 0, 'H Book 2': 0, 'P Chương 9': 1, 'P Chương 9: Trở về': 1, 'P Phần hai': 0,
            'H Chương 1.': 1, 'H Chương 1 – Khởi đầu': 1, 'H chương 2': 1,
        }
        reject = ['H Chương này kể về chuyện cũ', 'H Phần lớn', 'H Phần mềm', 'H Hồi ký', 'H Chương',
                  'H Tapestry 3', 'H Party 5', 'H Part Ice', 'H Chương1a', 'H Bản đồ',
                  'P Chương 3 kể lại câu chuyện của một người đàn ông đi qua nhiều vùng đất xa xôi.',
                  'H Phan Bội Châu', 'H Chương mở đầu']
        results = run('title', *accept, *reject)
        for (text, kind), line in zip(accept.items(), results):
            assert line == f'1 {kind}', (text, line)
        for text, line in zip(reject, results[len(accept):]):
            assert line == '0 -1', (text, line)

        # (a) Không có mục lục, 3 tệp × 5 chương → 15 mục cấp 0, đúng thứ tự.
        files = [chapters_html([f'Chương {f * 5 + i + 1}' for i in range(5)]) for f in range(3)]
        epub(work / 'a.epub', files)
        rows = run(work / 'a.epub')
        assert [r.split('|')[3] for r in rows] == [f'Chương {i}' for i in range(1, 16)], rows
        assert all(r.startswith('0|') for r in rows), rows
        assert [int(r.split('|')[1]) for r in rows] == [i // 5 for i in range(15)], rows
        assert all(r.split('|')[2] == '1' for r in rows), rows

        # (b) NCX chỉ có 3 Phần → 3 mục cấp 0 + 15 chương cấp 1, nằm dưới đúng Phần.
        ncx = ''.join(point(f'Phần {f + 1}', f't/c{f}.xhtml') for f in range(3))
        epub(work / 'b.epub', files, ncx=ncx)
        rows = run(work / 'b.epub')
        expect = []
        for f in range(3):
            expect.append(f'0|{f}|Phần {f + 1}')
            expect += [f'1|{f}|Chương {f * 5 + i + 1}' for i in range(5)]
        assert ['|'.join(r.split('|')[i] for i in (0, 1, 3)) for r in rows] == expect, rows

        # (b2) NCX không theo thứ tự đọc → sắp theo spine.
        ncx = ''.join(point(f'Phần {f + 1}', f't/c{f}.xhtml') for f in (2, 0, 1))
        epub(work / 'b2.epub', files, ncx=ncx)
        rows = run(work / 'b2.epub')
        assert ['|'.join(r.split('|')[i] for i in (0, 1, 3)) for r in rows] == expect, rows

        # (c) NCX lồng 2 cấp, đủ chi tiết → giữ nguyên, cấp đúng, không thêm mục.
        ncx = ''.join(point(f'Phần {f + 1}', f't/c{f}.xhtml',
                            ''.join(point(f'Chương {f * 5 + i + 1}', f't/c{f}.xhtml') for i in range(5)))
                      for f in range(3))
        epub(work / 'c.epub', files, ncx=ncx)
        rows = run(work / 'c.epub')
        assert ['|'.join(r.split('|')[i] for i in (0, 1, 3)) for r in rows] == expect, rows

        # (c2) NAV lồng <ol> 2 cấp.
        nav = '<ol>' + ''.join(
            f'<li><a href="t/c{f}.xhtml">Phần {f + 1}</a><ol>' +
            ''.join(f'<li><a href="t/c{f}.xhtml">Chương {f * 5 + i + 1}</a></li>' for i in range(5)) +
            '</ol></li>' for f in range(3)) + '</ol>'
        epub(work / 'c2.epub', files, nav=nav)
        rows = run(work / 'c2.epub')
        assert ['|'.join(r.split('|')[i] for i in (0, 1, 3)) for r in rows] == expect, rows

        # (d) Mục lục phẳng đủ, trùng tên thân sách → không nhân đôi.
        ncx = ''.join(point(f'Chương {f * 5 + i + 1}', f't/c{f}.xhtml') for f in range(3) for i in range(5))
        epub(work / 'd.epub', files, ncx=ncx)
        rows = run(work / 'd.epub')
        assert [r.split('|')[3] for r in rows] == [f'Chương {i}' for i in range(1, 16)], rows

        # (e) Sách tiếng Trung cũ: kết quả như trước (một mục mỗi tệp, cấp 0).
        zh = [f'<h1>第{i + 1}章 记录</h1><p>正文 {i}</p>' for i in range(4)]
        epub(work / 'e.epub', zh)
        rows = run(work / 'e.epub')
        assert [r.split('|')[3] for r in rows] == [f'第{i + 1}章 记录' for i in range(4)], rows
        assert all(r.startswith('0|') for r in rows), rows

        # (f) Không mục lục, tệp đầu không có tiêu đề chương vẫn giữ một dòng.
        epub(work / 'f.epub', ['<p>Lời mở đầu không có tiêu đề nhưng có chữ.</p>'] + files[:1])
        rows = run(work / 'f.epub')
        assert len(rows) == 6 and rows[0].split('|')[1] == '0', rows
        assert [r.split('|')[3] for r in rows[1:]] == [f'Chương {i}' for i in range(1, 6)], rows
    print('book_epub_toc_vi_host_test: PASS')


if __name__ == '__main__':
    main()
