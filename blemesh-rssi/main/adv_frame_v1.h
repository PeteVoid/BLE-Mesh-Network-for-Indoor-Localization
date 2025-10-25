// main/adv_frame_v1.h
#pragma once
#include "proto_v1.h"

#define ADV_COMPANY_ID     0x1234u
#define ADV_TYPE_PROJECT   0xA1u

typedef struct __attribute__((packed)) {
    uint8_t   ver;         // PROTO_VER_V1
    uint16_t  company_id;  // ADV_COMPANY_ID (LE)
    uint8_t   type;        // ADV_TYPE_PROJECT
    node_id_t from_id;     // Advertiser
    uint16_t  seq;         // origin seq
    int8_t    txp_dbm;     // Tx power (dBm) or 127
    uint8_t   flags;       // reserved
    uint8_t   crc8;        // optional app-level CRC
} adv_frame_v1_t;

// CRC-8/MAXIM 예시 (간단 참조 구현)
static inline uint8_t adv_crc8_maxim(const uint8_t* p, uint8_t len) {
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *p++;
        for (int i=0; i<8; ++i)
            crc = (crc & 0x01) ? (crc >> 1) ^ 0x8C : (crc >> 1);
    }
    return crc;
}

static inline void adv_build_v1(adv_frame_v1_t* f, node_id_t from, uint16_t seq, int8_t txp_dbm, uint8_t flags) {
    f->ver = PROTO_VER_V1; f->company_id = ADV_COMPANY_ID; f->type = ADV_TYPE_PROJECT;
    f->from_id = from; f->seq = seq; f->txp_dbm = txp_dbm; f->flags = flags;
    f->crc8 = adv_crc8_maxim((const uint8_t*)f, sizeof(adv_frame_v1_t)-1);
}

static inline bool adv_parse_v1(const uint8_t* buf, uint8_t len, adv_frame_v1_t* out) {
    if (len < sizeof(adv_frame_v1_t)) return false;
    const adv_frame_v1_t* f = (const adv_frame_v1_t*)buf;
    if (f->ver != PROTO_VER_V1 || f->company_id != ADV_COMPANY_ID || f->type != ADV_TYPE_PROJECT) return false;
    uint8_t crc = adv_crc8_maxim(buf, sizeof(adv_frame_v1_t)-1);
    if (crc != f->crc8) return false;
    *out = *f;
    return true;
}