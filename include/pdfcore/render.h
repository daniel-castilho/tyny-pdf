#ifndef PDFCORE_RENDER_H
#define PDFCORE_RENDER_H

#include <stddef.h>
#include <stdint.h>

#include "doc.h"
#include "page.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum pc_render_backend {
  PC_RENDER_BACKEND_NULL = 0,
  PC_RENDER_BACKEND_MUPDF = 1,
} pc_render_backend;

// Swapchain (story 5.2; the ABI is pinned by tests/unit/test_window_swapchain.cc).
typedef struct pc_swapchain pc_swapchain;

/// PC_ERR_NONE or PC_ERR_MEMORY.
pc_status pc_swapchain_create(pc_swapchain** out_swapchain);
/// PC_ERR_NONE or PC_ERR_STATE when the swapchain has not been created/recreated.
pc_status pc_swapchain_present(pc_swapchain* swapchain);
void pc_swapchain_destroy(pc_swapchain* swapchain);

// Tile cache (story 5.3, pinned by tests/unit/test_tiles*.cc and driven with
// content by the story 1.5 viewer loop).
typedef struct pc_tile_cache pc_tile_cache;
typedef struct pc_tile pc_tile;

pc_status pc_tile_cache_create(uint32_t max_tiles, pc_tile_cache** out_cache);
pc_status pc_tile_cache_get(pc_tile_cache* cache, uint32_t x, uint32_t y, pc_tile** out_tile);
pc_status pc_tile_cache_put(pc_tile_cache* cache, uint32_t x, uint32_t y, pc_tile* tile);
void pc_tile_cache_destroy(pc_tile_cache* cache);

// Cachemap (story 5.3, pinned by tests/unit/test_cachemap*.cc).
typedef struct pc_cachemap pc_cachemap;

pc_status pc_cachemap_create(uint32_t width, uint32_t height, pc_cachemap** out_map);
pc_status pc_cachemap_query(pc_cachemap* map, int x, int y, pc_tile** out_tile);
pc_status pc_cachemap_insert(pc_cachemap* map, int x, int y, pc_tile* tile);
void pc_cachemap_destroy(pc_cachemap* map);

// Render context (story 5.2).
typedef struct pc_render_context pc_render_context;

pc_status pc_render_context_create(pc_render_backend backend, pc_render_context** out_ctx);
void pc_render_context_destroy(pc_render_context* ctx);

pc_status pc_doc_open_with_ctx(pc_render_context* ctx, const char* path, const char* password,
                               pc_doc** out_doc);

#ifdef __cplusplus
}
#endif

#endif