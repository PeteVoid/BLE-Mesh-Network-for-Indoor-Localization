# blemesh-rssi/main/CMakeLists.txt
idf_component_register(
    SRCS
        app_main.c
        mesh_vendor.c
        report_v1_cbor.c
        util_stats.c
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash tinycbor
)