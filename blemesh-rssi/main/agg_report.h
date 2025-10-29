#pragma once
#include <stdbool.h>
#include "proto_v1.h"
#include "report_v1.h"

void agg_init(node_id_t self_reporter_id, uint16_t window_ms);

// 광고 관측이 들어올 때 호출
void agg_on_adv_observed(node_id_t from_id, RssiDbm rssi_dbm, EpochMs ts_ms);

// 주기(타이머/태스크)에서 호출 → 필요시 flush & publish
void agg_periodic_tick(EpochMs now_ms);