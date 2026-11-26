#!/usr/bin/env python3
"""Whole-book notes list and Markdown export with the real snapshot store.
SPDX-License-Identifier: Apache-2.0

中文：用真实 book_annotations_save 写多章快照（含旧内容版本、损坏章、当前章），检查整本书列表的
章内排序、当前版本优先、坏章计数、512 条上限与分步读取；Markdown 导出检查标题、章名、引用、
行首转义、越南语 UTF-8，以及写入失败时保留旧文件。
English: write multi-chapter snapshots with the real store (old revisions, a corrupt chapter, the current
chapter) and check ordering, current-revision preference, the bad-chapter count, the 512 cap and bounded
steps; the Markdown export checks title, chapter names, quotes, leading escapes, Vietnamese UTF-8 and that
a failed write keeps the old file.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BOOK = ROOT / 'main/book'
UNIT = r'''
#include "book_notes_book.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
static book_annotations_t a;
static void chapter(const char *root,uint32_t ch,const char *text,const char **notes,unsigned n,time_t when){
 assert(book_annotations_load(&a,root,"",ch,text,strlen(text)));
 for(unsigned i=0;i<n;i++){const char *hit=strstr(text,notes[2*i]);assert(hit);
  size_t at=(size_t)(hit-text);assert(book_annotations_put(&a,text,at,at+strlen(notes[2*i]),notes[2*i+1]));}
 assert(book_annotations_save(&a,root));
 // 固定修改时间，模拟较旧/较新的版本。/ Pin modification times to model older/newer revisions.
 char file[700];snprintf(file,sizeof(file),"%s/%016llx-%u-%016llx.%u.ann",root,(unsigned long long)a.path_hash,ch,(unsigned long long)a.text_hash,(unsigned)(a.sequence&1));
 struct utimbuf t={when,when};assert(!utime(file,&t));
}
static int titles(size_t ch,char *buf,size_t cap){if(ch==2)return -1;snprintf(buf,cap,"Phần %u",(unsigned)ch);return 0;}
static int run(book_notes_book_t *b){int steps=0,s;while((s=book_notes_book_step(b))==BOOK_NOTES_BOOK_LISTING||s==BOOK_NOTES_BOOK_LOADING)assert(++steps<10000);return steps;}
static char *slurp(const char *path){FILE *f=fopen(path,"rb");assert(f);static char buf[200000];size_t n=fread(buf,1,sizeof(buf)-1,f);buf[n]=0;fclose(f);return buf;}
int main(void){
 char dir[]="/tmp/notes-book-XXXXXX";assert(mkdtemp(dir));char root[600];snprintf(root,sizeof(root),"%s/snapshots",dir);
 const char *t0="Một hai ba bốn năm sáu bảy tám chín mười.",*t1="# Heading-like text\n> quoted\n1. listed item here",*t1old="Old revision text only";
 const char *n0[]={"bảy tám","ghi chú tiếng Việt","Một hai","đầu"};
 const char *n1[]={"# Heading-like","escape me","> quoted","","1. listed","- dash note"};
 const char *n1old[]={"Old revision","should be hidden"};
 chapter(root,3,t1old,n1old,1,1000);
 chapter(root,3,t1,n1,3,2000);
 chapter(root,0,t0,n0,2,1500);
 const char *t5="Current text of chapter five",*t5new="Newer text that is not live",*n5[]={"Current","live"},*n5new[]={"Newer","not live"};
 chapter(root,5,t5,n5,1,1000);
 chapter(root,5,t5new,n5new,1,9000);
 // 损坏的一章：两个槽都坏。/ A corrupt chapter: both slots broken.
 char bad[700];snprintf(bad,sizeof(bad),"%s/%016llx-%u-%016llx.0.ann",root,0x1111ull,7u,0x2222ull);
 FILE *f=fopen(bad,"wb");assert(f);fputs("KANN garbage",f);fclose(f);
 snprintf(bad,sizeof(bad),"%s/not-a-snapshot.txt",root);f=fopen(bad,"wb");assert(f);fclose(f);
 book_notes_book_t b={0};
 uint64_t live5=book_annotations_text_hash(t5,strlen(t5));
 assert(book_notes_book_begin(&b,root,true,5,live5));run(&b);
 assert(b.state==BOOK_NOTES_BOOK_DONE&&b.errors==1&&!b.truncated);
 assert(b.count==2+3+1);
 assert(b.items[0].chapter==0&&!strcmp(b.items[0].quote,"Một hai")&&!strcmp(b.items[1].quote,"bảy tám"));
 assert(b.items[2].chapter==3&&!strcmp(b.items[2].quote,"# Heading-like")&&!strcmp(b.items[4].quote,"1. listed"));
 assert(b.items[5].chapter==5&&!strcmp(b.items[5].note,"live")); // 当前章用现行版本 / live revision wins
 for(unsigned i=0;i<b.count;i++)assert(strcmp(b.items[i].note,"should be hidden"));
 // 非当前章：取最近修改的版本。/ Not the current chapter: the latest modified revision.
 assert(book_notes_book_begin(&b,root,false,0,0));run(&b);
 assert(b.items[5].chapter==5&&!strcmp(b.items[5].note,"not live"));
 char md[700];snprintf(md,sizeof(md),"%s/ghi-chu.md",dir);
 assert(book_notes_book_export(&b,md,"Sách #1","25/11/2026",titles));
 char *text=slurp(md);
 assert(!strncmp(text,"# Sách #1\n\nXuất ngày 25/11/2026 · 6 ghi chú\n\n## Phần 0\n\n> Một hai\n\nđầu\n\n> bảy tám\n\nghi chú tiếng Việt\n\n## Phần 3\n\n",strlen("# Sách #1\n\nXuất ngày 25/11/2026 · 6 ghi chú\n\n## Phần 0\n\n> Một hai\n\nđầu\n\n> bảy tám\n\nghi chú tiếng Việt\n\n## Phần 3\n\n")));
 assert(strstr(text,"> \\# Heading-like\n\nescape me\n\n"));
 assert(strstr(text,"> \\> quoted\n\n> 1\\. listed\n\n\\- dash note\n\n"));
 assert(strstr(text,"## Phần 5\n\n> Newer\n\nnot live\n"));
 assert(!strstr(text,"Chương 3")&&!strstr(text,"should be hidden"));
 // 写入失败：旧文件不变。/ A failed write keeps the old file.
 char blocked[720];snprintf(blocked,sizeof(blocked),"%s.tmp",md);assert(!mkdir(blocked,0700));
 assert(!book_notes_book_export(&b,md,"Khác",NULL,NULL));
 assert(!strncmp(slurp(md),"# Sách #1",9));assert(!rmdir(blocked));
 // 未完成的列表不能导出。/ An unfinished list cannot be exported.
 book_notes_book_t pending={0};assert(book_notes_book_begin(&pending,root,false,0,0));
 assert(!book_notes_book_export(&pending,md,"x",NULL,NULL));book_notes_book_end(&pending);
 // 没有快照目录：空列表；没有标题回调时写“Chương N”。/ No snapshot folder: empty list; no title callback writes "Chương N".
 char none[640];snprintf(none,sizeof(none),"%s/none",dir);assert(book_notes_book_begin(&pending,none,false,0,0));
 assert(pending.state==BOOK_NOTES_BOOK_DONE&&!pending.count);book_notes_book_end(&pending);
 assert(book_notes_book_begin(&b,root,false,0,0));run(&b);assert(book_notes_book_export(&b,md,NULL,NULL,NULL));
 text=slurp(md);assert(!strncmp(text,"# Ghi chú\n\n6 ghi chú\n\n## Chương 1\n",33));
 // 上限：20 章 × 32 条 = 640，截在 512。/ Cap: 20 chapters × 32 notes = 640, cut at 512.
 char many[640];snprintf(many,sizeof(many),"%s/many",dir);
 char body[32*8+1];for(int i=0;i<32;i++)memcpy(body+i*8,"word00, ",8);body[32*8]=0;
 for(int i=0;i<32;i++){body[i*8+4]=(char)('0'+i/10);body[i*8+5]=(char)('0'+i%10);}
 for(uint32_t ch=0;ch<20;ch++){
  assert(book_annotations_load(&a,many,"",ch,body,strlen(body)));
  for(int i=0;i<32;i++)assert(book_annotations_put(&a,body,(size_t)i*8,(size_t)i*8+6,"n"));
  assert(book_annotations_save(&a,many));
 }
 assert(book_notes_book_begin(&b,many,false,0,0));int steps=run(&b);
 assert(b.count==BOOK_NOTES_BOOK_MAX&&b.truncated&&steps>=5);
 for(unsigned i=1;i<b.count;i++)assert(b.items[i-1].chapter<b.items[i].chapter||(b.items[i-1].chapter==b.items[i].chapter&&b.items[i-1].start<b.items[i].start));
 assert(book_notes_book_export(&b,md,"Nhiều",NULL,NULL));assert(strstr(slurp(md),"512 ghi chú"));
 book_notes_book_end(&b);book_notes_book_end(&b); // 可重复释放 / Safe to end twice
 char cmd[700];snprintf(cmd,sizeof(cmd),"rm -rf '%s'",dir);assert(!system(cmd));
 puts("whole-book notes: PASS");return 0;
}
'''

if __name__ == '__main__':
    with tempfile.TemporaryDirectory() as d:
        p = Path(d)
        (p / 'test.c').write_text(UNIT)
        subprocess.run(['cc', '-std=gnu11', '-g', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                        '-fno-sanitize-recover=all', '-I' + str(BOOK), str(p / 'test.c'),
                        str(BOOK / 'book_notes_book.c'), str(BOOK / 'book_annotations.c'), '-o', str(p / 'test')],
                       check=True)
        subprocess.run([str(p / 'test')], check=True)
    print('PASS whole-book notes: chapter/offset order, live revision for the current chapter and newest revision '
          'otherwise, corrupt chapters counted, 512 cap, bounded steps; Markdown title/date/chapters/quotes/escapes, '
          'Vietnamese UTF-8, failed writes keep the old file')
