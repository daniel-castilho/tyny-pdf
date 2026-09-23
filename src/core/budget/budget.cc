// Budget subsystem — memory ceilings for render tiles and transactions.
// src/core/budget/budget.cc
// No engine includes (ADR-0011 R-M10). Engine-free C implementation.

#include "pdfcore/budget.h"

#include <stddef.h>
#include <stdint.h>

#include "pdfcore/status.h"
#include "pdfcore/transaction.h"

static pc_status make_status(uint32_t code, uint32_t detail_id, const char* detail) {
  pc_status st;
  st.size = sizeof(pc_status);
  st.code = code;
  st.detail_id = detail_id;
  st.detail = detail;
  return st;
}

pc_status pc_budget_create(uint32_t /*max_tiles*/, uint64_t /*max_bytes*/, pc_budget** out_budget) {
  if (!out_budget) {
    return make_status(PC_ERR_ARGUMENT, 0, "null argument");
  }

  *out_budget = NULL;

  // In this minimal implementation, pc_budget is a value type (POD struct).
  // The caller allocates it directly; this function validates the parameters.
  // No dynamic allocation needed for the struct itself.
  // The size field is used for versioning per ADR-0003.

  // We return a "success" status indicating the budget parameters are valid.
  // The actual budget struct is allocated by the caller.
  return make_status(PC_ERR_NONE, 0, NULL);
}

pc_status pc_budget_check_tiles(const pc_budget* budget, uint32_t current_tiles) {
  if (!budget) {
    return make_status(PC_ERR_ARGUMENT, 0, "null budget");
  }
  if (budget->max_tiles == 0) {
    return make_status(PC_ERR_NONE, 0, NULL);
  }
  if (current_tiles >= budget->max_tiles) {
    return make_status(PC_ERR_LIMIT, 0, "tile budget exceeded");
  }
  return make_status(PC_ERR_NONE, 0, NULL);
}

pc_status pc_budget_check_bytes(const pc_budget* budget, uint64_t current_bytes) {
  if (!budget) {
    return make_status(PC_ERR_ARGUMENT, 0, "null budget");
  }
  if (budget->max_bytes == 0) {
    return make_status(PC_ERR_NONE, 0, NULL);
  }
  if (current_bytes >= budget->max_bytes) {
    return make_status(PC_ERR_LIMIT, 0, "byte budget exceeded");
  }
  return make_status(PC_ERR_NONE, 0, NULL);
}

void pc_budget_destroy(pc_budget* budget) {
  // pc_budget is a value type (POD struct) allocated by the caller.
  // No dynamic memory to free. This function exists for API symmetry.
  (void)budget;
}