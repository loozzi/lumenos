/* SPDX-License-Identifier: Apache-2.0 */
#include "book_dictionary.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "esp_heap_caps.h"
#endif

/* ---- 稀疏索引缓存 / Sparse index cache ---- */
// 每个检查点保存记录位置与键的前 47 字节；更长的键只作“未知”比较。
// Each checkpoint keeps the record position and the first 47 key bytes; longer keys compare as unknown.
#define CP_KEY 47
#define CP_LONG 255
#define CACHE_ENTRIES 2
#define CACHE_INDEX_CAP (256u*1024u)
#define CACHE_MEMORY_CAP (4u*1024u*1024u)
#define SLICE_CAP (64u*1024u)
#define MIN_STRIDE 64u
typedef struct { uint32_t pos; uint8_t len; char key[CP_KEY]; } checkpoint_t;
struct book_dict_cache {
    char base[BOOK_DICT_PATH];
    uint32_t idx_physical, idx_size, words, stride;
    int64_t mtime;
    bool used, compressed, complete, broken;
    unsigned refs;
    uint32_t lru;
    checkpoint_t *cps; uint32_t count, cap;
    uint32_t built, built_pos;   // 已连续建立的记录数与字节位置 / Contiguously indexed records and bytes
    char last[256];              // 上一条完整键，用于检查排序 / Previous full key for the sort check
    unsigned char *memory;       // gzip 索引的解压副本 / Inflated copy of a gzip index
    uint32_t memory_len;
};
static book_dict_cache_t s_cache[CACHE_ENTRIES];
static uint32_t s_cache_clock;
static void *psram_alloc(size_t n) {
#ifdef ESP_PLATFORM
    return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
    return malloc(n);
#endif
}
static void *psram_realloc(void *p,size_t n) {
#ifdef ESP_PLATFORM
    return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
    return realloc(p,n);
#endif
}
static void cache_release_memory(book_dict_cache_t *c) {
    free(c->cps);free(c->memory);c->cps=NULL;c->memory=NULL;c->count=c->cap=c->memory_len=0;
}
static void cache_reset(book_dict_cache_t *c) { cache_release_memory(c);memset(c,0,sizeof(*c)); }
static void cache_break(book_dict_cache_t *c) { cache_release_memory(c);c->broken=true; }
void book_dictionary_cache_clear(void) {
    for(unsigned i=0;i<CACHE_ENTRIES;i++) {
        // 正在使用的项只作废，最后一个查询结束时再释放。/ Entries in use are invalidated and freed when the last lookup ends.
        if(s_cache[i].refs){s_cache[i].broken=true;s_cache[i].mtime=-1;}
        else cache_reset(&s_cache[i]);
    }
}
size_t book_dictionary_cache_bytes(void) {
    size_t n=0;
    for(unsigned i=0;i<CACHE_ENTRIES;i++)n+=(size_t)s_cache[i].cap*sizeof(checkpoint_t)+(s_cache[i].memory?s_cache[i].idx_size:0);
    return n;
}
bool book_dictionary_indexed(const book_dictionary_t *d) { return d&&d->indexed; }
static book_dict_cache_t *cache_acquire(const char *base,const char *idx_path,bool compressed,uint32_t idx_size,uint32_t words) {
    struct stat st;if(stat(idx_path,&st))return NULL;
    book_dict_cache_t *c=NULL,*victim=NULL;
    for(unsigned i=0;i<CACHE_ENTRIES;i++) {
        book_dict_cache_t *e=&s_cache[i];
        if(e->used&&!strcmp(e->base,base)){c=e;break;}
        if(!e->refs&&(!victim||!e->used||(victim->used&&e->lru<victim->lru)))victim=e;
    }
    if(c&&(c->idx_physical!=(uint32_t)st.st_size||c->mtime!=(int64_t)st.st_mtime||c->idx_size!=idx_size||c->words!=words||c->compressed!=compressed)) {
        if(c->refs)return NULL;
        cache_reset(c);victim=c;c=NULL;
    }
    if(!c) {
        if(!victim)return NULL;
        cache_reset(victim);c=victim;c->used=true;
        snprintf(c->base,sizeof(c->base),"%s",base);
        c->idx_physical=(uint32_t)st.st_size;c->mtime=(int64_t)st.st_mtime;c->idx_size=idx_size;c->words=words;c->compressed=compressed;
        // 检查点数量上限约 4096，超大词典放宽步长。/ Keep about 4096 checkpoints at most by widening the stride.
        c->stride=MIN_STRIDE;while(c->stride<(1u<<20)&&words/c->stride>4096u)c->stride<<=1;
    }
    c->refs++;c->lru=++s_cache_clock;return c;
}
static void cache_put(book_dictionary_t *d) {
    book_dict_cache_t *c=d->cache;d->cache=NULL;
    if(!c)return;
    if(c->refs)c->refs--;
    if(!c->refs&&c->broken&&c->mtime==-1)cache_reset(c);
}
// StarDict 顺序：先 ASCII 不分大小写，再逐字节。/ StarDict order: ASCII case-insensitive, then bytewise.
static int ascii_lower(unsigned char c){return c>='A'&&c<='Z'?c+32:c;}
static int casecmp(const char *a,const char *b) {
    for(;;a++,b++){int x=ascii_lower((unsigned char)*a),y=ascii_lower((unsigned char)*b);if(x!=y||!x)return x-y;}
}
static int stardict_cmp(const char *a,const char *b){int r=casecmp(a,b);return r?r:strcmp(a,b);}
// 检查点与查询比较：-1 小于，0 相等，1 大于，2 无法确定。/ Checkpoint vs query: -1,0,1 or 2 when unknown.
static int cp_cmp(const checkpoint_t *cp,const char *q) {
    size_t n=cp->len==CP_LONG?CP_KEY:cp->len;
    for(size_t i=0;i<n;i++) {
        int x=ascii_lower((unsigned char)cp->key[i]),y=ascii_lower((unsigned char)q[i]);
        if(x!=y)return x<y?-1:1;
        if(!y)return 1; // 查询更短 / The query is shorter
    }
    if(cp->len!=CP_LONG)return q[n]?-1:0;
    return 2;
}
// 记录第 at 条（位于 pos，长 bytes）；只在连续扩展时建立。/ Note record `at` at `pos`; build only when contiguous.
static void cache_note(book_dict_cache_t *c,uint32_t at,uint32_t pos,const char *word,bool full,uint32_t off,uint32_t len) {
    if(!c||c->complete||c->broken||at!=c->built||pos!=c->built_pos)return;
    if(!full||(at&&stardict_cmp(c->last,word)>0)){cache_break(c);return;}
    size_t key_len=strlen(word);uint32_t bytes=(uint32_t)key_len+9u;
    if(at%c->stride==0) {
        if(c->count==c->cap) {
            uint32_t cap=c->cap?c->cap*2:64;
            if((size_t)cap*sizeof(checkpoint_t)>CACHE_INDEX_CAP){cache_break(c);return;}
            checkpoint_t *p=psram_realloc(c->cps,(size_t)cap*sizeof(*p));if(!p){cache_break(c);return;}
            c->cps=p;c->cap=cap;
        }
        checkpoint_t *cp=&c->cps[c->count++];cp->pos=pos;
        cp->len=key_len>CP_KEY?CP_LONG:(uint8_t)key_len;memcpy(cp->key,word,key_len>CP_KEY?CP_KEY:key_len);
    }
    if(c->compressed) {
        if(c->memory_len+bytes>CACHE_MEMORY_CAP||c->memory_len+bytes>c->idx_size){cache_break(c);return;}
        if(!c->memory) {
            if(c->idx_size>CACHE_MEMORY_CAP){cache_break(c);return;}
            // 所有词典的 gzip 副本合计不超过 4 MiB：先释放空闲的其他副本。
            // gzip copies of all dictionaries stay within 4 MiB together: drop idle copies first.
            for(unsigned i=0;i<CACHE_ENTRIES;i++) {
                book_dict_cache_t *o=&s_cache[i];
                if(o!=c&&o->memory&&!o->refs)cache_reset(o);
                else if(o!=c&&o->memory&&o->idx_size+c->idx_size>CACHE_MEMORY_CAP){cache_break(c);return;}
            }
            c->memory=psram_alloc(c->idx_size);if(!c->memory){cache_break(c);return;}
        }
        unsigned char *m=c->memory+c->memory_len;memcpy(m,word,key_len+1);m+=key_len+1;
        m[0]=(unsigned char)(off>>24);m[1]=(unsigned char)(off>>16);m[2]=(unsigned char)(off>>8);m[3]=(unsigned char)off;
        m[4]=(unsigned char)(len>>24);m[5]=(unsigned char)(len>>16);m[6]=(unsigned char)(len>>8);m[7]=(unsigned char)len;
        c->memory_len+=bytes;
    }
    memcpy(c->last,word,key_len+1);c->built++;c->built_pos=pos+bytes;
    if(c->built==c->words&&c->built_pos==c->idx_size)c->complete=true;
}

static void slice_free(book_dictionary_t *d) {
    if(d->slice_owned)free(d->slice);
    d->slice=NULL;d->slice_owned=d->slice_loading=false;d->slice_len=0;
}
void book_dictionary_close(book_dictionary_t *d) {
    slice_free(d);cache_put(d);
    book_dict_file_close(d->index);
    book_dict_file_close(d->data);
    if(d->syn) fclose(d->syn);
    d->index=d->data=NULL;d->syn=NULL;
    d->state=0;
}
static int fail(book_dictionary_t *d,int state) {
    book_dictionary_close(d); d->state=state; return state;
}
static bool number(const char *s,uint32_t *n) {
    if(!*s) return false;
    uint64_t v=0;
    for(;*s;s++) { if(*s<'0'||*s>'9') return false; v=v*10+(unsigned)(*s-'0'); if(v>UINT32_MAX) return false; }
    *n=(uint32_t)v; return true;
}
static FILE *part(const char *base,const char *ext) {
    char path[BOOK_DICT_PATH];
    if(snprintf(path,sizeof(path),"%s%s",base,ext)>=(int)sizeof(path)) return NULL;
    return fopen(path,"rb");
}
static book_dict_file_t *file_part(const char *base,const char *plain,const char *packed,bool random,char *used,bool *compressed) {
    char path[BOOK_DICT_PATH];
    if(snprintf(path,sizeof(path),"%s%s",base,plain)>=(int)sizeof(path))return NULL;
    FILE *probe=fopen(path,"rb");
    if(probe) {
        fclose(probe);
        if(used)snprintf(used,BOOK_DICT_PATH,"%s",path);
        if(compressed)*compressed=false;
        return book_dict_file_open(path,false,random);
    }
    if(errno!=ENOENT||snprintf(path,sizeof(path),"%s%s",base,packed)>=(int)sizeof(path))return NULL;
    if(used)snprintf(used,BOOK_DICT_PATH,"%s",path);
    if(compressed)*compressed=true;
    return book_dict_file_open(path,true,random);
}
static bool plan_query(book_dictionary_t *d);
bool book_dictionary_open(book_dictionary_t *d,const char *ifo,const char *query) {
    book_dictionary_close(d);
    memset(d,0,sizeof(*d));
    size_t n=strlen(ifo),q=strlen(query);
    if(n<5||n>=sizeof(d->base)-8||strcmp(ifo+n-4,".ifo")||!q||q>=sizeof(d->query)) return fail(d,-1),false;
    memcpy(d->base,ifo,n-4); memcpy(d->query,query,q+1);
    FILE *f=fopen(ifo,"rb"); if(!f) return fail(d,-1),false;
    char line[512]; unsigned lines=0; bool header=false,version=false,types=false,words=false,idx=false;
    uint32_t index_size=0;
    while(fgets(line,sizeof(line),f)) {
        if(++lines>128) { fclose(f); return fail(d,-1),false; }
        if(!strchr(line,'\n')&&!feof(f)) {
            if(strncmp(line,"description=",12)){fclose(f);return fail(d,-1),false;}
            int c;unsigned skipped=0;
            while((c=fgetc(f))!=EOF&&c!='\n')if(++skipped>65536){fclose(f);return fail(d,-1),false;}
            continue;
        }
        line[strcspn(line,"\r\n")]=0;
        if(lines==1) { header=!strcmp(line,"StarDict's dict ifo file"); continue; }
        char *value=strchr(line,'='); if(!value) continue; *value++=0;
        if(!strcmp(line,"version")) version=!strcmp(value,"2.4.2")||!strcmp(value,"3.0.0");
        else if(!strcmp(line,"wordcount")) words=number(value,&d->words)&&d->words>0;
        else if(!strcmp(line,"idxfilesize")) idx=number(value,&index_size);
        else if(!strcmp(line,"idxoffsetbits")&&strcmp(value,"32")) { fclose(f); return fail(d,-1),false; }
        else if(!strcmp(line,"sametypesequence")) { types=*value&&strlen(value)<sizeof(d->types);
            for(const char *v=value;*v;v++)if(!strchr("mhgxt",*v))types=false;
            if(types)strcpy(d->types,value); }
        else if(!strcmp(line,"bookname")) snprintf(d->name,sizeof(d->name),"%.127s",value);
    }
    bool ok=!ferror(f); fclose(f);
    if(!header||!version||!types||!words||!idx||!ok) return fail(d,-1),false;
    char idx_path[BOOK_DICT_PATH];bool packed=false;
    d->index=file_part(d->base,".idx",".idx.gz",false,idx_path,&packed);
    d->data=file_part(d->base,".dict",".dict.dz",true,NULL,NULL);
    if(!d->index||!d->data||book_dict_file_size(d->index)!=index_size) return fail(d,-1),false;
    d->data_size=book_dict_file_size(d->data);
    d->syn=part(d->base,".syn"); d->state=1;
    d->cache=cache_acquire(d->base,idx_path,packed,index_size,d->words);
    if(d->cache&&d->cache->broken)cache_put(d);
    plan_query(d);
    return true;
}
static bool be32(FILE *f,uint32_t *v) {
    unsigned char b[4]; if(fread(b,1,4,f)!=4) return false;
    *v=((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3]; return true;
}
/* Long keys are skipped without allocating from untrusted sizes. */
static int key(book_dict_file_t *f,char *out,size_t cap) {
    size_t i=0,total=0; int c; bool overflow=false;
    while((c=book_dict_file_getc(f))!=EOF&&c) { if(++total>4096)return -1; if(i+1<cap) out[i++]=(char)c; else overflow=true; }
    out[i]=0; return c==EOF?-1:overflow?0:1;
}
static bool index32(book_dict_file_t *f,uint32_t *v) {
    unsigned char b[4];if(!book_dict_file_read(f,b,4))return false;
    *v=(uint32_t)b[0]<<24|(uint32_t)b[1]<<16|(uint32_t)b[2]<<8|b[3];return true;
}
static int synonym_key(FILE *f,char *out,size_t cap) {
    size_t i=0,total=0;int c;bool overflow=false;
    while((c=fgetc(f))!=EOF&&c){if(++total>4096)return -1;if(i+1<cap)out[i++]=(char)c;else overflow=true;}
    out[i]=0;return c==EOF?-1:overflow?0:1;
}
static bool equal(const char *a,const char *b) {
    /* ASCII case variants only; non-ASCII bytes and Vietnamese accents remain exact. */
    while(*a&&*b) { unsigned char x=*a++,y=*b++; if(x>='A'&&x<='Z')x+=32; if(y>='A'&&y<='Z')y+=32; if(x!=y)return false; }
    return !*a&&!*b;
}
static int definition(book_dictionary_t *d,uint32_t off,uint32_t len) {
    if(off>d->data_size||len>d->data_size-off)return fail(d,-1);
    size_t n=len<sizeof(d->text)-1?len:sizeof(d->text)-1;
    if(!book_dict_file_begin(d->data,off,d->text,n))return fail(d,-1);
    d->definition_length=len;d->definition_bytes=n;d->state=5;return 5;
}
static int definition_finish(book_dictionary_t *d) {
    int result=book_dict_file_step(d->data);
    if(result<0)return fail(d,-1);
    if(!result)return 5;
    size_t n=d->definition_bytes;uint32_t len=d->definition_length;
    d->truncated=n<len;
    /* Cut only at a UTF-8 boundary when limiting a long definition. */
    if(d->truncated) { while(n&&((unsigned char)d->text[n-1]&0xc0)==0x80) --n; if(n&&(unsigned char)d->text[n-1]>=0xc0)--n; }
    d->text[n]=0;
    size_t source=0,out=0,fields=strlen(d->types);
    for(size_t field=0;field<fields&&source<n;field++) {
        size_t stop=n;
        if(field+1<fields) {
            char *zero=memchr(d->text+source,0,n-source);
            if(zero)stop=(size_t)(zero-d->text);
            else if(!d->truncated)return fail(d,-1);
        }
        bool markup=strchr("hgx",d->types[field])!=NULL,tag=false;
        for(size_t i=source;i<stop;i++) {
            unsigned char c=(unsigned char)d->text[i];
            if(markup&&c=='<'){tag=true;if(out&&d->text[out-1]!=' ')d->text[out++]=' ';}
            else if(markup&&c=='>')tag=false;
            else if(!tag) {
                if(markup&&c=='&') {
                    const char *entities[]={"&amp;","&lt;","&gt;","&quot;","&apos;","&nbsp;"};
                    const char values[]="&<>\"' ";bool entity=false;
                    for(unsigned e=0;e<6;e++){size_t len=strlen(entities[e]);if(len<=stop-i&&!memcmp(d->text+i,entities[e],len)){d->text[out++]=values[e];i+=len-1;entity=true;break;}}
                    if(entity)continue;
                }
                d->text[out++]=(c<32&&c!='\n'&&c!='\t')?' ':c;
            }
        }
        source=stop<n?stop+1:n;
        if(field+1<fields&&source<n)d->text[out++]='\n';
    }
    d->text[out]=0;
    /* Reject malformed UTF-8 before text reaches glyph/UI decoders. */
    for(size_t i=0;i<out;) {
        unsigned char c=(unsigned char)d->text[i];unsigned count=0;uint32_t cp=0,min=0;
        if(c<128){++i;continue;}
        if(c>=0xc2&&c<=0xdf){count=1;cp=c&31;min=0x80;}
        else if(c>=0xe0&&c<=0xef){count=2;cp=c&15;min=0x800;}
        else if(c>=0xf0&&c<=0xf4){count=3;cp=c&7;min=0x10000;}
        else return fail(d,-1);
        if(i+count>=out)return fail(d,-1);
        for(unsigned j=1;j<=count;j++){unsigned char b=(unsigned char)d->text[i+j];if((b&0xc0)!=0x80)return fail(d,-1);cp=(cp<<6)|(b&63);}
        if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return fail(d,-1);
        i+=count+1;
    }
    book_dictionary_close(d); d->state=4; return 4;
}
/* ---- 按索引片段查找 / Indexed slice lookup ---- */
static bool slice_begin(book_dictionary_t *d,uint32_t at,uint32_t end,uint32_t record) {
    book_dict_cache_t *c=d->cache;
    if(end<at||end-at>SLICE_CAP)return false;
    d->slice_at=at;d->slice_len=end-at;d->slice_record=record;
    if(c->compressed) {
        if(!c->memory||end>c->memory_len)return false;
        d->slice=c->memory+at;d->slice_owned=false;return true;
    }
    d->slice=psram_alloc(d->slice_len?d->slice_len:1);if(!d->slice)return false;
    d->slice_owned=true;
    if(!book_dict_file_begin(d->index,at,d->slice,d->slice_len)){slice_free(d);return false;}
    d->slice_loading=true;return true;
}
// 为主查询选择索引片段，或决定顺序扫描的起点。/ Pick an indexed slice for the query, or where a sequential scan starts.
static bool plan_query(book_dictionary_t *d) {
    book_dict_cache_t *c=d->cache;
    d->state=1;d->scanned=0;d->skip_to=0;
    if(!c||c->broken||!c->count)return false;
    int start=-1;uint32_t end_index=c->count;
    for(uint32_t i=0;i<c->count;i++) {
        int r=cp_cmp(&c->cps[i],d->query);
        if(r==-1)start=(int)i;
        else if(r==1){end_index=i;break;}
    }
    uint32_t at=start>=0?c->cps[start].pos:0,record=start>=0?(uint32_t)start*c->stride:0,end;
    if(end_index<c->count) {
        end=c->cps[end_index].pos;
        // 多读一个检查点区间，让未命中也有后续相邻词。/ Read one more interval so misses still get following neighbours.
        uint32_t more=end_index+1<c->count?c->cps[end_index+1].pos:c->complete?c->idx_size:c->built_pos;
        if(more-at<=SLICE_CAP)end=more;
    }
    else if(c->complete)end=c->idx_size;
    else if(casecmp(c->last,d->query)>0)end=c->built_pos;
    else {
        // 查询超出已建部分：从最后一个确定更小的检查点继续顺序扫描。
        // The query lies past the built part: resume the sequential scan at the last smaller checkpoint.
        if(start>=0){d->skip_to=at;d->scanned=record;}
        return false;
    }
    if(!slice_begin(d,at,end,record))return false;
    d->indexed=c->complete;d->state=6;return true;
}
static bool plan_alias(book_dictionary_t *d) {
    book_dict_cache_t *c=d->cache;
    if(!c||c->broken||!c->count)return false;
    uint32_t i=d->target/c->stride;if(i>=c->count)return false;
    uint32_t end;
    if(i+1<c->count)end=c->cps[i+1].pos;
    else if(c->complete)end=c->idx_size;
    else if(d->target<c->built)end=c->built_pos;
    else return false;
    if(!slice_begin(d,c->cps[i].pos,end,i*c->stride))return false;
    d->state=7;return true;
}
static void suggest(book_dictionary_t *d,const char *word) {
    if(d->suggestion_count>=BOOK_DICT_SUGGESTIONS||casecmp(word,d->query)<=0)return;
    snprintf(d->suggestions[d->suggestion_count++],BOOK_DICT_SUGGESTION_CAP,"%s",word);
}
static uint32_t be32_at(const unsigned char *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
// 解析片段；返回下一状态。/ Parse the slice and return the next state.
static int slice_scan(book_dictionary_t *d,bool alias) {
    if(d->slice_loading) {
        int r=book_dict_file_step(d->index);
        if(r<0)return fail(d,-1);
        if(!r)return d->state;
        d->slice_loading=false;
    }
    const unsigned char *p=d->slice,*end=d->slice+d->slice_len;uint32_t record=d->slice_record;
    const char *previous=NULL;
    while(p<end) {
        const unsigned char *zero=memchr(p,0,(size_t)(end-p));
        if(!zero||end-zero<9)return fail(d,-1);
        const char *word=(const char*)p;uint32_t off=be32_at(zero+1),len=be32_at(zero+5);
        if(off>d->data_size||len>d->data_size-off)return fail(d,-1);
        if(alias?record==d->target:equal(word,d->query)) {
            slice_free(d);return definition(d,off,len);
        }
        if(!alias) {
            // 记录插入点前后的相邻词。/ Remember neighbours around the insertion point.
            if(casecmp(word,d->query)>0) {
                if(!d->suggestion_count&&previous)snprintf(d->suggestions[d->suggestion_count++],BOOK_DICT_SUGGESTION_CAP,"%s",previous);
                suggest(d,word);
            }
            previous=word;
        }
        p=zero+9;record++;
    }
    if(!alias&&!d->suggestion_count&&previous)snprintf(d->suggestions[d->suggestion_count++],BOOK_DICT_SUGGESTION_CAP,"%s",previous);
    slice_free(d);
    if(alias)return fail(d,-1);
    if(d->syn){d->state=2;return 2;}
    return fail(d,-2);
}
int book_dictionary_step(book_dictionary_t *d) {
    /* Spend useful CPU time instead of waiting a UI tick after 128 entries.
     * Bound work by both records and elapsed time so input can still cancel. */
#ifdef ESP_PLATFORM
    const int64_t began=esp_timer_get_time();
#endif
    for(unsigned budget=0;budget<4096;budget++) {
#ifdef ESP_PLATFORM
        if(budget&&!(budget&15)&&esp_timer_get_time()-began>=8000)break;
#endif
        if(d->state==6||d->state==7) {
            int state=d->state,r=slice_scan(d,state==7);
            if(r==state&&d->slice_loading)continue;
            return r;
        }
        if(d->state==1||d->state==3) {
            if(d->skip_to>book_dict_file_position(d->index)) {
                int r=book_dict_file_skip(d->index,d->skip_to,16);
                if(r<0)return fail(d,-1);
                continue;
            }
            if(d->scanned>=d->words) {
                if(!book_dict_file_end(d->index))return fail(d,-1);
                if(d->state==1&&d->syn){d->state=2;continue;}
                return fail(d,-2);
            }
            char word[256]; uint32_t off,len;
            uint32_t pos=book_dict_file_position(d->index);
            int k=key(d->index,word,sizeof(word));
            if(k<0||!index32(d->index,&off)||!index32(d->index,&len)||off>d->data_size||len>d->data_size-off) return fail(d,-1);
            uint32_t at=d->scanned++;
            if(d->state==1){cache_note(d->cache,at,pos,word,k==1,off,len);if(d->cache&&d->cache->complete)d->indexed=true;}
            if((d->state==3&&at==d->target)||(d->state==1&&k&&equal(word,d->query))) return definition(d,off,len);
        } else if(d->state==2) {
            if(feof(d->syn)) return fail(d,-2);
            int first=fgetc(d->syn); if(first==EOF)return fail(d,ferror(d->syn)?-1:-2); ungetc(first,d->syn);
            char word[256]; uint32_t target; int k=synonym_key(d->syn,word,sizeof(word));
            if(k<0||!be32(d->syn,&target)||target>=d->words) return fail(d,-1);
            if(k&&equal(word,d->query)) {
                d->target=target;
                if(plan_alias(d))continue;
                d->scanned=0;d->skip_to=0;d->state=3;
                if(!book_dict_file_rewind(d->index))return fail(d,-1);
            }
        } else if(d->state==5)return definition_finish(d);
        else return d->state;
    }
    return d->state;
}
