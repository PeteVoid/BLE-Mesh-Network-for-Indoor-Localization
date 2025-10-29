#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "role_config.h"
#include "proto_v1.h"
#include "adv_scan.h"
#include "agg_report.h"
#include "mesh_vendor.h"

static const char* TAG = "APP";

static node_id_t self_id(void){
    // TODO: efuse MAC → 해시. 스텁: 고정 ID
    return 0xB0F001u;
}

static void task_main(void* arg){
    (void)arg;
#if (NODE_ROLE == ROLE_ANCHOR)
    advscan_set_observer(agg_on_adv_observed);
    advscan_init_and_start();
    agg_init(self_id(), 1000);
#elif (NODE_ROLE == ROLE_CHILD)
    // TODO: ping_init/ping_publish 스텁
    ESP_LOGW(TAG, "ROLE_CHILD stub: implement adv tx later.");
#elif (NODE_ROLE == ROLE_PROXY)
    ESP_LOGW(TAG, "ROLE_PROXY stub: implement bridge later.");
#endif

    while (1) {
        EpochMs now = (EpochMs)esp_timer_get_time() / 1000ULL;
        agg_periodic_tick(now);
        ESP_LOGI(TAG, "tick role=%d now=%llu", (int)NODE_ROLE, (unsigned long long)now);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void){
    ESP_ERROR_CHECK(nvs_flash_init());

    // 1) Mesh 먼저
    mesh_init_and_start(self_id(), /*proxy*/(NODE_ROLE==ROLE_PROXY), /*relay*/true);

    // 2) 역할 태스크 시작 (Anchor는 스캔+집계 → REPORT → mesh_publish_report)
    xTaskCreate(task_main, "task_main", 4096, NULL, 5, NULL);
}
