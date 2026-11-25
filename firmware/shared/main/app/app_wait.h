/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stddef.h>
#include <stdint.h>
/* Absolute monotonic deadlines; only opt-in pages can extend the legacy wait. */
/* 中文：静态页全部周期工作都登记为截止时间后，单次等待上限可放宽到1秒。
 * English: once every periodic job is registered as a deadline, a static wait may last up to one second. */
#define APP_WAIT_LEGACY_CAP_MS 200
#define APP_WAIT_STATIC_CAP_MS 1000
static inline uint32_t app_wait_ms_capped(int64_t now,uint32_t legacy,int eligible,int pending,
                                         const int64_t *deadlines,size_t count,uint32_t cap) {
    if(pending)return 0;
    if(!eligible)return legacy;
    int64_t until=now+(int64_t)cap;
    for(size_t i=0;i<count;i++)if(deadlines[i]<until)until=deadlines[i];
    return until<=now?0:(uint32_t)(until-now);
}
static inline uint32_t app_wait_ms(int64_t now,uint32_t legacy,int eligible,
                                  int pending,const int64_t *deadlines,size_t count) {
    return app_wait_ms_capped(now,legacy,eligible,pending,deadlines,count,APP_WAIT_LEGACY_CAP_MS);
}
