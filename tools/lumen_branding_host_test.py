#!/usr/bin/env python3
"""Brand assets/default-name migration retain existing device and storage identity."""
from pathlib import Path
import base64,hashlib,json,re,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'main/assets/branding/lumen-logo-source.png').read_bytes()
assert source==(ROOT/'web/assets/lumen-logo.png').read_bytes()
header=(ROOT/'main/assets/startup_logo.h').read_text()
assert hashlib.sha256(source).hexdigest() in header and 'Wordmark: LumenOS;' in header
portal=(ROOT/'components/read_pico_transfer/upload.html').read_text()
uri=re.search(r'data-dpreader-logo src="data:image/png;base64,([^"]+)"',portal)[1]
assert base64.b64decode(uri)==source
assert '<title>LumenOS' in portal and '<strong>LumenOS</strong>' in portal
web=(ROOT/'web/index.html').read_text()
assert '<title>LumenOS' in web and 'assets/lumen-logo.png' in web
assert 'Các gói trực tuyến bên dưới là bản dpReader đã phát hành' in web
assert 'nemmanquy' not in web
board=json.loads((ROOT/'boards/metalio47/board.json').read_text())
assert board['name']=='LumenOS Metalio 4.7' and board['project']=='dpReader'
assert board['settings_namespace']=='metalio_s31' and board['image_chip_id']==32
config=(ROOT/'boards/metalio47/dpreader_board_config.h').read_text()
for name,value in [('PROJECT','dpReader'),('PRODUCT_NAME','LumenOS'),('SETTINGS_NAMESPACE','metalio_s31'),('LOCK_NAMESPACE','metalio_lock'),('STARTUP_NAMESPACE','metalio_start'),('BACKUP_PREFIX','dpReader-settings')]:
    assert re.search(r'#define DPREADER_BOARD_'+name+r'\s+"'+re.escape(value)+'"',config),name
settings=(ROOT/'main/settings.c').read_text()
assert '"/dpReader-settings.backup"' in settings
assert '"dpReader-%02X%02X%02X%02X"' in (ROOT/'main/book/kosync_client.c').read_text()
network=(ROOT/'components/read_pico_transfer/read_pico_transfer.c').read_text()
if 'esp_netif_set_hostname' in network:
    assert 'esp_netif_set_hostname(s_netif, "dpreader")' in network
harness=(ROOT/'tools/settings_backup_host_test.c').read_text().split('int main(void)')[0]
harness=harness.replace('#include "../main/settings.c"','#include "'+str(ROOT/'main/settings.c')+'"')
harness=harness.replace('esp_err_t nvs_get_str(nvs_handle_t h, const char *key, char *value, size_t *size) {', '''static char saved_device_name[64];
esp_err_t nvs_get_str(nvs_handle_t h, const char *key, char *value, size_t *size) {
 if(!strcmp(key,NVS_KEY_DEVICE_NAME)&&saved_device_name[0]){
  if(strlen(saved_device_name)+1>*size)return ESP_FAIL;
  strcpy(value,saved_device_name);*size=strlen(value)+1;return ESP_OK;
 }''')
harness+=r'''
int main(void){
 app_settings_init();assert(!strcmp(app_settings_device_name(),"LumenOS"));
 const char *old[]={"Lumen","dpReader","Pico","kiikoread","Metalio S31"};
 for(unsigned i=0;i<sizeof(old)/sizeof(*old);++i){strcpy(saved_device_name,old[i]);app_settings_init();assert(!strcmp(app_settings_device_name(),"LumenOS"));}
 strcpy(saved_device_name,"Máy của tôi");app_settings_init();assert(!strcmp(app_settings_device_name(),saved_device_name));
 puts("PASS LumenOS assets/portal identity, legacy default-name migration, custom-name preservation and unchanged OTA/NVS/backup/KOSync identities");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(harness)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-I'+str(ROOT/'tools/settings_backup_stubs'),'-I'+str(ROOT/'components/read_pico_transfer/include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
