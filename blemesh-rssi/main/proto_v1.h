// main/proto_v1.h
#pragma once
#include <stdint.h>

#define PROTO_VER_V1  1u

typedef uint32_t node_id_t;   // 4B 축약 ID 추천
typedef int8_t   rssi_dbm_t;

typedef enum {
    OBS_KIND_ADV       = 0,  // GAP Advertising
    OBS_KIND_MESH_ADV  = 1,  // Mesh ADV bearer
    OBS_KIND_MESH_GATT = 2   // Mesh via GATT proxy
} obs_kind_t;

#define RSSI_SENTINEL_INVALID  (127)
#define TXP_SENTINEL_UNKNOWN   (127)