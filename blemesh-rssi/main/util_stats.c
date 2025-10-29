// main/util_stats.c
#include "util_stats.h"
#include <string.h>

// O(n log n) 단순 정렬 기반(스텁)
static int cmp_i8(const void* a, const void* b){ return (int)(*(const int8_t*)a) - (int)(*(const int8_t*)b); }

void stats_quartiles_i8(const int8_t* vals, size_t n, int8_t* p25, int8_t* med, int8_t* p75){
    if (!vals || n==0){ if(p25)*p25=0; if(med)*med=0; if(p75)*p75=0; return; }
    int8_t buf[256];
    size_t m = (n>256)?256:n;
    memcpy(buf, vals, m);
    qsort(buf, m, sizeof(int8_t), cmp_i8);
    size_t i25 = (m-1) * 25 / 100;
    size_t i50 = (m-1) * 50 / 100;
    size_t i75 = (m-1) * 75 / 100;
    if (p25) *p25 = buf[i25];
    if (med) *med = buf[i50];
    if (p75) *p75 = buf[i75];
}