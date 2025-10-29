#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "proto_v1.h"

// 메시 초기화/키/모델/퍼브·섭
Result mesh_init_and_start(node_id_t self_id, bool proxy, bool relay);
Result mesh_keys_bind(uint8_t netkey_id, uint8_t appkey_id);
Result model_bind_vendor(uint8_t appkey_id);
Result pubsub_config(const char* model, uint16_t pub, const uint16_t* subs, size_t subs_len);
Result gatt_proxy_set(bool enabled);

// REPORT publish (CBOR 페이로드)
bool mesh_publish_report(const uint8_t* payload, uint16_t len);