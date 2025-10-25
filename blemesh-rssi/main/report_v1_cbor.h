// main/report_v1_cbor.h
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "report_v1.h"

bool report_v1_cbor_encode(const report_v1_t* r, uint8_t* out_buf, size_t out_cap, size_t* out_len);
bool report_v1_cbor_decode(const uint8_t* buf, size_t len, report_v1_t* r_out);