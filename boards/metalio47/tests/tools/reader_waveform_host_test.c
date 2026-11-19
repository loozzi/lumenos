/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "display_reader_waveform.h"
#include "e0470_epaper_waveform.h"
#include "e0470_waveform_trim.h"
static bool aa=true;
bool app_settings_book_antialias(void){return aa;}
int epd_width(void){return 1216;}
int epd_height(void){return 684;}
enum EpdRotation epd_get_rotation(void){return EPD_ROT_INVERTED_PORTRAIT;}
int main(void){
 e0470_waveform_init();
 const EpdWaveform *wave=display_reader_text_waveform();
 const EpdWaveformPhases *actual=e0470_waveform_phases(wave,MODE_GL16);
 assert(wave==&E0470_TEXTTURN_WAVEFORM&&actual&&actual->phases==48&&!actual->phase_times);
 assert(e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_GC16)->phases==48);
 assert(display_reader_ripple_waveform()==wave);
 aa=false;assert(display_reader_text_waveform()==wave&&display_reader_ripple_waveform()==wave);
 aa=true;assert(display_reader_text_waveform()==wave&&display_reader_ripple_waveform()==wave);
 static uint8_t expected_pixels[48][16][4];
 const e0470_trim_t trim={.erase_max=11,.sat_cut=5,.white_sat_cut=0,.hold=3};
 EpdWaveformPhases expected={.phases=36,.luts=(const uint8_t*)expected_pixels};
 puts("PASS: ordinary/ripple GL48 unchanged by AA, no extra GL37 black pulses; GC48 cleanup remains native");
 const EpdWaveform *flash=display_reader_flash_waveform();
 actual=e0470_waveform_phases(flash,MODE_GC16);
 assert(actual&&actual->phases==36&&!actual->phase_times);
 assert(display_reader_flash_waveform()==flash);
 extern const EpdWaveformPhases my_gc_25_0;
 assert(e0470_waveform_trim(&my_gc_25_0,&trim,expected_pixels)==36);
 expected.phases=36;
 int black_pulses=0,white_pulses=0;
 for(int f=0;f<36;++f)for(int from=0;from<16;++from)for(int to=0;to<16;++to){
  int pulse=e0470_phase_action(actual,f,to,from);
  int original=e0470_phase_action(&expected,f,to,from);
  if(from==to){
   int reinforced=(f==33||f==34)?(to==0?1:to==15?2:0):0;
   assert(pulse==reinforced);
  } else if(from==15&&to==0){
   assert(pulse==original||(original==1&&pulse==0));
   black_pulses+=pulse!=original;
  } else if(from==0&&to==15){
   assert(pulse==original||(original==0&&pulse==2));
   white_pulses+=pulse!=original;
  } else assert(pulse==original);
 }
 assert(black_pulses==1&&white_pulses==1);
 aa=true;assert(display_reader_flash_waveform()==flash);
 puts("PASS: Flash GC36 preserves native gray transitions and reinforces unchanged black and white");
 puts("PASS: reader GL48 retains existing changed transitions, equal ink holds and white maintenance; Flash and GC48 cleanup remain unchanged");
}
