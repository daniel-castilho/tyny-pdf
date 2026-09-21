#include "transaction.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "pdfcore/sha256.h"
#include "pdfcore/sidecar.h"
#include "pdfcore/transaction.h"

// One logged mutation. `before` is the IR snapshot captured at apply time (zero rect for ADD,
// meaning the element did not exist); `after` is the target state. Value types only, never an
// engine handle (R-M4, R-M8).
struct Command {
  enum Type { ADD_ANNOT = 1, MOVE = 2, DELETE = 3 } type;
  char id[11];
  pc_rect before;
  pc_rect after;
};

struct pc_txn {
  pc_doc* doc;
  std::vector<Command> undo;
  std::vector<Command> redo;
  uint32_t max_tiles;   // 0 = unlimited
  uint64_t max_bytes;   // 0 = unlimited
  uint64_t undo_bytes;  // running estimate of the undo stack
};

static uint64_t command_bytes_estimate() {
  // Every command holds the same fixed value-type payload; its in-memory size is the
  // tile envelope.
  return static_cast<uint64_t>(sizeof(Command));
}

static pc_status status_none() {
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

static pc_status status_err(uint32_t code, const char* detail) {
  return {sizeof(pc_status), code, 0, detail};
}

static IrAnnotation* find_annotation(pc_doc* doc, const char* id) {
  for (uint32_t i = 0; i < doc->annotation_count; ++i) {
    if (std::strcmp(doc->annotations[i].id, id) == 0) {
      return &doc->annotations[i];
    }
  }
  return nullptr;
}

static pc_status ir_annot_insert(pc_doc* doc, const char* id, pc_rect rect) {
  if (doc->annotation_count == doc->annotation_cap) {
    uint32_t new_cap = doc->annotation_cap == 0 ? 4 : doc->annotation_cap * 2;
    IrAnnotation* grown =
        static_cast<IrAnnotation*>(std::realloc(doc->annotations, new_cap * sizeof(IrAnnotation)));
    if (!grown) {
      return status_err(PC_ERR_MEMORY, "OOM");
    }
    doc->annotations = grown;
    doc->annotation_cap = new_cap;
  }
  IrAnnotation& ann = doc->annotations[doc->annotation_count++];
  std::strncpy(ann.id, id, sizeof(ann.id) - 1);
  ann.id[sizeof(ann.id) - 1] = '\0';
  ann.rect = rect;
  return status_none();
}

static void ir_annot_remove(pc_doc* doc, uint32_t index) {
  std::memmove(doc->annotations + index, doc->annotations + index + 1,
               static_cast<size_t>(doc->annotation_count - index - 1) * sizeof(IrAnnotation));
  --doc->annotation_count;
}

// Apply the target state `after` of `cmd` to the IR. Returns a command carrying the captured
// `before` state, or PC_ERR_ARGUMENT / PC_ERR_MEMORY with the IR left untouched.
static pc_status ir_apply(pc_doc* doc, const pc_command* cmd, Command& out) {
  out.type = static_cast<Command::Type>(cmd->type);
  std::strncpy(out.id, cmd->annotation_id, sizeof(out.id) - 1);
  out.id[sizeof(out.id) - 1] = '\0';
  out.before = {};
  out.after = cmd->after;

  IrAnnotation* ann = find_annotation(doc, out.id);
  switch (cmd->type) {
    case PC_CMD_ADD_ANNOT:
      if (ann) {
        return status_err(PC_ERR_ARGUMENT, "annotation id already exists");
      }
      return ir_annot_insert(doc, out.id, cmd->after);
    case PC_CMD_MOVE:
      if (!ann) {
        return status_err(PC_ERR_ARGUMENT, "annotation not found");
      }
      out.before = ann->rect;
      ann->rect = cmd->after;
      return status_none();
    case PC_CMD_DELETE:
      if (!ann) {
        return status_err(PC_ERR_ARGUMENT, "annotation not found");
      }
      out.before = ann->rect;
      ir_annot_remove(doc, static_cast<uint32_t>(ann - doc->annotations));
      return status_none();
    default:
      return status_err(PC_ERR_ARGUMENT, "unknown command type");
  }
}

// Reverse a stored command: undo applies `before`, redo applies `after`.
static pc_status ir_reverse(pc_doc* doc, const Command& cmd, bool undo_not_redo) {
  pc_rect target = undo_not_redo ? cmd.before : cmd.after;
  IrAnnotation* ann = find_annotation(doc, cmd.id);
  switch (cmd.type) {
    case Command::ADD_ANNOT:
      if (undo_not_redo) {
        if (!ann) {
          return status_err(PC_ERR_STATE, "undo violated IR invariant");
        }
        ir_annot_remove(doc, static_cast<uint32_t>(ann - doc->annotations));
      } else {
        return ir_annot_insert(doc, cmd.id, target);
      }
      return status_none();
    case Command::MOVE:
      if (!ann) {
        return status_err(PC_ERR_STATE, "undo violated IR invariant");
      }
      ann->rect = target;
      return status_none();
    case Command::DELETE:
      if (undo_not_redo) {
        return ir_annot_insert(doc, cmd.id, target);
      }
      if (!ann) {
        return status_err(PC_ERR_STATE, "undo violated IR invariant");
      }
      ir_annot_remove(doc, static_cast<uint32_t>(ann - doc->annotations));
      return status_none();
    default:
      return status_err(PC_ERR_STATE, "undo violated IR invariant");
  }
}

pc_status pc_txn_create(pc_doc* doc, const pc_budget* budget, pc_txn** out_txn) {
  if (!doc || !out_txn) {
    return status_err(PC_ERR_ARGUMENT, "null argument");
  }
  pc_txn* txn = new (std::nothrow) pc_txn();
  if (!txn) {
    return status_err(PC_ERR_MEMORY, "OOM");
  }
  txn->doc = doc;
  txn->max_tiles = (budget && budget->max_tiles > 0) ? budget->max_tiles : 0;
  txn->max_bytes = (budget && budget->max_bytes > 0) ? budget->max_bytes : 0;
  txn->undo_bytes = 0;
  *out_txn = txn;
  return status_none();
}

pc_status pc_txn_apply(pc_txn* txn, const pc_command* cmd) {
  if (!txn || !cmd) {
    return status_err(PC_ERR_ARGUMENT, "null argument");
  }
  if (pc_sidecar_validate_annotation_id(cmd->annotation_id).code != PC_ERR_NONE) {
    return status_err(PC_ERR_ARGUMENT, "invalid annotation id");
  }

  Command c;
  pc_status s = ir_apply(txn->doc, cmd, c);
  if (s.code != PC_ERR_NONE) {
    return s;
  }

  uint64_t est = command_bytes_estimate();
  if ((txn->max_tiles > 0 && txn->undo.size() >= txn->max_tiles) ||
      (txn->max_bytes > 0 && txn->undo_bytes + est > txn->max_bytes)) {
    // Leave the IR as it was: reverse the mutation just performed.
    ir_reverse(txn->doc, c, true);
    return status_err(PC_ERR_LIMIT, "undo budget exceeded");
  }

  txn->undo.push_back(c);
  txn->undo_bytes += est;
  txn->redo.clear();
  return status_none();
}

pc_status pc_txn_undo(pc_txn* txn) {
  if (!txn) {
    return status_err(PC_ERR_ARGUMENT, "null argument");
  }
  if (txn->undo.empty()) {
    return status_err(PC_ERR_STATE, "nothing to undo");
  }
  Command c = txn->undo.back();
  txn->undo.pop_back();
  pc_status s = ir_reverse(txn->doc, c, true);
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  txn->undo_bytes -= command_bytes_estimate();
  txn->redo.push_back(c);
  return status_none();
}

pc_status pc_txn_redo(pc_txn* txn) {
  if (!txn) {
    return status_err(PC_ERR_ARGUMENT, "null argument");
  }
  if (txn->redo.empty()) {
    return status_err(PC_ERR_STATE, "nothing to redo");
  }
  Command c = txn->redo.back();
  txn->redo.pop_back();
  pc_status s = ir_reverse(txn->doc, c, false);
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  txn->undo.push_back(c);
  txn->undo_bytes += command_bytes_estimate();
  return status_none();
}

void pc_txn_free(pc_txn* txn) {
  delete txn;
}

pc_status pc_doc_hash(const pc_doc* doc, char out_hex[65]) {
  if (!doc || !out_hex) {
    if (out_hex) {
      out_hex[0] = '\0';
    }
    return status_err(PC_ERR_ARGUMENT, "null argument");
  }

  pc_sha256 ctx;
  pc_sha256_init(&ctx);

  uint32_t page_count = doc->page_count;
  pc_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(&page_count), sizeof(page_count));
  pc_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(doc->page_boxes),
                   static_cast<size_t>(doc->page_count) * sizeof(pc_page_box));

  uint32_t annotation_count = doc->annotation_count;
  pc_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(&annotation_count),
                   sizeof(annotation_count));
  for (uint32_t i = 0; i < annotation_count; ++i) {
    pc_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(doc->annotations[i].id),
                     sizeof(doc->annotations[i].id));
    pc_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(&doc->annotations[i].rect),
                     sizeof(pc_rect));
  }

  unsigned char digest[32];
  pc_sha256_final(&ctx, digest);
  for (int i = 0; i < 32; ++i) {
    std::sprintf(out_hex + i * 2, "%02x", digest[i]);
  }
  out_hex[64] = '\0';
  return status_none();
}