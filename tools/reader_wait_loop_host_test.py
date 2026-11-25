#!/usr/bin/env python3
"""Run production UI loop: idle extension, pending input zero-wait, active touch8ms."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'tools/app_loop_host_test.c').read_text()
s=s.replace('const app_desc_t app_book = {.title="book"};','''static int reader_inputs;static bool page_busy;
static int64_t ready_reader_deadline(app_ctx_t*c){return page_busy?0:c->now_ms+1000;}
static app_redraw_t reader_input(app_ctx_t*c,const cst836u_touch_t*t){(void)c;if(t->touched)++reader_inputs;return APP_REDRAW_NONE;}
const app_desc_t app_book = {.title="book",.next_tick_ms=ready_reader_deadline,.on_touch=reader_input};''')
legacy_delay='void vTaskDelay(int ms) {(void)ms;if (++step>=sample_count) longjmp(done,1);}'
recorded_delay='static int delays[32];\nvoid vTaskDelay(int ms) {if(step>=0&&step<32)delays[step]=ms;if (++step>=sample_count) longjmp(done,1);}'
# 中文：通用版与S31测试版的延时桩不同；两者都改为记录请求的等待。/ English: the generic and S31 harness delay stubs differ; both record the requested wait.
if legacy_delay in s:
    s=s.replace(legacy_delay,'static int requested_wait[32];\nvoid vTaskDelay(int ms) {requested_wait[step]=ms;if (++step>=sample_count) longjmp(done,1);}')
else:
    assert recorded_delay in s
    s=s.replace(recorded_delay,'static int delays[32];\nstatic int requested_wait[32];\nvoid vTaskDelay(int ms) {if(step>=0&&step<32)delays[step]=requested_wait[step]=ms;if (++step>=sample_count) longjmp(done,1);}')
# 中文：自2026-11-15稳定BLE可空闲等待；此处模拟BLE忙（连接/扫描/重连），应保持短等待。
# English: since 2026-11-15 a stable BLE link may idle; model a busy BLE stack (connect/scan/retry), which keeps short waits.
assert 'bool ble_pt_idle_ready(void) { return true; }' in s
s=s.replace('bool ble_pt_idle_ready(void) { return true; }','static bool ble_busy;\nbool ble_pt_idle_ready(void) { return !ble_busy; }')
s=s.replace('int main(void) {','int legacy_scheduler_main(void) {',1)

s+=r'''
bool test_volume_pending(void){return step==24;}
int main(void){reset();time_step=100000;
 for(int i=0;i<32;i++)add(0,0,0,0);
 samples[25]=(sample_t){.x=300,.y=600,.count=1};
 app_loop_config_t cfg={.fb=frame,.first_app=&app_book};if(!setjmp(done))app_loop_run(&cfg);
 ble_busy=false;bool extended=false;for(int i=20;i<24;i++)if(requested_wait[i]>40)extended=true;
 assert(extended&&requested_wait[24]==0&&requested_wait[25]<=8&&reader_inputs==1);
 for(int i=0;i<32;i++)assert(requested_wait[i]<=1000); // 2026-11-25: static waits may reach the 1 s cap.
 for(int mode=0;mode<3;mode++) {
  reset();time_step=100000;reader_inputs=0;page_busy=mode==2;ble_enabled=ble_busy=mode==0;
  if(mode==1)transfer_status.state=1;
  for(int i=0;i<32;i++)add(0,0,0,0);
  if(!setjmp(done))app_loop_run(&cfg);
  for(int i=0;i<32;i++)assert(requested_wait[i]<=40);
 }
 puts("PASS actual UI loop: ready idle reader extends beyond40ms, queued input bypasses wait, next touch dispatched with8ms active cadence and1s static maximum; BLE/network/page-work retain short waits");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(s)
 header=(r/'tools/app_loop_stubs/metalio_hw.h').read_text().replace('static inline bool metalio_volume_events_pending(void){return false;}','bool test_volume_pending(void);\nstatic inline bool metalio_volume_events_pending(void){return test_volume_pending();}')
 (p/'metalio_hw.h').write_text(header)
 subprocess.run(['python3',str(r/'tools/power_dialog_host_test.py'),'--input-c',str(p/'power.c')],check=True,cwd=r)
 args=['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-g','-I'+str(p)]
 for directory in ['tools/app_loop_stubs','main','components/ble_page_turner/include','main/app','main/ui','tools/ui_gesture_stubs','tools/anti_fade_stubs']:args+=['-I'+str(r/directory)]
 args += [str(p/'power.c'),str(p/'test.c'),str(r/'main/anti_fade.c'),str(r/'main/app/app_loop.c'),str(r/'main/ui/ui_gesture.c'),str(r/'main/ui/ui_quick_menu_input.c'),'-o',str(p/'test')]
 subprocess.run(args,check=True);subprocess.run([str(p/'test')],check=True)
