#include "adv_scan.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "ADVSCAN";
static on_adv_observed_cb_t s_cb = 0;

void advscan_set_observer(on_adv_observed_cb_t cb){ s_cb = cb; }

void advscan_init_and_start(void){
    ESP_LOGI(TAG, "start scanning (stub). In real impl, enable GAP scan here.");
    // 데모: 2초마다 가짜 관측 콜백 호출
    // 실제 BLE 스캔 구현 전까지 스텁
}