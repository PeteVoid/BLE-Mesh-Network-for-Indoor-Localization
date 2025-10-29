#pragma once
#include <stdbool.h>
#include "proto_v1.h"

void advscan_init_and_start(void);

// Anchor 모드에서 수신시 상위로 전달될 콜백 시그니처(스텁)
typedef void (*on_adv_observed_cb_t)(node_id_t from_id, RssiDbm rssi, EpochMs ts_ms);
// 내부에서 등록 저장
void advscan_set_observer(on_adv_observed_cb_t cb);