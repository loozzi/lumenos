#!/usr/bin/env python3
"""Vocabulary packs, Leitner scheduling, progress slots and the word-of-the-day summary.
SPDX-License-Identifier: Apache-2.0

中文：用真实 vocab_pack.c / vocab_study.c / vocab_today.c：自写 CSV（与 TOEIC 600 数据集同列名，含引号、CRLF、BOM、
多余/缺失列、过长字段、坏 UTF-8、重复、空行、4001 行）；Leitner 升降盒、到期、每日新词上限、忘记重复上限、
未同步时钟的学习日与切换；进度两槽、校验和、坏槽回退、增删行不偏移；今日单词摘要只读一次。
English: exercise the real vocab_pack.c / vocab_study.c / vocab_today.c with self-written CSV in the TOEIC 600
column layout (quotes, CRLF, BOM, extra/missing columns, long fields, bad UTF-8, duplicates, blank rows, 4001
rows); Leitner boxes, due days, the daily new-word cap, the forget-repeat cap, study days without a synced
clock and switching clocks; two progress slots, checksums, bad-slot fallback, stable keys across row edits;
the word-of-the-day summary loads once.
No vocabulary data from third-party datasets is stored in the repository.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BOOK = ROOT / 'main/book'
UNIT = r'''
#include "vocab_pack.h"
#include "vocab_study.h"
#include "vocab_today.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void put(const char *path,const char *text){FILE *f=fopen(path,"wb");assert(f);fputs(text,f);fclose(f);}
static const char *T(vocab_pack_t *p,unsigned i,vocab_field_t f){return vocab_text(p,i,f);}
int main(int argc,char **argv){
 assert(argc==2);assert(!chdir(argv[1]));
 vocab_pack_t p={0};
 // 1. 与 TOEIC 600 同列名，外加未知列、引号、逗号、换行、CRLF、BOM。/ Same columns as TOEIC 600 plus unknown columns, quotes, commas, newlines, CRLF, BOM.
 put("a.csv","\xef\xbb\xbf" "english,type,vietnamese,pronounce,explain,example,example_vietnamese,image_url,topic\r\n"
  "invoice,n.,\"hóa đơn, phiếu tính tiền\",/ˈɪnvɔɪs/,a bill,\"Please send the \"\"final\"\" invoice.\",\"Vui lòng gửi hóa đơn\ncuối.\",http://x,Money\r\n"
  "\r\n"
  "Deadline,n.,hạn chót,,,,,,\r\n"
  "deadline,n.,trùng,,,,,,\r\n"
  ",n.,thiếu từ,,,,,,\r\n"
  "budget,n.,,,,,,,\r\n"
  "bad,adj.,\xff\xfe,,,,,,\r\n"
  "agenda,n.,chương trình nghị sự");
 assert(vocab_pack_load(&p,"a.csv"));
 assert(p.count==3&&p.skipped==4&&!p.truncated&&!p.limited);
 assert(!strcmp(T(&p,0,VOCAB_ENGLISH),"invoice")&&!strcmp(T(&p,0,VOCAB_VIETNAMESE),"hóa đơn, phiếu tính tiền"));
 assert(!strcmp(T(&p,0,VOCAB_EXAMPLE),"Please send the \"final\" invoice.")&&!strcmp(T(&p,0,VOCAB_EXAMPLE_VI),"Vui lòng gửi hóa đơn cuối."));
 assert(!strcmp(T(&p,0,VOCAB_PRONOUNCE),"/ˈɪnvɔɪs/")&&!strcmp(T(&p,1,VOCAB_ENGLISH),"Deadline")&&!strcmp(T(&p,2,VOCAB_TYPE),"n."));
 assert(vocab_key("Deadline")==vocab_key("deadline")&&vocab_find(&p,vocab_key("AGENDA"))==2&&vocab_find(&p,vocab_key("none"))<0);
 assert(!*T(&p,2,VOCAB_EXPLAIN)&&!*T(&p,99,VOCAB_ENGLISH));
 vocab_pack_free(&p);
 // 2. rank 列决定顺序；只有必填列；字段过长按 UTF-8 截断。/ A rank column orders words; required columns only; long fields cut at UTF-8.
 char big[8192];int n=snprintf(big,sizeof(big),"Vietnamese,ENGLISH,rank\n");
 n+=snprintf(big+n,sizeof(big)-n,"\"");for(int i=0;i<80;i++)n+=snprintf(big+n,sizeof(big)-n,"ước");n+=snprintf(big+n,sizeof(big)-n,"\",zeta,3\n");
 n+=snprintf(big+n,sizeof(big)-n,"alpha nghĩa,alpha,1\nbeta nghĩa,beta,2\n");put("b.csv",big);
 assert(vocab_pack_load(&p,"b.csv")&&p.count==3&&p.truncated==1);
 assert(!strcmp(T(&p,0,VOCAB_ENGLISH),"alpha")&&!strcmp(T(&p,2,VOCAB_ENGLISH),"zeta")&&p.words[2].rank==3);
 const char *cut=T(&p,2,VOCAB_VIETNAMESE);size_t cl=strlen(cut);
 assert(cl<=VOCAB_MEANING_CAP&&!strcmp(cut+cl-3,"…")&&!strncmp(cut,"ước",6));
 vocab_pack_free(&p);
 // 3. 错误：缺列、空包、过大、不存在。/ Errors: missing columns, empty pack, oversize, missing file.
 put("c.csv","word,meaning\nx,y\n");assert(!vocab_pack_load(&p,"c.csv")&&strstr(p.error,"english")&&!p.words);
 put("d.csv","english,vietnamese\n,\n");assert(!vocab_pack_load(&p,"d.csv")&&strstr(p.error,"hợp lệ"));
 assert(!vocab_pack_load(&p,"missing.csv")&&strstr(p.error,"mở"));
 FILE *f=fopen("e.csv","wb");assert(f);fputs("english,vietnamese\n",f);for(unsigned i=0;i<VOCAB_FILE_MAX/16;i++)fputs("aaaaaaa,bbbbbbb\n",f);fclose(f);
 assert(!vocab_pack_load(&p,"e.csv")&&strstr(p.error,"2 MiB"));
 // 4. 4001 词只取前 4000。/ 4001 words keep the first 4000.
 f=fopen("f.csv","wb");assert(f);fputs("english,vietnamese\n",f);for(int i=0;i<4001;i++)fprintf(f,"w%04d,n%d\n",i,i);fclose(f);
 assert(vocab_pack_load(&p,"f.csv")&&p.count==4000&&p.limited);vocab_pack_free(&p);
 // 5. Leitner。/ Leitner.
 f=fopen("g.csv","wb");assert(f);fputs("english,vietnamese\n",f);for(int i=0;i<30;i++)fprintf(f,"word%02d,nghĩa %d\n",i,i);fclose(f);
 assert(vocab_pack_load(&p,"g.csv")&&p.count==30);
 vocab_progress_t pr={0};assert(vocab_progress_load(&pr,".lumen","g.csv")&&pr.new_per_day==10&&pr.show_home&&pr.show_lock&&!pr.count);
 const int64_t day0=1800000000;uint32_t today=vocab_today(&pr,day0);assert(today==(uint32_t)((day0+7*3600)/86400));
 vocab_session_t s;vocab_session_begin(&s,&pr,&p,today);assert(s.count==10&&vocab_current(&s)==0);
 assert(vocab_featured(&pr,&p,today)==0);
 // 记得 9 个，忘记第 0 个：第 0 个回到队尾，最多出现 3 次。/ Remember 9, forget word 0: it returns to the end, at most three times.
 assert(vocab_answer(&s,&pr,&p,false,today)==1);
 for(int i=1;i<10;i++)vocab_answer(&s,&pr,&p,true,today);
 assert(vocab_current(&s)==0&&s.count==11);
 assert(vocab_answer(&s,&pr,&p,false,today)==0&&s.count==12);
 assert(vocab_answer(&s,&pr,&p,false,today)==-1&&s.count==12&&pr.cleared);
 assert(s.remembered==9&&s.forgotten==3&&pr.count==10&&pr.new_today==10);
 vocab_state_t *st=vocab_state(&pr,vocab_key("word01"));assert(st->box==2&&st->due==today+2);
 st=vocab_state(&pr,vocab_key("word00"));assert(st->box==1&&st->due==today);
 unsigned learned,due,fresh;vocab_counts(&pr,&p,today,&learned,&due,&fresh);assert(learned==10&&due==1&&fresh==20);
 // 当天再学：只有到期词，新词额度已用完。/ Same day: only due words; the new-word budget is spent.
 vocab_session_begin(&s,&pr,&p,today);assert(s.count==1&&vocab_current(&s)==0);
 assert(vocab_answer(&s,&pr,&p,true,today)==-1);st=vocab_state(&pr,vocab_key("word00"));assert(st->box==2&&st->due==today+2);
 // 两天后：到期词在前，再加 10 个新词。/ Two days later: due words first, then ten new words.
 vocab_session_begin(&s,&pr,&p,today+2);assert(s.count==20&&vocab_current(&s)==0&&s.word[10]==10);
 for(int i=0;i<10;i++)assert(vocab_answer(&s,&pr,&p,true,today+2)>=0);
 st=vocab_state(&pr,vocab_key("word05"));assert(st->box==3&&st->due==today+2+4);
 // 盒 5 封顶。/ Box 5 is the cap.
 // 状态数组会扩容，指针不能跨回答保留。/ The state array grows, so pointers never survive an answer.
 for(int k=0;k<6;k++){uint32_t at=vocab_state(&pr,vocab_key("word05"))->due;vocab_session_t one;vocab_session_begin(&one,&pr,&p,at);
  while(vocab_current(&one)>=0)vocab_answer(&one,&pr,&p,true,at);}
 st=vocab_state(&pr,vocab_key("word05"));
 assert(st->box==5&&vocab_interval(5)==16&&vocab_interval(0)==1);
 // 6. 进度两槽与校验。/ Two progress slots and checksums.
 pr.new_per_day=20;pr.show_lock=false;uint32_t count=pr.count;
 assert(vocab_progress_save(&pr,".lumen","g.csv")&&vocab_progress_save(&pr,".lumen","g.csv")&&pr.sequence==2);
 vocab_progress_t back={0};assert(vocab_progress_load(&back,".lumen","g.csv")&&back.count==count&&back.sequence==2);
 assert(back.new_per_day==20&&back.show_home&&!back.show_lock&&vocab_state(&back,vocab_key("word05"))->box==5);
 // 新槽损坏：回到旧槽。/ The newer slot is corrupt: fall back to the older one.
 f=fopen(".lumen/g.0.prog","r+b");assert(f);fseek(f,40,SEEK_SET);fputc('x',f);fclose(f);
 assert(vocab_progress_load(&back,".lumen","g.csv")&&back.sequence==1);
 f=fopen(".lumen/g.1.prog","r+b");assert(f);fseek(f,8,SEEK_SET);fputc(0x7f,f);fclose(f);
 assert(!vocab_progress_load(&back,".lumen","g.csv")); // 两槽都坏 / both slots bad
 vocab_progress_free(&back);
 // 增删 CSV 行后进度仍跟着单词。/ Progress follows words after rows are added or removed.
 f=fopen("g.csv","wb");assert(f);fputs("english,vietnamese\nfresh,mới\n",f);for(int i=29;i>=0;i--)fprintf(f,"word%02d,nghĩa %d\n",i,i);fclose(f);
 vocab_pack_t q={0};assert(vocab_pack_load(&q,"g.csv")&&q.count==31&&vocab_find(&q,vocab_key("word05"))==25);
 assert(vocab_progress_save(&pr,".lumen","g.csv"));assert(vocab_progress_load(&back,".lumen","g.csv"));
 assert(vocab_state(&back,q.words[25].key)->box==5);
 vocab_progress_free(&back);vocab_pack_free(&q);
 // 7. 未同步时钟：学习日，学完到期词后下一次是新的一天；切到真实日期时旧词视为到期。
 // Unsynced clock: study days; after clearing due words the next session is a new day; real dates treat old words as due.
 vocab_progress_t u={0};assert(vocab_progress_load(&u,".lumen","none.csv"));u.new_per_day=5;
 uint32_t d1=vocab_today(&u,0);assert(d1==(1u<<24));
 vocab_session_begin(&s,&u,&p,d1);assert(s.count==5);while(vocab_current(&s)>=0)vocab_answer(&s,&u,&p,true,d1);
 assert(u.cleared);uint32_t d2=vocab_today(&u,0);assert(d2==d1+1&&!u.cleared&&vocab_today(&u,0)==d2);
 vocab_session_begin(&s,&u,&p,d2);assert(s.count==5); // 新的一天：新词额度恢复，盒 2 未到期 / new day: fresh budget, box 2 not due
 uint32_t real=vocab_today(&u,day0);vocab_counts(&u,&p,real,&learned,&due,&fresh);assert(learned==5&&due==5);
 vocab_progress_free(&u);vocab_progress_free(&pr);
 // 8. 今日单词摘要：只读一次；拔卡后重读；写入后 RAM 副本立即更新。
 // Word of the day: loads once; reloads after invalidation; writing updates the RAM copy at once.
 vocab_today_t t={.valid=true,.show_home=true,.show_lock=false};strcpy(t.english,"invoice");strcpy(t.type,"n.");strcpy(t.vietnamese,"hóa đơn");
 assert(vocab_today_set(".lumen",&t));char line[240];vocab_today_line(vocab_today_cached(),line,sizeof(line));assert(!strcmp(line,"invoice (n.) — hóa đơn"));
 vocab_today_invalidate();assert(!vocab_today_cached()->valid);
 const vocab_today_t *g=vocab_today_get(".lumen");assert(g->valid&&g->show_home&&!g->show_lock&&!strcmp(g->vietnamese,"hóa đơn"));
 unlink(".lumen/today.txt");assert(vocab_today_get(".lumen")->valid); // 已加载，不再读卡 / already loaded, no card read
 vocab_today_invalidate();assert(!vocab_today_get(".lumen")->valid);
 t.type[0]=0;assert(vocab_today_set(".lumen",&t));vocab_today_line(vocab_today_cached(),line,sizeof(line));assert(!strcmp(line,"invoice — hóa đơn"));
 vocab_pack_free(&p);
 puts("vocabulary: PASS");return 0;
}
'''

MOUNT = r"""
#include "vocab_study.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#undef mkdir
int mkdir(const char *path, mode_t mode);
int esp_mkdir(const char *path, mode_t mode) {
    if (!strcmp(path, "/sdcard") || !strcmp(path, "sdcard")) { errno = EINVAL; return -1; }
    return mkdir(path, mode);
}
int main(int argc, char **argv) {
    assert(argc == 2 && !chdir(argv[1]));
    vocab_progress_t pr = {.new_per_day = 10, .show_home = true, .show_lock = true}, back = {0};
    assert(vocab_progress_save(&pr, "sdcard/vocab/.lumen", "TOEIC 600.csv"));
    assert(vocab_progress_load(&back, "sdcard/vocab/.lumen", "TOEIC 600.csv") && back.sequence == 1);
    vocab_progress_free(&back);
    return 0;
}
"""

if __name__ == '__main__':
    with tempfile.TemporaryDirectory() as d:
        p = Path(d)
        (p / 'test.c').write_text(UNIT)
        (p / 'work').mkdir()
        (p / 'work/.lumen').mkdir()
        subprocess.run(['cc', '-std=gnu11', '-g', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                        '-fno-sanitize-recover=all', '-I' + str(BOOK), str(p / 'test.c'), str(BOOK / 'vocab_pack.c'),
                        str(BOOK / 'vocab_study.c'), str(BOOK / 'vocab_today.c'), '-o', str(p / 'test')], check=True)
        subprocess.run([str(p / 'test'), str(p / 'work')], check=True)
        # ESP-IDF：mkdir 挂载点返回 EINVAL（不是 EEXIST）。曾导致进度永远存不上。
        # ESP-IDF: mkdir on the mount point returns EINVAL (not EEXIST); this once made progress never save.
        (p / 'mount.c').write_text(MOUNT)
        (p / 'sdcard').mkdir()
        subprocess.run(['cc', '-std=gnu11', '-g', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                        '-fno-sanitize-recover=all', '-Dmkdir=esp_mkdir', '-I' + str(BOOK), str(p / 'mount.c'),
                        str(BOOK / 'vocab_study.c'), '-o', str(p / 'mount')], check=True)
        subprocess.run([str(p / 'mount'), str(p)], check=True)
    print('PASS vocabulary: TOEIC-600-style columns, quotes/CRLF/BOM, skipped rows, UTF-8 cuts, rank order, errors, '
          '4000 cap; Leitner boxes, due days, daily new cap, forget repeats; two checksummed progress slots with '
          'fallback and stable keys; study days and clock switching; word-of-the-day summary loads once; saving under a mount point that rejects mkdir')
