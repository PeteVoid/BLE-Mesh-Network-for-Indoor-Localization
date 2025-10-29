#include "agg_report.h"
#include "util_ringbuf.h"
#include "util_stats.h"
#include "report_v1_cbor.h"
#include "mesh_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "AGG";
static node_id_t s_reporter = 0;
static uint16_t  s_window_ms = 1000;
static uint16_t  s_report_seq = 0;

typedef struct {
    node_id_t child;
    rssi_ringbuf_t rb;
    uint32_t last_seq; // 스텁: 아직 seq 추적 안함
} child_buf_t;

static child_buf_t s_cb; // 단일 Child 스텁 (확장 예정)

void agg_init(node_id_t self_reporter_id, uint16_t window_ms){
    s_reporter = self_reporter_id;
    s_window_ms = window_ms;
    rb_init(&s_cb.rb);
    s_cb.child = 0;
    s_cb.last_seq = 0;
    ESP_LOGI(TAG, "agg_init: reporter=0x%08x window=%u", (unsigned)s_reporter, s_window_ms);
}

void agg_on_adv_observed(node_id_t from_id, RssiDbm rssi_dbm, EpochMs ts_ms){
    (void)ts_ms;
    if (s_cb.child == 0) s_cb.child = from_id;
    rb_push(&s_cb.rb, rssi_dbm);
    ESP_LOGI(TAG, "obs: from=0x%08x rssi=%d len=%u", (unsigned)from_id, (int)rssi_dbm, rb_len(&s_cb.rb));
}

static void flush_and_publish(void){
    if (rb_len(&s_cb.rb) == 0 || s_cb.child == 0) return;

    int8_t p25=0, med=0, p75=0;
    stats_quartiles_i8(s_cb.rb.data, rb_len(&s_cb.rb), &p25, &med, &p75);

    report_v1_t rpt = {
        .ver = PROTO_VER_V1,
        .reporter_id = s_reporter,
        .window_ms = s_window_ms,
        .report_seq = ++s_report_seq,
        .entry_count = 1
    };
    rpt.entries[0].from_id = s_cb.child;
    rpt.entries[0].to_id   = s_reporter;
    rpt.entries[0].kind    = OBS_KIND_ADV;
    rpt.entries[0].seq     = 0; // 스텁
    rpt.entries[0].rssi_med= med;
    rpt.entries[0].p25     = p25;
    rpt.entries[0].p75     = p75;
    rpt.entries[0].n       = rb_len(&s_cb.rb);
    rpt.entries[0].txp_dbm = TXP_SENTINEL_UNKNOWN;
    rpt.entries[0].recv_ttl= 0;

    uint8_t buf[256]; size_t out=0;
    if (!report_v1_cbor_encode(&rpt, buf, sizeof(buf), &out)) {
        ESP_LOGE(TAG, "encode fail");
        return;
    }
    if (mesh_publish_report(buf, (uint16_t)out)) {
        ESP_LOGI(TAG, "report published: seq=%u bytes=%u n=%u med=%d", s_report_seq, (unsigned)out, rpt.entries[0].n, rpt.entries[0].rssi_med);
    }
    // 간단 초기화
    rb_init(&s_cb.rb);
}

void agg_periodic_tick(EpochMs now_ms){
    static EpochMs last = 0;
    if (last == 0) last = now_ms;
    if ((now_ms - last) >= s_window_ms) {
        flush_and_publish();
        last = now_ms;
    }
}