#!/usr/bin/env python3
"""Exercise the real S31 power/latency counters used by log export.

中文：编译真实的 metalio_power_stats.c，验证等待/刷屏/浅睡累加、前光积分、
锁屏电量、解锁标记、直方图分桶与百分位。
English: compile the real metalio_power_stats.c and check wait/scan/sleep accumulation,
the front-light integral, lock SOC, the unlock mark, bucketing and percentiles.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
UNIT = r'''
#include <assert.h>
#include <stdio.h>
#include "metalio_power_stats.h"
int main(void){
 metalio_power_stats_t s=metalio_power_stats(0);
 assert(s.lock_soc==-1&&s.unlock_soc==-1&&!s.waits_idle&&!s.lamp_on_ms);
 metalio_power_note_wait(true,900);metalio_power_note_wait(true,100);metalio_power_note_wait(false,40);
 metalio_power_note_scan(METALIO_SCAN_GC);metalio_power_note_scan(METALIO_SCAN_DU);metalio_power_note_scan(METALIO_SCAN_DU);
 metalio_power_note_scan((metalio_scan_kind_t)9); // out of range is ignored
 metalio_power_note_light_sleep(60000,1);metalio_power_note_light_sleep(5000,0);
 s=metalio_power_stats(0);
 assert(s.wait_idle_ms==1000&&s.waits_idle==2&&s.wait_legacy_ms==40&&s.waits_legacy==1);
 assert(s.scans[METALIO_SCAN_GC]==1&&s.scans[METALIO_SCAN_GL]==0&&s.scans[METALIO_SCAN_DU]==2);
 assert(s.light_sleeps==2&&s.light_sleep_ms==65000&&s.maintenance_wakes==1);
 // Front light: 50% for 10 s, then 100% for 2 s, then off for 5 s.
 metalio_power_note_lamp(50,1000000);metalio_power_note_lamp(100,11000000);metalio_power_note_lamp(0,13000000);
 s=metalio_power_stats(18000000);
 assert(s.lamp_on_ms==12000&&s.lamp_level_ms==5000+2000);
 // Snapshot while lit closes the running interval without double counting.
 metalio_power_note_lamp(150,20000000); // clamps to 100%
 s=metalio_power_stats(21000000);assert(s.lamp_on_ms==13000&&s.lamp_level_ms==8000);
 s=metalio_power_stats(21000000);assert(s.lamp_on_ms==13000);
 metalio_power_note_lock(true,42,3800);metalio_power_note_lock(false,41,3790);metalio_power_note_lock(false,250,70000);
 s=metalio_power_stats(21000000);assert(s.locks==1&&s.lock_soc==42&&s.lock_mv==3800&&s.unlock_soc==-1&&s.unlock_mv==0);
 assert(!metalio_power_take_unlock_mark());metalio_power_mark_unlock(5);assert(metalio_power_take_unlock_mark()==5&&!metalio_power_take_unlock_mark());
 metalio_power_mark_unlock(0);assert(metalio_power_take_unlock_mark()==1); // a zero timestamp still marks
 // Buckets: <50,<100,<200,<400,<800,<1600,slower; boundaries go up.
 const uint32_t samples[]={0,49,50,99,100,199,200,399,400,799,800,1599,1600,5000};
 for(unsigned i=0;i<sizeof(samples)/sizeof(*samples);i++)metalio_power_note_latency(METALIO_LATENCY_TURN,samples[i]);
 metalio_power_note_latency(METALIO_LATENCY_COUNT,10); // ignored
 s=metalio_power_stats(0);
 for(unsigned b=0;b<METALIO_LATENCY_BUCKETS;b++)assert(s.latency[METALIO_LATENCY_TURN][b]==2);
 assert(metalio_power_percentile(s.latency[METALIO_LATENCY_TURN],50)==400);
 assert(metalio_power_percentile(s.latency[METALIO_LATENCY_TURN],95)==UINT32_MAX);
 assert(metalio_power_percentile(s.latency[METALIO_LATENCY_TURN],1)==50);
 assert(metalio_power_percentile(s.latency[METALIO_LATENCY_UI],50)==0); // no samples
 uint32_t fast[METALIO_LATENCY_BUCKETS]={95,5,0,0,0,0,0};
 assert(metalio_power_percentile(fast,95)==50&&metalio_power_percentile(fast,96)==100&&metalio_power_percentile(fast,200)==100);
 puts("power stats ok");return 0;
}
'''

if __name__ == '__main__':
    with tempfile.TemporaryDirectory() as d:
        p = Path(d)
        (p / 'test.c').write_text(UNIT)
        cmd = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
               '-I' + str(ROOT / 'components/metalio_hw/include'), str(p / 'test.c'),
               str(ROOT / 'components/metalio_hw/metalio_power_stats.c'), '-o', str(p / 'test')]
        subprocess.run(cmd, check=True)
        subprocess.run([str(p / 'test')], check=True)
    print('PASS real S31 power counters: waits/scans/light sleep, front-light integral and clamp, lock SOC, unlock mark, '
          'latency buckets and p50/p95 percentiles')
