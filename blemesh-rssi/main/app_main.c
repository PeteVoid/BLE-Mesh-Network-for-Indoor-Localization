#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"

#include "mesh_vendor.h"
#include "adv_scan.c"     // 데모 편의상 같은 타겟에 컴파일
#include "agg_report.c"

static const char* TAG = "APP";

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    // BT 컨트롤러 & BLE 스택 초기화 (간단형)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // Mesh (스켈레톤): 실제론 프로비저닝 필요
    mesh_init_and_start(0xB0F001, /*proxy*/true, /*relay*/true);

    // 1초 집계 태스크 시작
    agg_init(0xB0F001);

    // GAP Adv/Scan 시작
    advscan_init_and_start();

    ESP_LOGI(TAG, "system up");
}