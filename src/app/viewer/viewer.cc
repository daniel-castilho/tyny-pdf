// Platform-neutral viewer loop (story 1.5, R30.1).
// src/app/viewer/viewer.cc
// Composition is curated here: assert() is never fed document input (a
// hostile PDF yields a reported pc_status, ADR-0003), engines are reached
// only through the backend vtable, and no engine buffer survives past the
// pixmap_free (R-M6).

#include "viewer.h"

#include <cstdlib>
#include <cstring>

namespace tynypdf {
namespace viewer {

namespace {

constexpr uint32_t kTilePx = 256;  // the tile cache's fixed tile size (R27.x)

// clip for one tile in PDF user-space points at the given dpi (the backend
// scales params.clip by dpi/72 and rounds, so 256 px at 72 dpi == 256 pt).
void tile_clip(pc_render_params* params, uint32_t page, int32_t col, int32_t row, uint32_t dpi) {
  (void)page;
  uint32_t span = dpi == 0 ? 1 : (kTilePx * 72) / dpi;
  uint32_t x0 = (uint32_t)col * span;
  uint32_t y0 = (uint32_t)row * span;
  params->clip.x0 = x0;
  params->clip.y0 = y0;
  params->clip.x1 = x0 + span;
  params->clip.y1 = y0 + span;
}

pc_status viewer_reset_tiles(viewer_state* st) {
  if (st->cache) {
    pc_tile_cache_destroy(st->cache);
    st->cache = nullptr;
  }
  if (st->map) {
    pc_cachemap_destroy(st->map);
    st->map = nullptr;
  }
  st->cached_tiles = 0;
  st->page_gen++;  // invalidates every blit slot (payloads change content)

  pc_status s = pc_tile_cache_create(st->max_tiles, &st->cache);
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  s = pc_cachemap_create(16, 12, &st->map);
  if (s.code != PC_ERR_NONE) {
    pc_tile_cache_destroy(st->cache);
    st->cache = nullptr;
    return s;
  }
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

}  // namespace

pc_status viewer_open(viewer_state* st, const pc_backend_api* api, const char* path, uint32_t dpi,
                      uint32_t max_tiles, uint64_t max_bytes) {
  if (!st || !api || !path) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  *st = viewer_state{};
  st->api = api;
  st->dpi = dpi ? dpi : 72;
  st->max_tiles = max_tiles;
  st->max_bytes = max_bytes;
  st->active_page = UINT32_MAX;

  pc_status s = api->doc_open(path, nullptr, &st->doc);
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  st->page_count = api->doc_page_count(st->doc);

  s = viewer_reset_tiles(st);
  if (s.code != PC_ERR_NONE) {
    api->doc_close(st->doc);
    st->doc = nullptr;
    return s;
  }

  st->budget.size = sizeof(pc_budget);
  st->budget.max_tiles = max_tiles;
  st->budget.max_bytes = max_bytes;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

pc_status viewer_demand(viewer_state* st, uint32_t page, int32_t col0, int32_t row0, uint32_t cols,
                        uint32_t rows) {
  if (!st || !st->api || !st->doc || !st->cache || !st->map) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null state"};
  }
  if (page >= st->page_count) {
    return {sizeof(pc_status), PC_ERR_RANGE, 0, "page index out of range"};
  }
  if (page != st->active_page) {
    pc_status s = viewer_reset_tiles(st);
    if (s.code != PC_ERR_NONE) {
      return s;
    }
    st->active_page = page;
  }

  void* backend_page = nullptr;
  pc_status s = st->api->page_get(st->doc, page, &backend_page);
  if (s.code != PC_ERR_NONE || !backend_page) {
    if (s.code != PC_ERR_NONE) {
      return s;
    }
    const pc_status err = {sizeof(pc_status), PC_ERR_BACKEND, 0, "page_get returned null"};
    return err;
  }

  for (uint32_t r = 0; r < rows; ++r) {
    for (uint32_t c = 0; c < cols; ++c) {
      int32_t col = col0 + (int32_t)c;
      int32_t row = row0 + (int32_t)r;

      pc_tile* hit = nullptr;
      pc_tile_cache_get(st->cache, (uint32_t)col, (uint32_t)row, &hit);
      const pc_tile_mirror* hm = reinterpret_cast<const pc_tile_mirror*>(hit);
      if (hit && hm->data) {
        continue;  // already cached for the current page
      }

      if (pc_budget_check_tiles(&st->budget, st->cached_tiles).code == PC_ERR_LIMIT) {
        continue;  // ceiling exhausted: keep the strip partial, report memory, not a stall
      }

      pc_render_params params = {};
      params.dpi = st->dpi;
      params.render_annots = 0;
      params.render_text = 1;
      tile_clip(&params, page, col, row, st->dpi);

      pc_pixmap pixmap = {};
      s = st->api->page_render(backend_page, &params, &pixmap);
      if (s.code != PC_ERR_NONE || !pixmap.data) {
        st->api->page_free(backend_page);
        if (s.code == PC_ERR_NONE) {
          return {sizeof(pc_status), PC_ERR_BACKEND, 0, "page_render returned null"};
        }
        return s;
      }

      // Marshal a malloc'd payload with the dims header; the cache owns and
      // may free() it. Engine memory is copied out first (R-M6).
      size_t payload_bytes = sizeof(tile_payload) + (size_t)pixmap.height * pixmap.stride;
      void* buffer = std::malloc(payload_bytes);
      if (!buffer) {
        st->api->pixmap_free(&pixmap);
        st->api->page_free(backend_page);
        return {sizeof(pc_status), PC_ERR_MEMORY, 0, "tile payload allocation failed"};
      }
      tile_payload* payload = static_cast<tile_payload*>(buffer);
      payload->width = pixmap.width;
      payload->height = pixmap.height;
      payload->stride = pixmap.stride;
      payload->page_gen = st->page_gen;
      uint8_t* dst = static_cast<uint8_t*>(buffer) + sizeof(tile_payload);
      std::memcpy(dst, pixmap.data, (size_t)pixmap.height * pixmap.stride);
      st->api->pixmap_free(&pixmap);

      pc_tile_mirror* tile = static_cast<pc_tile_mirror*>(std::malloc(sizeof(pc_tile_mirror)));
      if (!tile) {
        std::free(buffer);
        st->api->page_free(backend_page);
        return {sizeof(pc_status), PC_ERR_MEMORY, 0, "tile allocation failed"};
      }
      tile->size_bytes = payload_bytes;
      tile->data = buffer;

      s = pc_tile_cache_put(st->cache, (uint32_t)col, (uint32_t)row, (pc_tile*)tile);
      if (s.code != PC_ERR_NONE) {
        st->api->page_free(backend_page);
        return s;
      }
      s = pc_cachemap_insert(st->map, col, row, (pc_tile*)tile);
      if (s.code != PC_ERR_NONE) {
        st->api->page_free(backend_page);
        return s;
      }
      st->cached_tiles++;
      st->tiles_rendered++;
    }
  }

  st->api->page_free(backend_page);
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

pc_status viewer_draw(viewer_state* st, uint32_t page, int32_t col0, int32_t row0, uint32_t cols,
                      uint32_t rows, viewer_draw_fn draw, void* user_data) {
  (void)page;
  if (!st || !draw) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  for (uint32_t r = 0; r < rows; ++r) {
    for (uint32_t c = 0; c < cols; ++c) {
      int32_t col = col0 + (int32_t)c;
      int32_t row = row0 + (int32_t)r;

      pc_tile* tile = nullptr;
      pc_cachemap_query(st->map, col, row, &tile);
      const pc_tile_mirror* m = reinterpret_cast<const pc_tile_mirror*>(tile);
      if (!tile || !m->data) {
        continue;  // not in the visible rect: skip rather than re-render
      }
      const tile_payload* payload = static_cast<const tile_payload*>(m->data);
      pc_status s = draw(payload, col, row, user_data);
      if (s.code != PC_ERR_NONE) {
        return s;
      }
    }
  }
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

void viewer_close(viewer_state* st) {
  if (!st) {
    return;
  }
  if (st->map) {
    pc_cachemap_destroy(st->map);
    st->map = nullptr;
  }
  if (st->cache) {
    pc_tile_cache_destroy(st->cache);
    st->cache = nullptr;
  }
  if (st->api && st->doc) {
    st->api->doc_close(st->doc);
    st->doc = nullptr;
  }
}

// Story 6.1 (R32.1, R32.4): click handler wiring device-space click to hit-test.
pc_status viewer_on_click(viewer_state* st, uint32_t page, int x, int y, int modifiers) {
  (void)modifiers;  // unused in minimal viewer; reserved for future extensions
  if (!st || !st->api || !st->doc || page >= st->page_count) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null state"};
  }

  // Get page crop box
  pc_page_box box = {};
  pc_status s = st->api->page_get_box(st->doc, page, &box);
  if (s.code != PC_ERR_NONE) {
    return s;
  }

  // Get backend page
  void* backend_page = nullptr;
  s = st->api->page_get(st->doc, page, &backend_page);
  if (s.code != PC_ERR_NONE || !backend_page) {
    if (s.code != PC_ERR_NONE)
      return s;
    return {sizeof(pc_status), PC_ERR_BACKEND, 0, "page_get returned null"};
  }

  // Convert device point to page coordinates
  pc_point device_pt = {static_cast<double>(x), static_cast<double>(y)};
  pc_selection_result result = {};

  s = pc_selection_hit_test(st->api, backend_page, &device_pt, static_cast<float>(st->dpi),
                            &box.cropbox, &result);
  st->api->page_free(backend_page);

  if (s.code == PC_ERR_NONE) {
    st->selection = result;
    st->has_selection = true;
    return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  }

  st->has_selection = false;
  return s;
}

}  // namespace viewer
}  // namespace tynypdf