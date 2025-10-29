// main/report_v1_cbor.h
#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "report_v1.h"

// 스켈레톤: 진짜 CBOR 대신 raw copy (나중 교체)
bool report_v1_cbor_encode(const report_v1_t* rpt, uint8_t* out_buf, size_t buf_sz, size_t* out_len);
bool report_v1_cbor_decode(report_v1_t* out_rpt, const uint8_t* buf, size_t len);