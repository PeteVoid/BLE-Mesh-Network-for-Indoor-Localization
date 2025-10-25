// main/app_main.c
#include "esp_log.h"
#include "nvs_flash.h"
#include "proto_v1.h"
#include "report_v1.h"
#include "report_v1_cbor.h"
#include "mesh_vendor.h"
#include "util_ringbuf.h"

static const char* TAG = "APP";

// 노드 식별자 (예: MAC 해시)
static node_id_t self_id(void){
    // TODO: efuse MAC 읽어 32비트 해시
    return 0xB0F001;
}

static uint16_t s_report_seq = 0;
static uint16_t next_report_seq(void){ return ++s_report_seq; }

// 샘플: 1초 타이머/태스크에서 entries 채우기
static void build_and_send_example_report(void) {
    report_v1_t rpt = {
        .ver = PROTO_VER_V1,
        .reporter_id = self_id(),
        .window_ms = 1000,
        .report_seq = next_report_seq(),
        .entry_count = 0
    };

    // 예시로 2개 엔트리 추가
    report_entry_v1_t e1 = {
        .from_id=0xA001, .to_id=self_id(), .kind=OBS_KIND_ADV, .seq=237,
        .rssi_med=-67, .p25=-70, .p75=-65, .n=5, .txp_dbm=-4, .recv_ttl=0
    };
    rpt.entries[rpt.entry_count++] = e1;

    report_entry_v1_t e2 = {
        .from_id=self_id(), .to_id=0xP0XY01, .kind=OBS_KIND_MESH_ADV, .seq=0,
        .rssi_med=-58, .p25=0, .p75=0, .n=3, .txp_dbm=TXP_SENTINEL_UNKNOWN, .recv_ttl=1
    };
    rpt.entries[rpt.entry_count++] = e2;

    uint8_t buf[256]; size_t out=0;
    if (!report_v1_cbor_encode(&rpt, buf, sizeof(buf), &out)) {
        ESP_LOGE(TAG, "CBOR encode failed");
        return;
    }
    if (!mesh_publish_report(buf, (uint16_t)out)) {
        ESP_LOGE(TAG, "Mesh publish failed");
    } else {
        ESP_LOGI(TAG, "Report sent: %d bytes, entries=%d, seq=%u", (int)out, rpt.entry_count, rpt.report_seq);
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    // TODO: BLE init (controller/BT stack enable) – IDF bt controller init 시퀀스
    mesh_init_and_start(self_id(), /*proxy*/true, /*relay*/true);

    // 실제 구현: GAP 스캔 Task, ADV 송신 Task, 집계 Task 분리
    while (1) {
        build_and_send_example_report();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}