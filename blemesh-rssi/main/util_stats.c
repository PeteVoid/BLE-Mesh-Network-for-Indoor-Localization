// main/util_stats.c
#include "util_ringbuf.h"
#include <string.h>
#include <stdlib.h>

static int cmp_i8(const void* a, const void* b){ return (*(int8_t*)a) - (*(int8_t*)b); }

void rssi_compute_iqr(const rssi_win_t* w, rssi_dbm_t* p25, rssi_dbm_t* med, rssi_dbm_t* p75) {
    if (w->count == 0){ *p25=*med=*p75=0; return; }
    rssi_dbm_t buf[16]; memcpy(buf, w->data, w->count);
    qsort(buf, w->count, sizeof(buf[0]), cmp_i8);
    int n=w->count;
    int i25=(n-1)*0.25, i50=(n-1)*0.5, i75=(n-1)*0.75;
    *p25 = buf[i25]; *med = buf[i50]; *p75 = buf[i75];
}