// main/mesh_vendor.h
#pragma once
#include <stdint.h>
#include "report_v1.h"

#define VENDOR_COMPANY_ID    0x1234
#define VENDOR_MODEL_ID      0x0001

// Vendor Opcodes
#define VM_PING   0x01
#define VM_REPORT 0x02
#define VM_CTRL   0x03

void mesh_init_and_start(node_id_t self_id, bool enable_proxy, bool enable_relay);
bool mesh_publish_report(const uint8_t* cbor, uint16_t len);  // publish to G_REPORT group