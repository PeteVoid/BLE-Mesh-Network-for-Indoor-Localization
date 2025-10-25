// main/mesh_vendor.c
#include "mesh_vendor.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"

static const char* TAG = "MESH";
static uint16_t g_report_group = 0xC100; // G_REPORT 예시
static esp_ble_mesh_model_t vendor_models[1];
static esp_ble_mesh_elem_t elements[1];
static esp_ble_mesh_comp_t comp;

static node_id_t g_self;

static void _vendor_msg_cb(esp_ble_mesh_model_t *model, esp_ble_mesh_msg_ctx_t *ctx,
                           esp_ble_mesh_server_recv_gen_onoff_set_t *ignored) {
    (void)model; (void)ignored;
    // 수신 콜백 예시 (VM_CTRL 수신 등)
    ESP_LOGI(TAG, "Vendor msg from 0x%04X, opcode=0x%06X, len=%d", ctx->addr, ctx->recv_op, ctx->recv_len);
}

void mesh_init_and_start(node_id_t self_id, bool enable_proxy, bool enable_relay) {
    g_self = self_id;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase(); nvs_flash_init();
    }

    esp_ble_mesh_register_prov_callback(NULL);
    esp_ble_mesh_register_generic_server_callback(NULL);
    esp_ble_mesh_register_custom_model_callback((esp_ble_mesh_model_cb_t)_vendor_msg_cb);

    // Vendor Model 선언
    static esp_ble_mesh_model_t vmodel = {
        .model_id = ESP_BLE_MESH_VND_MODEL_ID(VENDOR_COMPANY_ID, VENDOR_MODEL_ID),
        .op = NULL, // 필요시 op 테이블 구성
        .keys = {0},
        .pub = &(esp_ble_mesh_model_pub_t) {
            .msg = NET_BUF_SIMPLE(384),
            .dev_role = ESP_BLE_MESH_ROLE_NODE,
        },
    };
    vendor_models[0] = vmodel;

    elements[0].loc = 0;
    elements[0].model_count = 1;
    elements[0].models = vendor_models;

    comp.cid = VENDOR_COMPANY_ID;
    comp.element_count = 1;
    comp.elements = elements;

    esp_ble_mesh_init(&comp);
    if (enable_proxy) esp_ble_mesh_proxy_client_enable();
    if (enable_relay) esp_ble_mesh_node_enable_relay(true);

    // 키/바인딩/퍼브서브는 프로비저닝 단계에서 적용(도구로 주입). 여기선 스켈레톤.
    ESP_LOGI(TAG, "Mesh started (proxy=%d, relay=%d)", enable_proxy, enable_relay);
}

bool mesh_publish_report(const uint8_t* cbor, uint16_t len) {
    esp_ble_mesh_msg_ctx_t ctx = {
        .net_idx = 0, .app_idx = 0,
        .addr = g_report_group,
        .send_ttl = 2, .send_rel = false,
    };
    esp_ble_mesh_model_t* m = &vendor_models[0];
    if (!m->pub || !m->pub->msg) return false;
    struct net_buf_simple* b = m->pub->msg;
    net_buf_simple_reset(b);
    // Opcode: VENDOR (3B) – IDF의 vnd opcode macro 사용 권장, 여기선 간단히 raw push
    // [주의] 실제 구현에선 esp_ble_mesh_model_publish 사용 시 opcode 조립 필요
    // 여기선 payload만 넣는 스켈레톤:
    net_buf_simple_add_mem(b, cbor, len);
    return esp_ble_mesh_model_publish(m, &ctx, b->data, b->len, 0) == ESP_OK;
}