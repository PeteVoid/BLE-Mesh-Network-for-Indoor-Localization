// main/util_ringbuf.h  (간단 링버퍼 헤더 예시)
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int8_t  data[256];
    uint8_t head;
    uint8_t len;
} rssi_ringbuf_t;

static inline void rb_init(rssi_ringbuf_t* rb){ rb->head=0; rb->len=0; }
static inline bool rb_push(rssi_ringbuf_t* rb, int8_t v){
    if (rb->len < sizeof(rb->data)) {
        rb->data[(rb->head + rb->len) % sizeof(rb->data)] = v;
        rb->len++;
        return true;
    }
    rb->data[rb->head] = v;
    rb->head = (rb->head + 1) % sizeof(rb->data);
    return true;
}
static inline uint8_t rb_len(const rssi_ringbuf_t* rb){ return rb->len; }