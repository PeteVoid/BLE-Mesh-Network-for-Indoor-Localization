#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "proto_v1.h"
#include "adv_frame_v1.h"

static const char *TAG = "ADVSCAN";

static node_id_t g_self_id = 0xB0F001;
static uint16_t  g_adv_seq = 0;
static int8_t    g_txp_dbm = -4;   // 모르면 TXP_SENTINEL_UNKNOWN 사용
static uint8_t   g_flags   = 0;

static uint8_t adv_raw[31];
static uint8_t adv_raw_len = 0;

static TimerHandle_t s_adv_timer;

static void build_adv_raw(void)
{
    adv_frame_v1_t f;
    adv_build_v1(&f, g_self_id, ++g_adv_seq, g_txp_dbm, g_flags);

    // 간단한 Manufacturer Specific Data AD 구조 생성
    // AD type: 0xFF (Manufacturer specific)
    // [len][0xFF][payload...]
    uint8_t *p = adv_raw;
    uint8_t payload_len = sizeof(adv_frame_v1_t);
    *p++ = payload_len + 1; // type 포함 길이
    *p++ = 0xFF;            // AD type
    memcpy(p, &f, sizeof(f)); p += sizeof(f);

    adv_raw_len = 1 + 1 + payload_len;
}

static void adv_start_once(void)
{
    build_adv_raw();

    // Raw ADV data 설정
    esp_ble_gap_stop_advertising();
    esp_ble_gap_config_adv_data_raw(adv_raw, adv_raw_len);
    // 콜백에서 start_advertising가 호출될 수 있으니 여기서는 config만
}

static void adv_tick_cb(TimerHandle_t h)
{
    (void)h;
    adv_start_once();
}

static esp_ble_scan_params_t s_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x0320, // 800*0.625ms ≈ 500ms
    .scan_window            = 0x0064, // 100*0.625ms ≈ 62.5ms
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE,
};

typedef struct {
    rssi_dbm_t rssi;
    adv_frame_v1_t adv;
} obs_sample_t;

static void handle_mfg_adv(const uint8_t *payload, uint8_t len, int rssi)
{
    adv_frame_v1_t frame;
    if (len < sizeof(frame)) return;
    if (!adv_parse_v1(payload, len, &frame)) return;

    // TODO: 1초 윈도 집계 버퍼로 insert
    ESP_LOGD(TAG, "ADV from=0x%08X seq=%u rssi=%d",
             frame.from_id, frame.seq, rssi);
    // 여긴 수신 측정치만 쌓고, 1초 타이머에서 median/p25/p75/n 계산해서 REPORT 생성
}

static void gap_cb(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p)
{
    switch (e) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        esp_ble_gap_start_scanning(0); // 무기한
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        if (p->scan_rst.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT) break;
        const uint8_t *adv = p->scan_rst.ble_adv;
        uint8_t adv_len = p->scan_rst.adv_data_len;
        // AD 파싱: 0xFF 블럭 찾기
        for (int i = 0; i < adv_len; ) {
            uint8_t len = adv[i];
            if (len == 0) break;
            if (i + 1 + len > adv_len) break;
            uint8_t type = adv[i+1];
            if (type == 0xFF) { // Manufacturer Specific
                const uint8_t *pl = &adv[i+2];
                uint8_t pl_len = len-1;
                handle_mfg_adv(pl, pl_len, p->scan_rst.rssi);
            }
            i += 1 + len;
        }
        break;
    }
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        // advertising start
        esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
            .adv_int_min        = 0x00C8, // 200ms
            .adv_int_max        = 0x00C8,
            .adv_type           = ADV_TYPE_NONCONN_IND,
            .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
            .channel_map        = ADV_CHNL_ALL,
            .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        });
        break;
    default: break;
    }
}

void advscan_init_and_start(void)
{
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_cb));
    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&s_scan_params));

    // 광고는 주기 타이머로 매 200ms 페이로드 갱신
    s_adv_timer = xTimerCreate("adv_tick", pdMS_TO_TICKS(200), pdTRUE, NULL, adv_tick_cb);
    xTimerStart(s_adv_timer, 0);
}