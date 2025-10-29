// main/report_v1.h
#pragma once
#include "proto_v1.h"

#define REPORT_ENTRY_MAX  12

typedef struct __attribute__((packed)) {
    node_id_t from_id;
    node_id_t to_id;
    uint8_t   kind;
    uint32_t  seq;
    int8_t    rssi_med;
    int8_t    p25;
    int8_t    p75;
    uint8_t   n;
    int8_t    txp_dbm;
    uint8_t   recv_ttl;
} report_entry_v1_t;

typedef struct __attribute__((packed)) {
    uint8_t   ver;
    node_id_t reporter_id;
    uint16_t  window_ms;
    uint16_t  report_seq;
    uint8_t   entry_count;
    report_entry_v1_t entries[REPORT_ENTRY_MAX];
} report_v1_t;

// 간단 검증
bool report_v1_validate(const report_v1_t* r);