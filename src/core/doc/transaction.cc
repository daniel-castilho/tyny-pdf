#include "pdfcore/transaction.h"
#include "pdfcore/status.h"

#include <stdlib.h>
#include <string.h>
#include <cstdio>

struct pc_txn {
  pc_document* document;
  pc_txn_op** undo_stack;
  size_t undo_count;
  size_t undo_capacity;
  pc_txn_op** redo_stack;
  size_t redo_count;
  size_t redo_capacity;
  size_t max_ops;
  int in_transaction;
  pc_txn_op* current_op;
};

static pc_status make_status(uint32_t code, const char* detail) {
  pc_status s = {};
  s.size = sizeof(pc_status);
  s.code = code;
  s.detail = detail;
  return s;
}

static void txn_op_free(pc_txn_op* op) {
  if (!op) return;
  free(op->target_id);
  free(op->before_json);
  free(op->after_json);
  free(op->custom_data);
  free(op);
}

static int ensure_capacity(pc_txn_op*** stack, size_t* count, size_t* capacity) {
  if (*count >= *capacity) {
    size_t new_cap = *capacity ? *capacity * 2 : 16;
    pc_txn_op** new_stack = (pc_txn_op**)realloc(*stack, new_cap * sizeof(pc_txn_op*));
    if (!new_stack) return 0;
    *stack = new_stack;
    *capacity = new_cap;
  }
  return 1;
}

pc_status pc_txn_create(pc_document* doc, pc_txn** out_txn) {
  if (!doc || !out_txn) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }

  pc_txn* txn = (pc_txn*)calloc(1, sizeof(pc_txn));
  if (!txn) {
    return make_status(PC_ERR_MEMORY, "OOM");
  }

  txn->document = doc;
  txn->max_ops = 10000;
  txn->in_transaction = 0;

  *out_txn = txn;
  return make_status(PC_ERR_NONE, nullptr);
}

void pc_txn_free(pc_txn* txn) {
  if (!txn) return;

  for (size_t i = 0; i < txn->undo_count; ++i) {
    txn_op_free(txn->undo_stack[i]);
  }
  free(txn->undo_stack);

  for (size_t i = 0; i < txn->redo_count; ++i) {
    txn_op_free(txn->redo_stack[i]);
  }
  free(txn->redo_stack);

  free(txn);
}

pc_status pc_txn_begin(pc_txn* txn) {
  if (!txn) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }
  if (txn->in_transaction) {
    return make_status(PC_ERR_STATE, "transaction already in progress");
  }
  txn->in_transaction = 1;
  txn->current_op = (pc_txn_op*)calloc(1, sizeof(pc_txn_op));
  if (!txn->current_op) {
    return make_status(PC_ERR_MEMORY, "OOM");
  }
  return make_status(PC_ERR_NONE, nullptr);
}

pc_status pc_txn_commit(pc_txn* txn) {
  if (!txn) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }
  if (!txn->in_transaction) {
    return make_status(PC_ERR_STATE, "no transaction in progress");
  }
  if (!txn->current_op) {
    return make_status(PC_ERR_STATE, "no current operation");
  }

  // Check max operations limit
  if (txn->max_ops > 0 && txn->undo_count >= txn->max_ops) {
    return make_status(PC_ERR_LIMIT, "max operations reached");
  }

  if (!ensure_capacity(&txn->undo_stack, &txn->undo_count, &txn->undo_capacity)) {
    return make_status(PC_ERR_MEMORY, "OOM");
  }

  txn->undo_stack[txn->undo_count++] = txn->current_op;
  txn->current_op = nullptr;
  txn->in_transaction = 0;

  // Clear redo stack on new commit
  for (size_t i = 0; i < txn->redo_count; ++i) {
    txn_op_free(txn->redo_stack[i]);
  }
  txn->redo_count = 0;

  return make_status(PC_ERR_NONE, nullptr);
}

pc_status pc_txn_rollback(pc_txn* txn) {
  if (!txn) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }
  if (!txn->in_transaction) {
    return make_status(PC_ERR_STATE, "no transaction in progress");
  }
  if (txn->current_op) {
    txn_op_free(txn->current_op);
    txn->current_op = nullptr;
  }
  txn->in_transaction = 0;
  return make_status(PC_ERR_NONE, nullptr);
}

pc_status pc_txn_record_op(pc_txn* txn, const pc_txn_op* op) {
  if (!txn || !op) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }
  if (!txn->in_transaction) {
    return make_status(PC_ERR_STATE, "no transaction in progress");
  }
  if (!txn->current_op) {
    return make_status(PC_ERR_STATE, "no current operation");
  }

  // Append to current operation (simplified: single op per transaction)
  // In a real implementation, this would append to a list within current_op
  *txn->current_op = *op;
  txn->current_op->target_id = op->target_id ? strdup(op->target_id) : nullptr;
  txn->current_op->before_json = op->before_json ? strdup(op->before_json) : nullptr;
  txn->current_op->after_json = op->after_json ? strdup(op->after_json) : nullptr;
  txn->current_op->custom_data = op->custom_data ? strdup(op->custom_data) : nullptr;

  return make_status(PC_ERR_NONE, nullptr);
}

pc_status pc_txn_undo(pc_txn* txn) {
  if (!txn) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }
  if (txn->undo_count == 0) {
    return make_status(PC_ERR_STATE, "nothing to undo");
  }

  // Move last undo to redo
  pc_txn_op* op = txn->undo_stack[--txn->undo_count];
  if (!ensure_capacity(&txn->redo_stack, &txn->redo_count, &txn->redo_capacity)) {
    txn->undo_stack[txn->undo_count++] = op;
    return make_status(PC_ERR_MEMORY, "OOM");
  }
  txn->redo_stack[txn->redo_count++] = op;

  // TODO: Apply reverse operation to document IR

  return make_status(PC_ERR_NONE, nullptr);
}

pc_status pc_txn_redo(pc_txn* txn) {
  if (!txn) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }
  if (txn->redo_count == 0) {
    return make_status(PC_ERR_STATE, "nothing to redo");
  }

  // Move last redo to undo
  pc_txn_op* op = txn->redo_stack[--txn->redo_count];
  if (!ensure_capacity(&txn->undo_stack, &txn->undo_count, &txn->undo_capacity)) {
    txn->redo_stack[txn->redo_count++] = op;
    return make_status(PC_ERR_MEMORY, "OOM");
  }
  txn->undo_stack[txn->undo_count++] = op;

  // TODO: Apply forward operation to document IR

  return make_status(PC_ERR_NONE, nullptr);
}

size_t pc_txn_undo_count(pc_txn* txn) {
  return txn ? txn->undo_count : 0;
}

size_t pc_txn_redo_count(pc_txn* txn) {
  return txn ? txn->redo_count : 0;
}

pc_status pc_txn_clear_log(pc_txn* txn) {
  if (!txn) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }
  for (size_t i = 0; i < txn->undo_count; ++i) {
    txn_op_free(txn->undo_stack[i]);
  }
  txn->undo_count = 0;
  for (size_t i = 0; i < txn->redo_count; ++i) {
    txn_op_free(txn->redo_stack[i]);
  }
  txn->redo_count = 0;
  return make_status(PC_ERR_NONE, nullptr);
}

pc_status pc_txn_log_to_json(pc_txn* txn, char** out_json) {
  if (!txn || !out_json) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }

  // Build JSON: {"undo":[...],"redo":[...]}
  size_t cap = 8192;
  char* buf = (char*)malloc(cap);
  if (!buf) return make_status(PC_ERR_MEMORY, "OOM");

  size_t pos = 0;
  auto append = [&](const char* s) {
    size_t len = strlen(s);
    if (pos + len >= cap) {
      cap *= 2;
      char* nb = (char*)realloc(buf, cap);
      if (!nb) { free(buf); return 0; }
      buf = nb;
    }
    memcpy(buf + pos, s, len);
    pos += len;
    return 1;
  };
  auto append_int = [&](int64_t v) {
    char tmp[32];
    int len = snprintf(tmp, sizeof(tmp), "%ld", v);
    if (pos + len >= cap) {
      cap *= 2;
      char* nb = (char*)realloc(buf, cap);
      if (!nb) { free(buf); return 0; }
      buf = nb;
    }
    memcpy(buf + pos, tmp, len);
    pos += len;
    return 1;
  };
  auto append_escaped = [&](const char* s) {
    if (!s) s = "";
    append("\"");
    for (const char* p = s; *p; ++p) {
      unsigned char c = *p;
      char esc[7] = {0};
      int elen = 0;
      switch (c) {
        case '"':  esc[0] = '\\'; esc[1] = '"'; elen = 2; break;
        case '\\': esc[0] = '\\'; esc[1] = '\\'; elen = 2; break;
        case '\b': esc[0] = '\\'; esc[1] = 'b'; elen = 2; break;
        case '\f': esc[0] = '\\'; esc[1] = 'f'; elen = 2; break;
        case '\n': esc[0] = '\\'; esc[1] = 'n'; elen = 2; break;
        case '\r': esc[0] = '\\'; esc[1] = 'r'; elen = 2; break;
        case '\t': esc[0] = '\\'; esc[1] = 't'; elen = 2; break;
        default:
          if (c < 0x20) { snprintf(esc, sizeof(esc), "\\u%04x", c); elen = 6; }
          break;
      }
      if (elen) {
        if (!append(esc)) return 0;
      } else {
        if (pos + 1 >= cap) return 0;
        buf[pos++] = c;
      }
    }
    append("\"");
    return 1;
  };

  if (!append("{\n  \"undo\": [\n")) { free(buf); return make_status(PC_ERR_MEMORY, "OOM"); }
  for (size_t i = 0; i < txn->undo_count; ++i) {
    pc_txn_op* op = txn->undo_stack[i];
    append("    {\n      \"type\": ");
    append_int(op->type);
    append(",\n      \"target_id\": ");
    append_escaped(op->target_id);
    append(",\n      \"before\": ");
    append(op->before_json ? op->before_json : "{}");
    append(",\n      \"after\": ");
    append(op->after_json ? op->after_json : "{}");
    append("\n    }");
    if (i + 1 < txn->undo_count) append(",");
    append("\n");
  }
  append("  ],\n  \"redo\": [\n");
  for (size_t i = 0; i < txn->redo_count; ++i) {
    pc_txn_op* op = txn->redo_stack[i];
    append("    {\n      \"type\": ");
    append_int(op->type);
    append(",\n      \"target_id\": ");
    append_escaped(op->target_id);
    append(",\n      \"before\": ");
    append(op->before_json ? op->before_json : "{}");
    append(",\n      \"after\": ");
    append(op->after_json ? op->after_json : "{}");
    append("\n    }");
    if (i + 1 < txn->redo_count) append(",");
    append("\n");
  }
  append("  ]\n}\n");

  *out_json = buf;
  return make_status(PC_ERR_NONE, nullptr);
}

pc_status pc_txn_replay_from_json(pc_txn* txn, const char* json) {
  if (!txn || !json) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }
  // TODO: Implement JSON parsing and replay
  return make_status(PC_ERR_UNSUPPORTED, "not implemented");
}

int pc_txn_is_log_empty(pc_txn* txn) {
  return txn && txn->undo_count == 0 && txn->redo_count == 0;
}

void pc_txn_set_max_ops(pc_txn* txn, size_t max_ops) {
  if (txn) txn->max_ops = max_ops;
}

size_t pc_txn_get_max_ops(pc_txn* txn) {
  return txn ? txn->max_ops : 0;
}