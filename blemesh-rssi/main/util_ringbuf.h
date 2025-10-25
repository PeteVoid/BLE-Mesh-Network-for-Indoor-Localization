// main/util_ringbuf.h  (간단 링버퍼 헤더 예시)
#pragma once
#include <stdint.h>
#include "proto_v1.h"

typedef struct {
    rssi_dbm_t data[16];
    uint8_t    count;
} rssi_win_t;

static inline void rssi_win_add(rssi_win_t* w, rssi_dbm_t v) {
    if (w->count < 16) w->data[w->count++] = v;
}

void rssi_compute_iqr(const rssi_win_t* w, rssi_dbm_t* p25, rssi_dbm_t* med, rssi_dbm_t* p75);