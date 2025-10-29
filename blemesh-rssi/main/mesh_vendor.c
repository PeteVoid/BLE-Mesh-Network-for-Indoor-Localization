#include "mesh_vendor.h"
#include "proto_v1.h"

#include "esp_log.h"
#include "esp_check.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bluedroid.h"
#include "nvs_flash.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_config_model_api.h"

static const char* TAG = "VENDOR";

// ---------- Net/App Key (테스트용; 실제는 Mother에서 주입) ----------
static const uint8_t s_netkey[16] = { 0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88, 0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xF0,0x01 };
static const uint8_t s_appkey[16] = { 0xA1,0xB2,0xC3,0xD4,0xE5,0xF6,0x07,0x18, 0x29,0x3A,0x4B,0x5C,0x6D,0x7E,0x8F,0x90 };
static const uint16_t s_netkey_idx = 0x0000;
static const uint16_t s_appkey_idx = 0x0000;

// ---------- 모델/퍼브/구독 컨텍스트 ----------
static esp_ble_mesh_model_t s_vnd_models[1];
static esp_ble_mesh_elem_t  s_elements[1];
static esp_ble_mesh_comp_t  s_comp;

static esp_ble_mesh_model_pub_t s_vnd_pub;       // 출판 컨텍스트
static uint8_t s_vnd_pub_buf_data[384];
static struct net_buf_simple s_vnd_pub_buf = {
    .data = s_vnd_pub_buf_data,
    .size = sizeof(s_vnd_pub_buf_data),
    .len  = 0,
    .__buf = s_vnd_pub_buf_data,
};

static uint16_t s_primary_addr = 0x0001;         // 프로비저닝 후 갱신됨
static bool     s_is_provisioned = false;

// ---------- 오퍼코드 정의 ----------
#define OP_VND(_op) ESP_BLE_MESH_MODEL_OP_3(_op, VENDOR_COMPANY_ID)

static const esp_ble_mesh_model_op_t vnd_op[] = {
    { OP_VND(VM_OP_PING),   0, NULL },  // 수신 핸들러 필요 시 콜백 연결
    { OP_VND(VM_OP_REPORT), 0, NULL },
    { OP_VND(VM_OP_CTRL),   0, NULL },
    ESP_BLE_MESH_MODEL_OP_END,
};

// ---------- 구성 서버/클라이언트 모델 ----------
static esp_ble_mesh_model_t s_cfg_srv_model = ESP_BLE_MESH_MODEL_CFG_SRV();
static esp_ble_mesh_model_t s_cfg_cli_model = ESP_BLE_MESH_MODEL_CFG_CLI();

// ---------- 프로비저닝/컴포지션 ----------
static uint8_t s_dev_uuid[16];

static esp_ble_mesh_prov_t s_prov = {
    .uuid = s_dev_uuid,
    .output_size = 0,
    .output_actions = 0,
    .prov_algorithm = ESP_BLE_MESH_PROV_ALG_P256,   // 기본
    .bearers = ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT,
};

static void gen_uuid_from_nodeid(node_id_t id)
{
    // 간단 UUID: 0xF0..F0 + node_id
    memset(s_dev_uuid, 0xF0, sizeof(s_dev_uuid));
    s_dev_uuid[12] = (id >> 24) & 0xFF;
    s_dev_uuid[13] = (id >> 16) & 0xFF;
    s_dev_uuid[14] = (id >> 8)  & 0xFF;
    s_dev_uuid[15] = (id >> 0)  & 0xFF;
}

// ---------- 콜백: 프로비저닝 ----------
static void prov_cb(esp_ble_mesh_prov_cb_event_t event, esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(TAG, "PROV_REGISTER_COMP status=%d", param->prov_register_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
        ESP_LOGI(TAG, "PROV_ENABLE_COMP bearers=0x%x", param->node_prov_enable_comp.bearers);
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
        ESP_LOGI(TAG, "PROV_LINK_OPEN bearer=%d", param->node_prov_link_open.bearer);
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(TAG, "PROV_LINK_CLOSE reason=0x%02X", param->node_prov_link_close.reason);
        break;
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        s_is_provisioned = true;
        s_primary_addr   = param->node_prov_complete.addr;
        ESP_LOGI(TAG, "PROV_COMPLETE addr=0x%04X net_idx=0x%04X", s_primary_addr, param->node_prov_complete.net_idx);
        break;
    case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
        s_is_provisioned = false;
        ESP_LOGW(TAG, "PROV_RESET");
        break;
    default:
        break;
    }
}

// ---------- 콜백: 구성 모델 ----------
static void cfg_srv_cb(esp_ble_mesh_cfg_server_cb_event_t event, esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        ESP_LOGI(TAG, "CFG_SRV state change: op=0x%04X", param->ctx.recv_op);
    }
}

// ---------- 콜백: 베ンダ 모델 ----------
static void vnd_model_cb(esp_ble_mesh_model_cb_event_t event, esp_ble_mesh_model_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_MODEL_OPERATION_EVT: {
        uint32_t op = param->model_operation.opcode;
        ESP_LOGI(TAG, "VND_OP recv: op=0x%08X len=%d src=0x%04X", op, param->model_operation.length, param->model_operation.ctx->addr);
        // 필요 시 VM_PING/VM_CTRL 처리 추가
    } break;
    case ESP_BLE_MESH_MODEL_PUBLISH_UPDATE_EVT:
        ESP_LOGI(TAG, "PUBLISH_UPDATE");
        break;
    default:
        break;
    }
}

// ---------- 초기화/시작 ----------
Result mesh_init_and_start(node_id_t self_id, bool proxy, bool relay)
{
    ESP_LOGI(TAG, "mesh_init_and_start: self=0x%08X proxy=%d relay=%d", (unsigned)self_id, proxy, relay);

    // 1) BT 스택 준비 (Bluedroid) — adv_scan에서 이미 한다면 중복 호출 피드백 필요
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // 2) 컴포지션 (1개 엘리먼트: CFG_SRV, CFG_CLI, VND)
    gen_uuid_from_nodeid(self_id);

    s_vnd_pub.addr = G_REPORT;           // 기본 퍼브 주소 (그룹)
    s_vnd_pub.update = NULL;             // 주기 퍼브 없음
    s_vnd_pub.msg = &s_vnd_pub_buf;
    s_vnd_pub.ttl = 2;                   // 기본 TTL

    s_vnd_models[0] = (esp_ble_mesh_model_t) {
        .model_id = ESP_BLE_MESH_VND_MODEL_ID(VENDOR_COMPANY_ID, VENDOR_MODEL_ID),
        .vnd.company_id = VENDOR_COMPANY_ID,
        .vnd.model_id   = VENDOR_MODEL_ID,
        .op = vnd_op,
        .keys = NULL,
        .pub  = &s_vnd_pub,
    };

    s_elements[0].location = 0x0000;
    s_elements[0].sig_model_count = 2;
    s_elements[0].vnd_model_count = 1;
    s_elements[0].sig_models = (esp_ble_mesh_model_t[]) { s_cfg_srv_model, s_cfg_cli_model };
    s_elements[0].vnd_models = &s_vnd_models[0];

    s_comp.cid = VENDOR_COMPANY_ID;
    s_comp.pid = 0x0001;
    s_comp.vid = 0x0001;
    s_comp.elem_count = 1;
    s_comp.elements = s_elements;

    // 3) 콜백 등록
    esp_ble_mesh_register_prov_callback(prov_cb);
    esp_ble_mesh_register_config_server_callback(cfg_srv_cb);
    esp_ble_mesh_register_custom_model_callback(vnd_model_cb);

    // 4) 노드 초기화
    esp_ble_mesh_node_t node = {
        .comp = &s_comp,
    };
    ESP_ERROR_CHECK(esp_ble_mesh_init(&s_prov, &node));

    // 5) 프로비저닝 상태에 따라 enable
    if (!esp_ble_mesh_node_is_provisioned()) {
        // Unprovisioned: 어드버타이즈 시작 (PB-ADV/GATT)
        ESP_ERROR_CHECK(esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
        ESP_LOGW(TAG, "Node is unprovisioned. Waiting for provisioning...");
    } else {
        s_is_provisioned = true;
        // 보통 여기서 addr 읽어옴 (esp_ble_mesh_get_primary_element_address)
        s_primary_addr = esp_ble_mesh_get_primary_element_address();
        ESP_LOGI(TAG, "Node already provisioned. Primary addr=0x%04X", s_primary_addr);
    }

    // 6) (데모) 키 주입/바인딩 시도 — 이미 프로비저닝 됐거나, 프로비저닝 직후에도 호출 가능
    ESP_ERROR_CHECK(esp_ble_mesh_add_local_net_key(s_netkey, s_netkey_idx));
    ESP_ERROR_CHECK(esp_ble_mesh_add_local_app_key(s_appkey, s_netkey_idx, s_appkey_idx));
    ESP_ERROR_CHECK(esp_ble_mesh_bind_app_key_to_local_model(s_appkey_idx, s_primary_addr, s_vnd_models[0].model_id, VENDOR_COMPANY_ID));

    // 7) 프록시/릴레이 설정
    esp_ble_mesh_node_enable_features(proxy ? ESP_BLE_MESH_FEATURE_PROXY : 0);
    // 릴레이 특징은 Configuration Server 상태로 제어되지만, 기본 동작은 펌웨어/Config Model로 세팅

    // 8) 기본 구독 그룹 등록 (REPORT 수신/루프백 대비)
    uint16_t subs[1] = { G_REPORT };
    pubsub_config("REPORT", G_REPORT, subs, 1);

    return RES_OK;
}

Result mesh_keys_bind(uint8_t netkey_id, uint8_t appkey_id){
    (void)netkey_id; (void)appkey_id;
    // 이미 init에서 주입 — 외부 제어가 필요하면 구현 확장
    return RES_OK;
}

Result model_bind_vendor(uint8_t appkey_id){
    (void)appkey_id; return RES_OK;
}

Result pubsub_config(const char* model, uint16_t pub, const uint16_t* subs, size_t subs_len){
    (void)model;
    // Publish 주소 설정
    s_vnd_pub.addr = pub;

    // 구독 주소 추가
    for (size_t i=0;i<subs_len;i++){
        esp_ble_mesh_model_subscribe_group_addr(s_primary_addr, &s_vnd_models[0], subs[i]);
    }
    ESP_LOGI(TAG, "pubsub_config: pub=0x%04X subs=%u", pub, (unsigned)subs_len);
    return RES_OK;
}

Result gatt_proxy_set(bool enabled){
    if (enabled) esp_ble_mesh_node_enable_features(ESP_BLE_MESH_FEATURE_PROXY);
    else         esp_ble_mesh_node_disable_features(ESP_BLE_MESH_FEATURE_PROXY);
    ESP_LOGI(TAG, "gatt_proxy_set: %d", enabled);
    return RES_OK;
}

// ---------- 퍼블리시: VM_REPORT ----------
bool mesh_publish_report(const uint8_t* payload, uint16_t len)
{
    if (!s_is_provisioned) {
        ESP_LOGW(TAG, "publish skipped: node not provisioned yet");
        return false;
    }
    if (!payload || len==0) return false;

    // Vendor Opcode + payload
    NET_BUF_SIMPLE_INIT(s_vnd_pub.msg, sizeof(s_vnd_pub_buf_data));
    net_buf_simple_add_u8(s_vnd_pub.msg, (OP_VND(VM_OP_REPORT)      ) & 0xFF);
    net_buf_simple_add_u8(s_vnd_pub.msg, (OP_VND(VM_OP_REPORT) >> 8 ) & 0xFF);
    net_buf_simple_add_u8(s_vnd_pub.msg, (OP_VND(VM_OP_REPORT) >>16 ) & 0xFF);

    if (net_buf_simple_tailroom(s_vnd_pub.msg) < len) {
        ESP_LOGE(TAG, "publish buffer too small");
        return false;
    }
    net_buf_simple_add_mem(s_vnd_pub.msg, payload, len);

    int err = esp_ble_mesh_model_publish(&s_vnd_models[0], s_vnd_pub.addr);
    if (err) {
        ESP_LOGE(TAG, "model_publish err=%d", err);
        return false;
    }
    ESP_LOGI(TAG, "VM_REPORT published: addr=0x%04X len=%u", s_vnd_pub.addr, (unsigned)len);
    return true;
}