#pragma once
#include <stdint.h>
#include <stdbool.h>

#define PROTO_VER_V1            0x01
#define TXP_SENTINEL_UNKNOWN    0x7F

// 관측 종류
#define OBS_KIND_ADV            0x00
#define OBS_KIND_MESH_ADV       0x01
#define OBS_KIND_MESH_REPORT    0x02

typedef uint32_t node_id_t;
typedef uint64_t EpochMs;
typedef int8_t   RssiDbm;
typedef uint8_t  Ttl;
typedef uint32_t Seq;
typedef uint16_t ReportSeq;

typedef enum {
  RES_OK = 0,
  RES_ERR = -1
} Result;

// ---------- BLE Mesh 공용 상수 ----------

// 그룹 주소 (임시 예제 — 현장에 맞게 바꿔도 됨)
#define G_PING      0xC101  // Child PING (필요시 사용)
#define G_REPORT    0xC102  // REPORT publish 그룹
#define G_CTRL      0xC103  // CTRL 명령 수신 그룹

// Vendor Company ID / Model ID (테스트용)
// 회사 ID: 0xFFFF (벤더 테스트용), 모델 ID는 0x0001 사용
#define VENDOR_COMPANY_ID   0xFFFF
#define VENDOR_MODEL_ID     0x0001

// Vendor Model Opcodes (3-byte)
#define VM_OP_PING    0x01
#define VM_OP_REPORT  0x02
#define VM_OP_CTRL    0x03