// Swapchain implementation - stub for Epic 2 spike
// Real implementation will create DComp/D3D11 swapchain and present frames

#include "pdfcore/render.h"
#include <pdfcore/status.h>

pc_status pc_swapchain_create(pc_swapchain** out_swapchain) {
    if (!out_swapchain) return { sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null out_swapchain" };
    *out_swapchain = nullptr;
    return { sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "swapchain not implemented" };
}

pc_status pc_swapchain_present(pc_swapchain* swapchain) {
    (void)swapchain;
    return { sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "not implemented" };
}

void pc_swapchain_destroy(pc_swapchain* swapchain) {
    (void)swapchain;
}