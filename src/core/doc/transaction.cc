#include "transaction.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "pdfcore/json.h"
#include "pdfcore/sha256.h"
#include "pdfcore/sidecar.h"
#include "pdfcore/transaction.h"

// One logged mutation. `before` is the IR snapshot captured at apply time (zero rect for ADD,
// meaning the element did not exist); `after` is the target state. Value types only, never an
// engine handle (R-M4, R-M8).
struct Command {
  enum Type { ADD_ANNOT = 1, MOVE = 2, DELETE = 3 } type = ADD_ANNOT;
  char id[11] = {};
  pc_rect before = {};
  pc_rect after = {};
  pc_json_value* extras = nullptr;  // unknown keys, owned

  Command() = default;
  Command(const Command& o) {
    type = o.type;
    std::memcpy(id, o.id, sizeof(id));
    before = o.before;
    after = o.after;
    extras = pc_json_clone(o.extras);
  }
  Command& operator=(const Command& o) {
    if (this == &o) {
      return *this;
    }
    type = o.type;
    std::memcpy(id, o.id, sizeof(id));
    before = o.before;
    after = o.after;
    pc_json_free(extras);
    extras = pc_json_clone(o.extras);
    return *this;
  }
  Command(Command&& o) noexcept {
    type = o.type;
    std::memcpy(id, o.id, sizeof(id));
    before = o.before;
    after = o.after;
    extras = o.extras;
    o.extras = nullptr;
  }
  Command& operator=(Command&& o) noexcept {
    if (this == &o) {
      return *this;
    }
    pc_json_free(extras);
    type = o.type;
    std::memcpy(id, o.id, sizeof(id));
    before = o.before;
    after = o.after;
    extras = o.extras;
    o.extras = nullptr;
    return *this;
  }
  ~Command() { pc_json_free(extras); }
};

struct pc_txn {
  pc_doc* doc;
  std::vector<Command> undo;
  std::vector<Command> redo;
  uint32_t max_tiles;   // 0 = unlimited
  uint64_t max_bytes;   // 0 = unlimited
  uint64_t undo_bytes;  // running estimate of the undo stack
  pc_json_value* extras = nullptr;

  ~pc_txn() { pc_json_free(extras); }
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

static bool is_known_cmd_key(const char* key) {
  return std::strcmp(key, "after") == 0 || std::strcmp(key, "annotation_id") == 0 ||
         std::strcmp(key, "before") == 0 || std::strcmp(key, "type") == 0;
}

static bool is_known_root_key(const char* key) {
  return std::strcmp(key, "budget") == 0 || std::strcmp(key, "redo") == 0 ||
         std::strcmp(key, "undo") == 0;
}

static pc_json_value* extras_from_object(const pc_json_value* obj, bool (*known)(const char*)) {
  pc_json_value* extras = nullptr;
  size_t n = pc_json_object_size(obj);
  for (size_t i = 0; i < n; ++i) {
    const char* key = pc_json_object_key_at(obj, i);
    if (!key || known(key)) {
      continue;
    }
    if (!extras) {
      extras = pc_json_object(nullptr);
      if (!extras) {
        return nullptr;
      }
    }
    pc_json_object_put(extras, key, pc_json_clone(pc_json_object_value_at(obj, i)));
  }
  return extras;
}

static void merge_extras(pc_json_value* obj, const pc_json_value* extras) {
  if (!obj || !extras) {
    return;
  }
  size_t n = pc_json_object_size(extras);
  for (size_t i = 0; i < n; ++i) {
    const char* key = pc_json_object_key_at(extras, i);
    pc_json_object_put(obj, key, pc_json_clone(pc_json_object_value_at(extras, i)));
  }
}

static pc_json_value* rect_to_json(const pc_rect& r) {
  pc_json_pair pairs[] = {
      {"x0", pc_json_double(r.x0)}, {"x1", pc_json_double(r.x1)}, {"y0", pc_json_double(r.y0)},
      {"y1", pc_json_double(r.y1)}, {nullptr, nullptr},
  };
  return pc_json_object(pairs);
}

static pc_json_value* command_to_json(const Command& c) {
  const char* type_str = (c.type == Command::ADD_ANNOT) ? "ADD_ANNOT"
                         : (c.type == Command::MOVE)    ? "MOVE"
                                                        : "DELETE";
  pc_json_pair pairs[] = {
      {"after", rect_to_json(c.after)},
      {"annotation_id", pc_json_string(c.id)},
      {"before", rect_to_json(c.before)},
      {"type", pc_json_string(type_str)},
      {nullptr, nullptr},
  };
  pc_json_value* obj = pc_json_object(pairs);
  merge_extras(obj, c.extras);
  return obj;
}

static pc_json_value* commands_to_json(const std::vector<Command>& cmds) {
  std::vector<pc_json_value*> arr;
  arr.reserve(cmds.size() + 1);
  for (const Command& c : cmds) {
    arr.push_back(command_to_json(c));
  }
  arr.push_back(nullptr);
  return pc_json_array(arr.data());
}

pc_status pc_txn_to_json(const pc_txn* txn, char** out_json) {
  if (!txn || !out_json) {
    return status_err(PC_ERR_ARGUMENT, "null argument");
  }

  pc_json_pair budget_pairs[] = {
      {"max_bytes", pc_json_int(static_cast<int64_t>(txn->max_bytes))},
      {"max_tiles", pc_json_int(static_cast<int64_t>(txn->max_tiles))},
      {nullptr, nullptr},
  };
  pc_json_pair root_pairs[] = {
      {"budget", pc_json_object(budget_pairs)},
      {"redo", commands_to_json(txn->redo)},
      {"undo", commands_to_json(txn->undo)},
      {nullptr, nullptr},
  };
  pc_json_value* root = pc_json_object(root_pairs);
  if (!root) {
    return status_err(PC_ERR_MEMORY, "OOM building JSON");
  }
  merge_extras(root, txn->extras);

  char json_buf[65536];
  int len = pc_json_serialize(root, json_buf, sizeof(json_buf));
  pc_json_free(root);

  if (len < 0 || len >= static_cast<int>(sizeof(json_buf))) {
    return status_err(PC_ERR_MEMORY, "JSON too large");
  }

  *out_json = static_cast<char*>(std::malloc(static_cast<size_t>(len) + 1));
  if (!*out_json) {
    return status_err(PC_ERR_MEMORY, "OOM");
  }
  std::memcpy(*out_json, json_buf, static_cast<size_t>(len) + 1);
  return status_none();
}

static pc_rect rect_from_json(const pc_json_value* obj) {
  pc_rect r = {};
  if (!obj || pc_json_get_type(obj) != PC_JSON_OBJECT) {
    return r;
  }
  r.x0 = pc_json_as_number(pc_json_object_get(obj, "x0"));
  r.y0 = pc_json_as_number(pc_json_object_get(obj, "y0"));
  r.x1 = pc_json_as_number(pc_json_object_get(obj, "x1"));
  r.y1 = pc_json_as_number(pc_json_object_get(obj, "y1"));
  return r;
}

static pc_status command_from_json(const pc_json_value* obj, Command& c) {
  if (!obj || pc_json_get_type(obj) != PC_JSON_OBJECT) {
    return status_err(PC_ERR_ARGUMENT, "malformed JSON");
  }
  const char* type_str = pc_json_as_string(pc_json_object_get(obj, "type"));
  if (!type_str) {
    return status_err(PC_ERR_ARGUMENT, "malformed JSON");
  }
  if (std::strcmp(type_str, "ADD_ANNOT") == 0) {
    c.type = Command::ADD_ANNOT;
  } else if (std::strcmp(type_str, "MOVE") == 0) {
    c.type = Command::MOVE;
  } else if (std::strcmp(type_str, "DELETE") == 0) {
    c.type = Command::DELETE;
  } else {
    return status_err(PC_ERR_ARGUMENT, "unknown command type");
  }
  const char* id = pc_json_as_string(pc_json_object_get(obj, "annotation_id"));
  if (!id) {
    return status_err(PC_ERR_ARGUMENT, "malformed JSON");
  }
  std::strncpy(c.id, id, sizeof(c.id) - 1);
  c.id[sizeof(c.id) - 1] = '\0';
  c.before = rect_from_json(pc_json_object_get(obj, "before"));
  c.after = rect_from_json(pc_json_object_get(obj, "after"));
  c.extras = extras_from_object(obj, is_known_cmd_key);
  return status_none();
}

static pc_status apply_undo_commands(pc_txn* txn, const pc_json_value* arr) {
  if (!arr) {
    return status_none();
  }
  if (pc_json_get_type(arr) != PC_JSON_ARRAY) {
    return status_err(PC_ERR_ARGUMENT, "malformed JSON");
  }
  size_t n = pc_json_array_size(arr);
  for (size_t i = 0; i < n; ++i) {
    Command c;
    pc_status s = command_from_json(pc_json_array_at(arr, i), c);
    if (s.code != PC_ERR_NONE) {
      return s;
    }
    pc_command pc_cmd = {sizeof(pc_command), static_cast<pc_command_type>(c.type), "", c.before,
                         c.after};
    std::strncpy(pc_cmd.annotation_id, c.id, sizeof(pc_cmd.annotation_id) - 1);
    s = pc_txn_apply(txn, &pc_cmd);
    if (s.code != PC_ERR_NONE) {
      return s;
    }
    // Restore unknown keys onto the applied command (apply built a fresh Command).
    if (c.extras && !txn->undo.empty()) {
      pc_json_free(txn->undo.back().extras);
      txn->undo.back().extras = pc_json_clone(c.extras);
    }
  }
  return status_none();
}

static pc_status load_redo_commands(pc_txn* txn, const pc_json_value* arr) {
  if (!arr) {
    return status_none();
  }
  if (pc_json_get_type(arr) != PC_JSON_ARRAY) {
    return status_err(PC_ERR_ARGUMENT, "malformed JSON");
  }
  size_t n = pc_json_array_size(arr);
  for (size_t i = 0; i < n; ++i) {
    Command c;
    pc_status s = command_from_json(pc_json_array_at(arr, i), c);
    if (s.code != PC_ERR_NONE) {
      return s;
    }
    txn->redo.push_back(std::move(c));
  }
  return status_none();
}

pc_status pc_txn_from_json(const char* json_str, pc_doc* doc, pc_txn** out_txn) {
  if (!json_str || !doc || !out_txn) {
    return status_err(PC_ERR_ARGUMENT, "null argument");
  }

  pc_json_value* root = pc_json_parse(json_str);
  if (!root || pc_json_get_type(root) != PC_JSON_OBJECT) {
    pc_json_free(root);
    return status_err(PC_ERR_ARGUMENT, "malformed JSON");
  }

  const pc_json_value* budget_obj = pc_json_object_get(root, "budget");
  uint32_t max_tiles = 0;
  uint64_t max_bytes = 0;
  if (budget_obj && pc_json_get_type(budget_obj) == PC_JSON_OBJECT) {
    max_tiles = static_cast<uint32_t>(pc_json_as_int(pc_json_object_get(budget_obj, "max_tiles")));
    max_bytes = static_cast<uint64_t>(pc_json_as_int(pc_json_object_get(budget_obj, "max_bytes")));
  }

  pc_budget budget = {sizeof(pc_budget), max_tiles, max_bytes};
  pc_txn* txn = nullptr;
  pc_status s = pc_txn_create(doc, &budget, &txn);
  if (s.code != PC_ERR_NONE) {
    pc_json_free(root);
    return s;
  }

  s = apply_undo_commands(txn, pc_json_object_get(root, "undo"));
  if (s.code != PC_ERR_NONE) {
    pc_txn_free(txn);
    pc_json_free(root);
    return s;
  }
  s = load_redo_commands(txn, pc_json_object_get(root, "redo"));
  if (s.code != PC_ERR_NONE) {
    pc_txn_free(txn);
    pc_json_free(root);
    return s;
  }

  txn->extras = extras_from_object(root, is_known_root_key);
  pc_json_free(root);
  *out_txn = txn;
  return status_none();
}