#include "report_v1.h"

bool report_v1_validate(const report_v1_t* r){
    if (!r) return false;
    if (r->ver != PROTO_VER_V1) return false;
    if (r->entry_count > REPORT_ENTRY_MAX) return false;
    return true;
}