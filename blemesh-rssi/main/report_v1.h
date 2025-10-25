// main/report_v1.h
#pragma once
#include "proto_v1.h"

typedef struct {
    node_id_t  from_id;
    node_id_t  to_id;       // reporter (나)
    uint8_t    kind;        // obs_kind_t
    uint16_t   seq;         // ADV 필수, MESH는 0 허용
    rssi_dbm_t rssi_med;
    rssi_dbm_t p25;
    rssi_dbm_t p75;
    uint8_t    n;
    int8_t     txp_dbm;     // 127면 미지정
    uint8_t    recv_ttl;    // 0이면 미지정
} report_entry_v1_t;

#define REPORT_V1_MAX_ENTRIES  12

typedef struct {
    uint8_t     ver;         // PROTO_VER_V1
    node_id_t   reporter_id; // 나
    uint16_t    window_ms;   // 1000
    uint16_t    report_seq;  // 디듑용
    uint8_t     entry_count;
    report_entry_v1_t entries[REPORT_V1_MAX_ENTRIES];
} report_v1_t;