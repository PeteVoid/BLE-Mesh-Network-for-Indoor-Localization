// main/report_v1_cbor.c
#include "report_v1_cbor.h"
#include <tinycbor/cbor.h>

bool report_v1_cbor_encode(const report_v1_t* r, uint8_t* out_buf, size_t out_cap, size_t* out_len) {
    CborEncoder enc, map, arr, em;
    cbor_encoder_init(&enc, out_buf, out_cap, 0);

    if (cbor_encoder_create_map(&enc, &map, 5) != CborNoError) return false;
    cbor_encode_uint(&map, 1); cbor_encode_uint(&map, r->ver);
    cbor_encode_uint(&map, 2); cbor_encode_uint(&map, (uint64_t)r->reporter_id);
    cbor_encode_uint(&map, 3); cbor_encode_uint(&map, r->window_ms);
    cbor_encode_uint(&map, 4); cbor_encode_uint(&map, r->report_seq);
    cbor_encode_uint(&map, 5);
    if (cbor_encoder_create_array(&map, &arr, r->entry_count) != CborNoError) return false;

    for (uint8_t i=0;i<r->entry_count;i++) {
        const report_entry_v1_t* e = &r->entries[i];
        // 필수: from,to,kind,seq,rssi_med,n (6개) + 옵션(p25,p75,txp,ttl)
        int cnt = 6;
        if (e->p25 != 0) cnt++;
        if (e->p75 != 0) cnt++;
        if (e->txp_dbm != TXP_SENTINEL_UNKNOWN) cnt++;
        if (e->recv_ttl != 0) cnt++;

        if (cbor_encoder_create_map(&arr, &em, cnt) != CborNoError) return false;

        cbor_encode_uint(&em, 1); cbor_encode_uint(&em, (uint64_t)e->from_id);
        cbor_encode_uint(&em, 2); cbor_encode_uint(&em, (uint64_t)e->to_id);
        cbor_encode_uint(&em, 3); cbor_encode_uint(&em, e->kind);
        cbor_encode_uint(&em, 4); cbor_encode_uint(&em, e->seq);
        cbor_encode_uint(&em, 5); cbor_encode_int(&em,  e->rssi_med);
        cbor_encode_uint(&em, 8); cbor_encode_uint(&em,  e->n);

        if (e->p25 != 0) { cbor_encode_uint(&em, 6); cbor_encode_int(&em, e->p25); }
        if (e->p75 != 0) { cbor_encode_uint(&em, 7); cbor_encode_int(&em, e->p75); }
        if (e->txp_dbm != TXP_SENTINEL_UNKNOWN) { cbor_encode_uint(&em, 9); cbor_encode_int(&em, e->txp_dbm); }
        if (e->recv_ttl != 0) { cbor_encode_uint(&em, 10); cbor_encode_uint(&em, e->recv_ttl); }

        if (cbor_encoder_close_container(&arr, &em) != CborNoError) return false;
    }

    if (cbor_encoder_close_container(&map, &arr) != CborNoError) return false;
    if (cbor_encoder_close_container(&enc, &map) != CborNoError) return false;

    *out_len = cbor_encoder_get_buffer_size(&enc, out_buf);
    return true;
}

// 디코더는 필요 시 추가 (MVP에선 인코더만으로 충분)
bool report_v1_cbor_decode(const uint8_t* buf, size_t len, report_v1_t* r_out) {
    (void)buf; (void)len; (void)r_out;
    return false; // TODO (확장 시)
}