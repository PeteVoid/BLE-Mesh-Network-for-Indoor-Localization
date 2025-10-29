// main/report_v1_cbor.c
#include "report_v1_cbor.h"
#include <string.h>

bool report_v1_cbor_encode(const report_v1_t* rpt, uint8_t* out_buf, size_t buf_sz, size_t* out_len){
    if (!rpt || !out_buf || buf_sz < sizeof(*rpt)) return false;
    memcpy(out_buf, rpt, sizeof(*rpt));
    if (out_len) *out_len = sizeof(*rpt);
    return true;
}

bool report_v1_cbor_decode(report_v1_t* out_rpt, const uint8_t* buf, size_t len){
    if (!out_rpt || !buf || len < sizeof(*out_rpt)) return false;
    memcpy(out_rpt, buf, sizeof(*out_rpt));
    return true;
}