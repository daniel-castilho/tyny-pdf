#ifndef PDFCORE_TRANSACTION_H
#define PDFCORE_TRANSACTION_H

#include <stddef.h>
#include <stdint.h>

#include "doc.h"
#include "geom.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Memory budget for the transaction log (kickoff D-6, R-M8). `size` is the first field so the
/// struct can be versioned (ADR-0003). A field set to 0 removes that ceiling.
typedef struct pc_budget {
  uint32_t size;
  uint32_t max_tiles;  // maximum commands on the undo stack; 0 = unlimited
  uint64_t max_bytes;  // maximum cumulative log estimate; 0 = unlimited
} pc_budget;

/// Kind of IR mutation a command applies (R18.1). Value types only, never an engine handle
/// (R-M4). The enum values form part of the frozen public surface (R-M12).
typedef enum pc_command_type {
  PC_CMD_ADD_ANNOT = 1,
  PC_CMD_MOVE = 2,
  PC_CMD_DELETE = 3,
} pc_command_type;

/// One logged IR mutation (R18.1). `size` first (ADR-0003). `before` is the IR snapshot taken
/// by pc_txn_apply at apply time (zero rect means "element did not exist"); `after` is the
/// target state supplied by the caller. A command must be a value type and never reference an
/// engine object (R-M4).
typedef struct pc_command {
  uint32_t size;
  pc_command_type type;
  char annotation_id[11];  // RFC 4648 base32, 10 chars + NUL (R2.3 id, sidecar mirror)
  pc_rect before;
  pc_rect after;
} pc_command;

typedef struct pc_txn pc_txn;

/// Create a transaction log bound to `doc`. The log mutates the doc IR directly and keeps the
/// undo/redo stacks within `budget` (0 field = unlimited). Returns PC_ERR_NONE and sets
/// *out_txn, or PC_ERR_MEMORY. The caller owns *out_txn and must release it with pc_txn_free.
/// PC_ERR_ARGUMENT for null doc or out_txn.
pc_status pc_txn_create(pc_doc* doc, const pc_budget* budget, pc_txn** out_txn);

/// Apply a command to the IR: fill *cmd->before from the current IR state, mutate the IR to
/// *cmd->after per type, and push the command onto the undo stack. A fresh MOVE or DELETE of an
/// unknown annotation_id answers PC_ERR_ARGUMENT. Pushing a command past budget->max_tiles or
/// budget->max_bytes answers PC_ERR_LIMIT with detail "undo budget exceeded"; the IR is left
/// untouched. PC_ERR_NONE otherwise.
pc_status pc_txn_apply(pc_txn* txn, const pc_command* cmd);

/// Undo the most recent applied command: revert the IR to *cmd->before and move the command to
/// the redo stack. PC_ERR_NONE, or PC_ERR_STATE when the undo stack is empty.
pc_status pc_txn_undo(pc_txn* txn);

/// Redo the most recently undone command: apply *cmd->after again and move the command back to
/// the undo stack. PC_ERR_NONE, or PC_ERR_STATE when the redo stack is empty.
pc_status pc_txn_redo(pc_txn* txn);

/// Free the transaction log and its stacks. Does not free `doc` and does not touch the IR.
void pc_txn_free(pc_txn* txn);

/// Hex-encoded SHA-256 of the IR: page count, page boxes, then each annotation (id + rect),
/// in IR order. Value types only, no engine reach (R-M4). Returns PC_ERR_ARGUMENT for a null
/// doc or out_hex; on error out_hex[0] is set to '\0'.
pc_status pc_doc_hash(const pc_doc* doc, char out_hex[65]);

/// Serialize the transaction log (undo + redo stacks + budget) to canonical JSON.
/// The caller owns *out_json and must free it with free(). Returns PC_ERR_NONE,
/// PC_ERR_MEMORY, or PC_ERR_ARGUMENT.
pc_status pc_txn_to_json(const pc_txn* txn, char** out_json);

/// Deserialize a canonical JSON transaction log and recreate the txn object bound to `doc`.
/// The caller owns *out_txn and must free it with pc_txn_free. Returns PC_ERR_NONE,
/// PC_ERR_ARGUMENT (malformed JSON or type mismatch), or PC_ERR_MEMORY.
pc_status pc_txn_from_json(const char* json, pc_doc* doc, pc_txn** out_txn);

#ifdef __cplusplus
}
#endif

#endif