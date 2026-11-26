from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
p=Path(__file__).resolve().parents[1]/'components/ble_page_turner/src/ble_page_turner.c'
s=p.read_text();code='#include <stdint.h>\n#include <assert.h>\n'+function('reconnect_delay_ms',p)+'''\nint main(void){assert(reconnect_delay_ms(0)==4000);assert(reconnect_delay_ms(1)==8000);assert(reconnect_delay_ms(2)==16000);assert(reconnect_delay_ms(3)==32000);assert(reconnect_delay_ms(4)==64000);assert(reconnect_delay_ms(5)==128000);assert(reconnect_delay_ms(6)==256000);for(unsigned n=7;n<1000;n++)assert(reconnect_delay_ms(n)==300000);}'''
assert 's_link_up = true;\n            s_reconnect_failures=0;' in s
assert 's_reconnect_failures=0;' in function('reconnect_recent_requested',p)
assert 'reconnect_delay_ms(s_reconnect_failures)' in function('poll_locked',p)
with tempfile.TemporaryDirectory() as t:
 d=Path(t);(d/'test.c').write_text(code)
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(d/'test.c'),'-o',str(d/'test')],check=True);subprocess.run([str(d/'test')],check=True)
print('PASS bounded BLE retry4/8/16/32/64/128/256/300s, success/manual reset and poll wiring')

code = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
enum {STEP_IDLE,STEP_LINK,STEP_READY};
static bool s_running,s_connecting,s_scanning,s_link_up,s_connected;
static int s_step;
static char s_pending_addr[18];
static atomic_bool s_recent_requested,s_host_ready,s_stop_active;
"""+function('ble_pt_idle_ready',p)+r"""
int main(void){
 assert(ble_pt_idle_ready());s_running=true;assert(!ble_pt_idle_ready());
 s_host_ready=true;assert(ble_pt_idle_ready());
 s_connecting=true;assert(!ble_pt_idle_ready());s_connecting=false;
 s_link_up=true;s_step=STEP_LINK;assert(!ble_pt_idle_ready());
 s_connected=true;s_step=STEP_READY;assert(ble_pt_idle_ready());
 s_scanning=true;assert(!ble_pt_idle_ready());s_scanning=false;
 s_pending_addr[0]='A';assert(!ble_pt_idle_ready());s_pending_addr[0]=0;
 s_recent_requested=true;assert(!ble_pt_idle_ready());s_recent_requested=false;
 s_stop_active=true;assert(!ble_pt_idle_ready());
}
"""
with tempfile.TemporaryDirectory() as d:
 path=Path(d);(path/'stable.c').write_text(code)
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror',str(path/'stable.c'),'-o',str(path/'stable')],check=True)
 subprocess.run([str(path/'stable')],check=True)
print('PASS BLE idle gate:stable link/backoff eligible; startup,pairing,scan,pending target and shutdown rejected')
