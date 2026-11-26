#!/usr/bin/env python3
"""Execute the real panic wrapper with a bounded RISC-V frame and RTC storage."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'esp_private').mkdir();(p/'riscv').mkdir()
 (p/'esp_attr.h').write_text('#define RTC_NOINIT_ATTR\n')
 (p/'esp_log.h').write_text('#define ESP_LOGE(tag,...) ((void)(tag))\n')
 (p/'esp_private/panic_internal.h').write_text('#include <stdint.h>\n#include <stdbool.h>\ntypedef struct{void *addr,*frame;int exception,core;const char *reason,*description;}panic_info_t;\nextern bool g_panic_abort;extern const char *g_panic_abort_details;\n#define PANIC_EXCEPTION_ABORT 99\n')
 (p/'riscv/rvruntime-frames.h').write_text('typedef struct{uint32_t ra,sp,mcause,mtval;}RvExcFrame;\n')
 (p/'test.c').write_text(r'''#include <assert.h>
#include <string.h>
#include "crash_note.c"
bool g_panic_abort;const char *g_panic_abort_details;
static unsigned forwarded;
void __real_esp_panic_handler(panic_info_t *info){(void)info;++forwarded;}
int main(void){
 memset(&s_note,0,sizeof(s_note));assert(!crash_note_valid());
 RvExcFrame frame={0x40001234,0x3ff01234,7,0xdeadbeef};
 panic_info_t panic={.addr=(void*)0x4200abcd,.frame=&frame,.exception=7,.core=1,.reason="store fault"};
 __wrap_esp_panic_handler(&panic);assert(forwarded==1&&crash_note_valid());
 assert(strstr(crash_note_text(),"ra=0x40001234")&&strstr(crash_note_text(),"sp=0x3ff01234")&&strstr(crash_note_text(),"mtval=0xdeadbeef"));
 assert(!strcmp(crash_note_detail(),"store fault"));crash_note_log();assert(crash_note_valid());
 char large[300];memset(large,'a',sizeof(large)-1);large[sizeof(large)-1]=0;
 g_panic_abort=true;g_panic_abort_details=large;panic.frame=NULL;
 __wrap_esp_panic_handler(&panic);assert(strlen(crash_note_detail())==CRASH_NOTE_DETAIL_SIZE-1&&s_note.sp==0&&s_note.exception==99);
 __wrap_esp_panic_handler(NULL);assert(forwarded==3&&crash_note_valid());
 puts("PASS panic RTC capture:PC/RA/SP/cause/trap, bounded abort details, forwarding and non-destructive export");}
''')
 subprocess.run(['cc','-std=gnu11','-D__riscv=1','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(p),'-I'+str(r/'main/app'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
