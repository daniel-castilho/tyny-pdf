#include "pdfcore/budget.h"

pc_budget pc_budget_default(void) {
  pc_budget b = {
    .max_rss_mb = 250,
    .max_tiles = 64,
    .max_sidecar_bytes = 4 * 1024 * 1024,
  };
  return b;
}

pc_status pc_budget_check(const pc_budget* b, uint32_t tiles, size_t rss) {
  if (!b) {
    pc_status s = {};
    s.size = sizeof(pc_status);
    s.code = PC_ERR_ARGUMENT;
    s.detail = "null budget";
    return s;
  }
  if (tiles > b->max_tiles) {
    pc_status s = {};
    s.size = sizeof(pc_status);
    s.code = PC_ERR_LIMIT;
    s.detail = "tile budget exceeded";
    return s;
  }
  if (rss > (size_t)b->max_rss_mb * 1024 * 1024) {
    pc_status s = {};
    s.size = sizeof(pc_status);
    s.code = PC_ERR_LIMIT;
    s.detail = "RSS budget exceeded";
    return s;
  }
  pc_status s = {};
  s.size = sizeof(pc_status);
  s.code = PC_ERR_NONE;
  return s;
}