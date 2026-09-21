#ifndef PDFCORE_TRANSACTION_H
#define PDFCORE_TRANSACTION_H

#include <stddef.h>
#include <stdint.h>

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_txn pc_txn;
typedef struct pc_document pc_document;

/// Transaction operation types
typedef enum pc_txn_op_type {
  PC_TXN_OP_NONE = 0,
  PC_TXN_OP_ANNOTATION_ADD,
  PC_TXN_OP_ANNOTATION_EDIT,
  PC_TXN_OP_ANNOTATION_DELETE,
  PC_TXN_OP_FORM_FIELD_CHANGE,
  PC_TXN_OP_REDACTION_ADD,
  PC_TXN_OP_REDACTION_REMOVE,
  PC_TXN_OP_TEXT_EDIT,
  PC_TXN_OP_CUSTOM,
} pc_txn_op_type;

/// A single operation in the transaction log
typedef struct pc_txn_op {
  pc_txn_op_type type;
  char* target_id;        /// ID of affected object (annotation ID, field name, etc.)
  char* before_json;      /// State before operation (JSON)
  char* after_json;       /// State after operation (JSON)
  char* custom_data;      /// For PC_TXN_OP_CUSTOM
} pc_txn_op;

/// Transaction handle
typedef struct pc_txn pc_txn;

/// Create a new transaction context for a document
/// Returns PC_ERR_NONE on success, out_txn must be freed with pc_txn_free
pc_status pc_txn_create(pc_document* doc, pc_txn** out_txn);

/// Free a transaction context
void pc_txn_free(pc_txn* txn);

/// Begin a new transaction (start recording operations)
/// Returns PC_ERR_NONE on success
pc_status pc_txn_begin(pc_txn* txn);

/// Commit the current transaction (make changes permanent)
/// Returns PC_ERR_NONE on success
pc_status pc_txn_commit(pc_txn* txn);

/// Rollback the current transaction (discard changes)
/// Returns PC_ERR_NONE on success
pc_status pc_txn_rollback(pc_txn* txn);

/// Record an operation in the current transaction
/// Returns PC_ERR_NONE on success
pc_status pc_txn_record_op(pc_txn* txn, const pc_txn_op* op);

/// Undo the last committed transaction
/// Returns PC_ERR_NONE on success, PC_ERR_STATE if nothing to undo
pc_status pc_txn_undo(pc_txn* txn);

/// Redo the last undone transaction
/// Returns PC_ERR_NONE on success, PC_ERR_STATE if nothing to redo
pc_status pc_txn_redo(pc_txn* txn);

/// Get number of transactions available for undo
size_t pc_txn_undo_count(pc_txn* txn);

/// Get number of transactions available for redo
size_t pc_txn_redo_count(pc_txn* txn);

/// Clear the transaction log
/// Returns PC_ERR_NONE on success
pc_status pc_txn_clear_log(pc_txn* txn);

/// Serialize transaction log to canonical JSON (for CLI replay)
/// Returns PC_ERR_NONE on success, out_json must be freed by caller
pc_status pc_txn_log_to_json(pc_txn* txn, char** out_json);

/// Replay a transaction log from JSON
/// Returns PC_ERR_NONE on success
pc_status pc_txn_replay_from_json(pc_txn* txn, const char* json);

/// Check if transaction log is empty
int pc_txn_is_log_empty(pc_txn* txn);

/// Maximum number of operations per transaction (0 = unlimited)
/// Default: 10000
void pc_txn_set_max_ops(pc_txn* txn, size_t max_ops);

/// Get current max operations limit
size_t pc_txn_get_max_ops(pc_txn* txn);

#ifdef __cplusplus
}
#endif

#endif