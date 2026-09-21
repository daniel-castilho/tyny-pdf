#ifndef PDFCORE_BUDGET_H
#define PDFCORE_BUDGET_H

#include <stddef.h>
#include <stdint.h>

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_budget {
  uint32_t max_rss_mb;
  uint32_t max_tiles;
  uint32_t max_sidecar_bytes;
} pc_budget;

pc_budget pc_budget_default(void);

pc_status pc_budget_check(const pc_budget* b, uint32_t tiles, size_t rss);

#ifdef __cplusplus
}
#endif

#endif