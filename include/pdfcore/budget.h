#ifndef PDFCORE_BUDGET_H
#define PDFCORE_BUDGET_H

#include <stddef.h>
#include <stdint.h>

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Opaque budget handle. In the current implementation, pc_budget is a value type
/// (POD struct) defined in transaction.h. This header provides helper functions
/// for budget validation and enforcement.
typedef struct pc_budget pc_budget;

/// Create a budget with the given ceilings. Returns PC_ERR_NONE on success,
/// PC_ERR_ARGUMENT if out_budget is null.
pc_status pc_budget_create(uint32_t max_tiles, uint64_t max_bytes, pc_budget** out_budget);

/// Check if adding one more tile would exceed the tile ceiling.
/// Returns PC_ERR_NONE if within budget, PC_ERR_LIMIT if the ceiling would be
/// exceeded, PC_ERR_ARGUMENT if budget is null.
pc_status pc_budget_check_tiles(const pc_budget* budget, uint32_t current_tiles);

/// Check if adding the given bytes would exceed the byte ceiling.
/// Returns PC_ERR_NONE if within budget, PC_ERR_LIMIT if the ceiling would be
/// exceeded, PC_ERR_ARGUMENT if budget is null.
pc_status pc_budget_check_bytes(const pc_budget* budget, uint64_t current_bytes);

/// Destroy a budget handle. In the current implementation, pc_budget is a
/// value type (POD struct) allocated by the caller, so this is a no-op.
/// Provided for API symmetry.
void pc_budget_destroy(pc_budget* budget);

#ifdef __cplusplus
}
#endif

#endif