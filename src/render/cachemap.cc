// Cachemap implementation - stub for Epic 2 spike
// Real implementation will provide a spatial index for tile management

#include "pdfcore/render.h"
#include <pdfcore/status.h>

pc_status pc_cachemap_create(uint32_t width, uint32_t height, pc_cachemap** out_map) {
    (void)width; (void)height;
    if (!out_map) return { sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null out_map" };
    *out_map = nullptr;
    return { sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "not implemented" };
}

pc_status pc_cachemap_query(pc_cachemap* map, int x, int y, pc_tile** out_tile) {
    (void)map; (void)x; (void)y;
    if (!out_tile) return { sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null out_tile" };
    *out_tile = nullptr;
    return { sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "not implemented" };
}

pc_status pc_cachemap_insert(pc_cachemap* map, int x, int y, pc_tile* tile) {
    (void)map; (void)x; (void)y; (void)tile;
    return { sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "not implemented" };
}

void pc_cachemap_destroy(pc_cachemap* map) {
    (void)map;
}