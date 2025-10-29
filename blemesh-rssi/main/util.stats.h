#pragma once
#include <stdint.h>
#include <stddef.h>

// 매우 단순한 중앙값/사분위 스텁 (정확도보다 틀 확보용)
// 실제 구현은 나중에 교체
void stats_quartiles_i8(const int8_t* vals, size_t n, int8_t* p25, int8_t* med, int8_t* p75);