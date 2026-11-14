#!/usr/bin/env python3
"""Real preferences capture/stage/restore and NVS reload for optional sleep keys."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
harness=(ROOT/'tools/settings_backup_host_test.c').read_text().split('int main(void)')[0]
harness=harness.replace('#include "../main/settings.c"','#include "'+str(ROOT/'main/settings.c')+'"')
harness=harness.replace('esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *value) {', '''static int wake_buttons=-1,wake_presses=-1;
esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *value) {
 if(!strcmp(key,"wake_buttons")&&wake_buttons>=0){*value=wake_buttons;return ESP_OK;}
 if(!strcmp(key,NVS_KEY_WAKE_PRESSES)&&wake_presses>=0){*value=wake_presses;return ESP_OK;}''')
harness=harness.replace('esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t value) {', '''esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t value) {
 if(!strcmp(key,"wake_buttons")&&!commit_fails)wake_buttons=value;
 if(!strcmp(key,NVS_KEY_WAKE_PRESSES)&&!commit_fails)wake_presses=value;''')
harness+=r'''
static void checksum(settings_backup_preferences_t *p){uint32_t h=preferences_checksum(p);for(unsigned i=0;i<4;++i)p->checksum[i]=h>>(8*i);}
int main(void){
 app_settings_init();assert(app_settings_wake_buttons()==0);
 for(unsigned keys=0;keys<4;++keys)for(unsigned presses=1;presses<=2;++presses){
  app_settings_set_wake_presses(presses);assert(app_settings_set_wake_buttons(keys)==ESP_OK);
  settings_backup_preferences_t p={0};assert(preferences_capture(&p)==ESP_OK);
  assert(p.wake_presses==(presses|(keys<<2))&&preferences_valid(&p));
  s_wake_buttons=3;s_wake_presses=2;
  assert(preferences_stage(1,&p)==ESP_OK&&wake_buttons==(int)keys&&wake_presses==(int)presses);
  assert(preferences_apply(&p)==ESP_OK&&app_settings_wake_buttons()==keys&&app_settings_wake_presses()==presses);
  s_wake_buttons=0;app_settings_init();assert(app_settings_wake_buttons()==keys&&app_settings_wake_presses()==presses);
  // Old V32 backups have no upper wake-key bits and restore power-only.
  p.wake_presses=presses;checksum(&p);assert(preferences_valid(&p));
  assert(preferences_apply(&p)==ESP_OK&&app_settings_wake_buttons()==0);
  for(unsigned bad=0;bad<256;++bad){
   p.wake_presses=bad;checksum(&p);
   bool valid=(bad&3)>=1&&(bad&3)<=2&&bad<=14;
   assert(preferences_valid(&p)==valid);
  }
 }
 wake_buttons=255;app_settings_init();assert(app_settings_wake_buttons()==0);
 commit_fails=true;assert(app_settings_set_wake_buttons(1)!=ESP_OK&&app_settings_wake_buttons()==0);
 puts("PASS: real wake preferences backup checksum, all key/count combinations, legacy backup, 256 corrupt masks, NVS reload and failed commit");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(harness)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-I'+str(ROOT/'tools/settings_backup_stubs'),'-I'+str(ROOT/'components/read_pico_transfer/include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
