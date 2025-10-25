#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "report_v1.h"
#include "report_v1_cbor.h"
#include "mesh_vendor.h"

static const char* TAG = "AGG";

typedef struct {
    node_id_t from_id;
    uint16_t  seq;
    rssi_dbm_t samples[16];
    uint8_t   n;
    int8_t    txp_dbm;
} win_item_t;

#define MAX_ITEMS 32
static win_item_t s_items[MAX_ITEMS];
static uint8_t    s_items_cnt = 0;

static node_id_t g_self_id_reporter = 0xB0F001;
static uint16_t  s_report_seq = 0;

static void win_add(node_id_t from, uint16_t seq, rssi_dbm_t rssi, int8_t txp_dbm)
{
    for (int i=0;i<s_items_cnt;i++){
        if (s_items[i].from_id==from && s_items[i].seq==seq) {
            if (s_items[i].n<16) s_items[i].samples[s_items[i].n++] = rssi;
            return;
        }
    }
    if (s_items_cnt < MAX_ITEMS) {
        s_items[s_items_cnt] = (win_item_t){ .from_id=from, .seq=seq, .n=1, .txp_dbm=txp_dbm };
        s_items[s_items_cnt].samples[0]=rssi;
        s_items_cnt++;
    } else {
        ESP_LOGW(TAG, "win overflow");
    }
}

static int cmp_i8(const void* a,const void* b){ return (*(int8_t*)a)-(*(int8_t*)b); }

static void flush_and_publish(void)
{
    report_v1_t rpt = {
        .ver = PROTO_VER_V1,
        .reporter_id = g_self_id_reporter,
        .window_ms = 1000,
        .report_seq = ++s_report_seq,
        .entry_count = 0
    };

    for (int i=0;i<s_items_cnt && rpt.entry_count<REPORT_V1_MAX_ENTRIES;i++){
        win_item_t *it = &s_items[i];
        qsort(it->samples, it->n, sizeof(it->samples[0]), cmp_i8);
        int n = it->n;
        int i25=(n-1)*0.25, i50=(n-1)*0.5, i75=(n-1)*0.75;

        report_entry_v1_t e = {
            .from_id   = it->from_id,
            .to_id     = g_self_id_reporter,
            .kind      = OBS_KIND_ADV,
            .seq       = it->seq,
            .rssi_med  = it->samples[i50],
            .p25       = it->samples[i25],
            .p75       = it->samples[i75],
            .n         = it->n,
            .txp_dbm   = it->txp_dbm,
            .recv_ttl  = 0
        };
        rpt.entries[rpt.entry_count++] = e;
    }

    uint8_t buf[256]; size_t out=0;
    if (!report_v1_cbor_encode(&rpt, buf, sizeof(buf), &out)) {
        ESP_LOGE(TAG, "CBOR encode fail");
        goto clear;
    }
    if (!mesh_publish_report(buf, (uint16_t)out)) {
        ESP_LOGE(TAG, "mesh publish fail");
    } else {
        ESP_LOGI(TAG, "report sent: entries=%d seq=%u", rpt.entry_count, rpt.report_seq);
    }

clear:
    s_items_cnt = 0; // 창 초기화
}

// 외부에서 호출: ADV 스캔 콜백에서 관측 추가
void agg_on_adv_observed(node_id_t from_id, uint16_t seq, int8_t rssi, int8_t txp_dbm)
{
    win_add(from_id, seq, rssi, txp_dbm);
}

static void agg_task(void* arg)
{
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000);
    while (1) {
        vTaskDelayUntil(&last, period);
        flush_and_publish();
    }
}

void agg_init(node_id_t self_id_reporter)
{
    g_self_id_reporter = self_id_reporter;
    xTaskCreate(agg_task, "agg_task", 4096, NULL, 5, NULL);
}