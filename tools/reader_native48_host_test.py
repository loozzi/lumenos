#!/usr/bin/env python3
"""Validate production GL48 routing against actual S31 tables without hardware.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
source = root/'boards/metalio47/src/main/display_reader_waveform.c'
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "epdiy.h"
#include "e0470_epaper_waveform.h"
static bool aa;
bool app_settings_book_antialias(void){return aa;}
extern const EpdWaveformPhases my_gl_25_0,my_gc_25_0,my_du_25_0;
'''
for name in ('display_reader_text_waveform', 'display_reader_ripple_waveform'):
    unit += function(name, source)+'\n'
unit += r'''
int main(void){
 e0470_waveform_init();
 const EpdWaveformPhases *text=e0470_waveform_phases(&E0470_TEXTTURN_WAVEFORM,MODE_GL16);
 const EpdWaveformPhases *gc=e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_GC16);
 const EpdWaveformPhases *du=e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_DU);
 assert(text&&text->phases==48&&!text->phase_times&&gc->phases==48&&du->phases==20);
 uint8_t raw_gl[48*64],raw_gc[48*64],raw_du[20*64],expected[48*64];
 memcpy(raw_gl,my_gl_25_0.luts,sizeof(raw_gl));memcpy(raw_gc,my_gc_25_0.luts,sizeof(raw_gc));
 memcpy(raw_du,my_du_25_0.luts,sizeof(raw_du));memcpy(expected,text->luts,sizeof(expected));
 unsigned unchanged_white=0;
 for(int pass=0;pass<100;++pass){
  aa=pass%2;
  const EpdWaveform *wave=display_reader_text_waveform();
  assert(wave==&E0470_TEXTTURN_WAVEFORM&&display_reader_ripple_waveform()==wave);
  const EpdWaveformPhases *actual=e0470_waveform_phases(wave,MODE_GL16);
  for(int to=0;to<16;++to)for(int from=0;from<16;++from)for(int f=0;f<48;++f){
   int value=e0470_phase_action(actual,f,to,from);
   assert(value==e0470_phase_action(text,f,to,from));
   if(to!=from)assert(value==e0470_phase_action(&my_gl_25_0,f,to,from));
   else if(to!=15)assert(value==0); // Existing equal-ink hold retained.
   else if(pass==0)unchanged_white+=value==2;
  }
 }
 assert(unchanged_white>0); // Preserve the existing white maintenance path.
 assert(!memcmp(expected,text->luts,sizeof(expected))&&!memcmp(raw_gl,my_gl_25_0.luts,sizeof(raw_gl)));
 assert(!memcmp(raw_gc,my_gc_25_0.luts,sizeof(raw_gc))&&!memcmp(raw_du,my_du_25_0.luts,sizeof(raw_du)));
 puts("PASS native48: all256transitions/48phases match existing text path, AA unchanged, ripple uses same unboosted table, white maintenance retained; raw GL48/GC48/DU20 immutable");
}
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    (work/'test.c').write_text(unit)
    cmd = ['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',
           '-DDPREADER_BOARD_METALIO47=1']
    for include in ['main','tools/waveform_host_stubs','tools/shelf_waveform_host_stubs',
                    'components/epdiy/include','components/epdiy/src',
                    'components/e0470_epaper_waveform/include','components/e0470_epaper_waveform/waveforms',
                    'components/e0470_epaper_waveform/metalio']:
        cmd += ['-I'+str(root/include)]
    cmd += [str(work/'test.c'),
            str(root/'boards/metalio47/src/components/e0470_epaper_waveform/e0470_epaper_waveform.c'),
            str(root/'components/e0470_epaper_waveform/e0470_waveform_trim.c'),'-o',str(work/'test')]
    subprocess.run(cmd,check=True)
    subprocess.run([str(work/'test')],check=True)
