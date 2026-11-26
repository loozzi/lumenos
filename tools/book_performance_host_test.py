#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise real EPUB I/O batching, CRC, clean progress commits and RAM cover dimensions."""
from pathlib import Path
import subprocess
import tempfile
import zipfile
from home_cover_cache_host_test import function

ROOT = Path(__file__).resolve().parents[1]
FLAGS = ['cc', '-std=gnu11', '-D_DEFAULT_SOURCE', '-Wall', '-Wextra', '-Werror',
         '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']


def run(folder, name, source, args=(), extra=()):
    c = folder / (name + '.c')
    exe = folder / name
    c.write_text(source)
    subprocess.run(FLAGS + list(extra) + [str(c), '-lz', '-o', str(exe)], check=True)
    subprocess.run([str(exe)] + list(args), check=True)


with tempfile.TemporaryDirectory(prefix='book-performance-') as d:
    folder = Path(d)
    archive = folder / 'large.epub'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_STORED) as z:
        for i in range(4096):
            z.writestr(f'OEBPS/{i:04d}-' + 'chapter' * 10 + '.html', b'123456789')
    source = (ROOT / 'main/book/zip_reader.c').read_text()
    source = source.replace('#include "metalio_memory.h"', '')
    prelude = r'''
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <zlib.h>
#define DPREADER_BOARD_METALIO47 1
#define DPREADER_BOARD_ZIP_PREFETCH_KB 512
static bool metalio_cache_can_grow(size_t n){(void)n;return true;}
static unsigned seeks;static bool baseline;
static int counted_seek(FILE *f,long p,int whence){++seeks;return fseek(f,p,whence);}
#define fseek counted_seek
'''
    source = source.replace("z->directory_buffer = tail;", "z->directory_buffer = baseline ? NULL : tail;")
    test = r'''
int main(int argc,char **argv){
 assert(argc==2||argc==3);baseline=argc==3;zip_reader_t *z=NULL;assert(zip_open(argv[1],&z)==ESP_OK);
 assert(zip_entry_count(z)==4096);assert(!z->directory_buffer);
 printf("ZIP open (%s): %u seeks for 4096 entries\n",baseline?"unbuffered":"buffered",seeks);assert(baseline?seeks>8192:seeks<40);
 const char *name=zip_entry_name(z,4095);int index=zip_find(z,name);assert(index==4095);
 uint8_t out[9];assert(zip_extract(z,index,out,sizeof(out))==ESP_OK);
 assert(!memcmp(out,"123456789",9));zip_close(z);
 uint8_t data[4096];for(size_t i=0;i<sizeof(data);i++)data[i]=(uint8_t)(i*73+i/7);
 for(size_t n=0;n<=sizeof(data);++n)assert(zip_crc32(data,n)==crc32(0,data,n));
 assert(zip_crc32((const uint8_t*)"123456789",9)==0xcbf43926);
 puts("PASS: bounded directory batching, extraction and CRC differential lengths 0..4096");
}
'''
    run(folder, 'zip', prelude + source + test, [str(archive)],
        ['-I' + str(ROOT / 'tools/zip_host_stubs'), '-I' + str(ROOT / 'main/book')])
    subprocess.run([str(folder / 'zip'), str(archive), 'baseline'], check=True)

    reader = ROOT / 'main/apps/app_book.c'
    unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ESP_OK 0
typedef struct {uint32_t file_size;uint16_t chapter;uint32_t byte_off;uint8_t px,pct;uint32_t last_open_s;} book_progress_t;
typedef struct pending_progress {char path[32];book_progress_t value;bool dirty,progress_saved;struct pending_progress *next;}pending_progress_t;
static pending_progress_t record={.path="/sdcard/book.epub"},other;
static pending_progress_t *s_pending=&record;
static const char *s_path="/sdcard/book.epub",*s_latest_path="/sdcard/book.epub";
static char *s_text="text";
static unsigned s_file_size=100,s_chapter,s_page,s_px=24,s_unsaved;
static bool s_save_failed,fail_progress,fail_last;
static unsigned writes,last_writes,invalidations;
static unsigned book_layout_page_count(void){return 10;}
static unsigned reader_page_offset(unsigned page){return page*100;}
static unsigned percent(unsigned page){return page*10;}
static void kosync_state_touch(const char*p,unsigned size,uint16_t chapter,uint32_t off){(void)p;(void)size;(void)chapter;(void)off;}
static pending_progress_t *pending_find(const char*p){assert(!strcmp(p,s_path));return &record;}
static int book_progress_save(const char*p,book_progress_t*v){assert(!strcmp(p,s_path));++writes;(void)v;return fail_progress?-1:0;}
static int book_progress_set_last_path(const char*p){assert(!strcmp(p,s_path));++last_writes;return fail_last?-1:0;}
static void invalidate_prep(void){++invalidations;}
'''
    unit += function('pending_flush', reader) + '\n' + function('save_progress', reader)
    unit += r'''
int main(void){
 save_progress();assert(writes==1&&last_writes==1&&!record.dirty&&!s_save_failed);
 for(int i=0;i<100;i++){save_progress();}assert(writes==1&&last_writes==1);
 s_page=1;save_progress();assert(writes==2&&last_writes==2&&record.value.byte_off==100);
 s_page=2;fail_progress=true;save_progress();assert(s_save_failed&&record.dirty&&s_unsaved);
 fail_progress=false;save_progress();assert(writes==4&&!s_save_failed&&!s_unsaved);
 s_page=3;fail_last=true;save_progress();assert(writes==5&&record.progress_saved&&record.dirty&&s_save_failed);
 fail_last=false;save_progress();assert(writes==5&&last_writes==5&&!s_save_failed&&!record.dirty);
 record.next=&other;other.dirty=true;save_progress();assert(s_save_failed&&writes==5);
 other.dirty=false;save_progress();assert(!s_save_failed&&writes==5);
 record.progress_saved=false;save_progress();assert(writes==6); // A new open must still persist.
 puts("PASS: 100 clean saves do no IO; changed positions/new opens persist; progress/last-path failures retry");
}
'''
    run(folder, 'progress', unit)

    cover = ROOT / 'main/book/book_cover.c'
    unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define DPREADER_BOARD_METALIO47 1
#define BOOK_COVER_W 176
#define BOOK_COVER_H 240
typedef struct {uint32_t magic,schema,template_version,width,height;uint64_t file_size;int64_t modified;uint64_t path_hash,text_hash;}cover_cache_header_t;
typedef struct {unsigned width,height;}metalio_cover_meta_t;
typedef int metalio_cover_key_t;
static bool hit=true,valid=true;static unsigned opens;static unsigned width=120,height=240;
static bool book_title_from_path(const char*p,char*out,size_t cap){(void)p;(void)out;(void)cap;return false;}
static bool cover_cache_key(const char*p,const char*t,const char*a,char*out,size_t n,cover_cache_header_t*k){(void)p;(void)t;(void)a;snprintf(out,n,"/nonexistent/cache.bin");memset(k,0,sizeof(*k));return valid;}
static metalio_cover_key_t ram_key(const cover_cache_header_t*k,unsigned v){(void)k;assert(v==1);return 1;}
static bool metalio_cover_cache_get(const metalio_cover_key_t*k,void*out,size_t cap,metalio_cover_meta_t*m){assert(*k==1&&!out&&!cap);*m=(metalio_cover_meta_t){width,height};return hit;}
static FILE *counted_open(const char*p,const char*mode){(void)p;(void)mode;++opens;return NULL;}
#define fopen counted_open
'''
    unit += function('book_cover_list_cache_dimensions', cover)
    unit += r'''
int main(void){unsigned w=0,h=0;
 assert(book_cover_list_cache_dimensions("book","title","author",&w,&h)&&w==120&&h==240&&!opens);
 hit=false;assert(!book_cover_list_cache_dimensions("book","title","author",&w,&h)&&opens==1);
 hit=true;width=177;assert(!book_cover_list_cache_dimensions("book","title","author",&w,&h)&&opens==2);
 valid=false;width=120;assert(!book_cover_list_cache_dimensions("book","title","author",&w,&h)&&opens==2);
 puts("PASS: valid RAM dimensions skip disk; misses/invalid dimensions fall back; changed source rejected");
}
'''
    run(folder, 'cover', unit)
