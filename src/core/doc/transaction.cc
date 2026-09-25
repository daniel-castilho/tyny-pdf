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

// Command and pc_txn are declared in the internal transaction.h (shared with forms.cc,
// story 7.2); their non-trivial members are defined here.
Command::Command(const Command& o) {
  type = o.type;
  std::memcpy(id, o.id, sizeof(id));
  before = o.before;
  after = o.after;
  std::memcpy(form_field_name, o.form_field_name, sizeof(form_field_name));
  std::memcpy(form_old_value, o.form_old_value, sizeof(form_old_value));
  std::memcpy(form_new_value, o.form_new_value, sizeof(form_new_value));
  flat_count = o.flat_count;
  flat_fields = nullptr;
  if (o.flat_fields && o.flat_count) {
    // Deep copy: the snapshot is IR value types, never shared (R-M4). malloc family to
    // match the doc's realloc-grown arrays (pc_doc_close frees with std::free).
    flat_fields = static_cast<IrFormField*>(std::malloc(o.flat_count * sizeof(IrFormField)));
    if (flat_fields) {
      for (uint32_t i = 0; i < o.flat_count; ++i) {
        flat_fields[i] = o.flat_fields[i];
      }
    } else {
      flat_count = 0;
    }
  }
  extras = pc_json_clone(o.extras);
}

Command& Command::operator=(const Command& o) {
  if (this == &o) {
    return *this;
  }
  type = o.type;
  std::memcpy(id, o.id, sizeof(id));
  before = o.before;
  after = o.after;
  std::memcpy(form_field_name, o.form_field_name, sizeof(form_field_name));
  std::memcpy(form_old_value, o.form_old_value, sizeof(form_old_value));
  std::memcpy(form_new_value, o.form_new_value, sizeof(form_new_value));
  std::free(flat_fields);
  flat_fields = nullptr;
  flat_count = o.flat_count;
  if (o.flat_fields && o.flat_count) {
    flat_fields = static_cast<IrFormField*>(std::malloc(o.flat_count * sizeof(IrFormField)));
    if (flat_fields) {
      for (uint32_t i = 0; i < o.flat_count; ++i) {
        flat_fields[i] = o.flat_fields[i];
      }
    } else {
      flat_count = 0;
    }
  }
  pc_json_free(extras);
  extras = pc_json_clone(o.extras);
  return *this;
}

Command::Command(Command&& o) noexcept {
  type = o.type;
  std::memcpy(id, o.id, sizeof(id));
  before = o.before;
  after = o.after;
  std::memcpy(form_field_name, o.form_field_name, sizeof(form_field_name));
  std::memcpy(form_old_value, o.form_old_value, sizeof(form_old_value));
  std::memcpy(form_new_value, o.form_new_value, sizeof(form_new_value));
  flat_fields = o.flat_fields;
  flat_count = o.flat_count;
  o.flat_fields = nullptr;
  o.flat_count = 0;
  extras = o.extras;
  o.extras = nullptr;
}

Command& Command::operator=(Command&& o) noexcept {
  if (this == &o) {
    return *this;
  }
  pc_json_free(extras);
  std::free(flat_fields);
  type = o.type;
  std::memcpy(id, o.id, sizeof(id));
  before = o.before;
  after = o.after;
  std::memcpy(form_field_name, o.form_field_name, sizeof(form_field_name));
  std::memcpy(form_old_value, o.form_old_value, sizeof(form_old_value));
  std::memcpy(form_new_value, o.form_new_value, sizeof(form_new_value));
  flat_fields = o.flat_fields;
  flat_count = o.flat_count;
  o.flat_fields = nullptr;
  o.flat_count = 0;
  extras = o.extras;
  o.extras = nullptr;
  return *this;
}

Command::~Command() {
  pc_json_free(extras);
  std::free(flat_fields);
}

pc_txn::~pc_txn() {
  pc_json_free(extras);
}

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

  switch (cmd->type) {
    case PC_CMD_ADD_ANNOT: {
      IrAnnotation* ann = find_annotation(doc, out.id);
      if (ann) {
        return status_err(PC_ERR_ARGUMENT, "annotation id already exists");
      }
      return ir_annot_insert(doc, out.id, cmd->after);
    }
    case PC_CMD_MOVE: {
      IrAnnotation* ann = find_annotation(doc, out.id);
      if (!ann) {
        return status_err(PC_ERR_ARGUMENT, "annotation not found");
      }
      out.before = ann->rect;
      ann->rect = cmd->after;
      return status_none();
    }
    case PC_CMD_DELETE: {
      IrAnnotation* ann = find_annotation(doc, out.id);
      if (!ann) {
        return status_err(PC_ERR_ARGUMENT, "annotation not found");
      }
      out.before = ann->rect;
      ir_annot_remove(doc, static_cast<uint32_t>(ann - doc->annotations));
      return status_none();
    }
    case PC_CMD_FORM_SET: {
      // The fill itself was validated by pc_form_fill_field before the command was built;
      // apply only needs the field to exist and copies the target value into the IR.
      IrFormField* field = ir_form_field_find(doc, cmd->form_field_name);
      if (!field) {
        return status_err(PC_ERR_ARGUMENT, "form field not found");
      }
      std::strncpy(out.form_field_name, cmd->form_field_name, sizeof(out.form_field_name) - 1);
      out.form_field_name[sizeof(out.form_field_name) - 1] = '\0';
      std::strncpy(out.form_old_value, field->value, sizeof(out.form_old_value) - 1);
      out.form_old_value[sizeof(out.form_old_value) - 1] = '\0';
      std::strncpy(out.form_new_value, cmd->form_new_value, sizeof(out.form_new_value) - 1);
      out.form_new_value[sizeof(out.form_new_value) - 1] = '\0';
      std::strncpy(field->value, cmd->form_new_value, sizeof(field->value) - 1);
      field->value[sizeof(field->value) - 1] = '\0';
      return status_none();
    }
    case PC_CMD_FORM_FLATTEN: {
      // Steal the IR's field array into the command: the doc has no fields until undo
      // hands the array back. O(1), no copy, and the snapshot is pure IR value types
      // (R-M4, story 7.4).
      out.flat_fields = doc->form_fields;
      out.flat_count = doc->form_field_count;
      doc->form_fields = nullptr;
      doc->form_field_count = 0;
      doc->form_field_cap = 0;
      return status_none();
    }
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
    case Command::FORM_SET: {
      IrFormField* field = ir_form_field_find(doc, cmd.form_field_name);
      if (!field) {
        return status_err(PC_ERR_STATE, "form field not found during undo/redo");
      }
      // For undo: restore old value; for redo: apply new value
      if (undo_not_redo) {
        std::strncpy(field->value, cmd.form_old_value, sizeof(field->value) - 1);
        field->value[sizeof(field->value) - 1] = '\0';
      } else {
        std::strncpy(field->value, cmd.form_new_value, sizeof(field->value) - 1);
        field->value[sizeof(field->value) - 1] = '\0';
      }
      return status_none();
    }
    case Command::FORM_FLATTEN: {
      // The command's snapshot is private; the doc never shares a pointer with a live
      // Command, because the undo/redo stacks copy Commands around by value. Undo
      // builds the doc a fresh array from the snapshot; redo frees the doc's array.
      // Both directions stay alias-free under any copy pattern (story 7.4).
      if (undo_not_redo) {
        if (doc->form_fields) {
          return status_err(PC_ERR_STATE, "undo violated IR invariant");
        }
        if (cmd.flat_count) {
          doc->form_fields =
              static_cast<IrFormField*>(std::malloc(cmd.flat_count * sizeof(IrFormField)));
          if (!doc->form_fields) {
            return status_err(PC_ERR_MEMORY, "OOM restoring flattened fields");
          }
          for (uint32_t i = 0; i < cmd.flat_count; ++i) {
            doc->form_fields[i] = cmd.flat_fields[i];
          }
        }
        doc->form_field_count = cmd.flat_count;
        doc->form_field_cap = cmd.flat_count;
        return status_none();
      }
      std::free(doc->form_fields);
      doc->form_fields = nullptr;
      doc->form_field_count = 0;
      doc->form_field_cap = 0;
      return status_none();
    }
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
  // The base32 annotation id is only meaningful for annotation commands; PC_CMD_FORM_SET
  // identifies its target by form_field_name instead (story 7.2), and PC_CMD_FORM_FLATTEN
  // targets the whole form (story 7.4).
  if (cmd->type != PC_CMD_FORM_SET && cmd->type != PC_CMD_FORM_FLATTEN &&
      pc_sidecar_validate_annotation_id(cmd->annotation_id).code != PC_ERR_NONE) {
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

  // Form field state (story 7.2): a fill changes the hash, an undo restores it. Strings are
  // hashed by content length, never by the fixed IR buffers - padding bytes are not stable.
  uint32_t form_field_count = doc->form_field_count;
  pc_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(&form_field_count),
                   sizeof(form_field_count));
  for (uint32_t i = 0; i < form_field_count; ++i) {
    const IrFormField* f = &doc->form_fields[i];
    pc_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(f->name), std::strlen(f->name) + 1);
    pc_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(f->value), std::strlen(f->value) + 1);
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
         std::strcmp(key, "before") == 0 || std::strcmp(key, "type") == 0 ||
         std::strcmp(key, "field_name") == 0 || std::strcmp(key, "old_value") == 0 ||
         std::strcmp(key, "new_value") == 0 || std::strcmp(key, "flattened_fields") == 0;
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

// Story 7.4: the FORM_FLATTEN snapshot rides in the command JSON so a replayed log
// can undo the flatten. Pure value types on both sides (R-M4).
static pc_json_value* field_to_json(const IrFormField& f) {
  pc_json_pair rect_pairs[] = {
      {"x0", pc_json_double(f.rect.x0)},
      {"x1", pc_json_double(f.rect.x1)},
      {"y0", pc_json_double(f.rect.y0)},
      {"y1", pc_json_double(f.rect.y1)},
      {nullptr, nullptr},
  };
  pc_json_pair pairs[] = {
      {"default_value", pc_json_string(f.default_value)},
      {"flags", pc_json_int(static_cast<int64_t>(f.flags))},
      {"format", pc_json_string(f.format)},
      {"max_len", pc_json_int(static_cast<int64_t>(f.max_len))},
      {"name", pc_json_string(f.name)},
      {"page_index", pc_json_int(static_cast<int64_t>(f.page_index))},
      {"rect", pc_json_object(rect_pairs)},
      {"type", pc_json_int(static_cast<int64_t>(f.type))},
      {"value", pc_json_string(f.value)},
      {nullptr, nullptr},
  };
  return pc_json_object(pairs);
}

static pc_json_value* fields_to_json(const IrFormField* fields, uint32_t count) {
  std::vector<pc_json_value*> arr;
  arr.reserve(count + 1);
  for (uint32_t i = 0; i < count; ++i) {
    arr.push_back(field_to_json(fields[i]));
  }
  arr.push_back(nullptr);
  return pc_json_array(arr.data());
}

static pc_json_value* command_to_json(const Command& c) {
  const char* type_str = (c.type == Command::ADD_ANNOT)  ? "ADD_ANNOT"
                         : (c.type == Command::MOVE)     ? "MOVE"
                         : (c.type == Command::DELETE)   ? "DELETE"
                         : (c.type == Command::FORM_SET) ? "FORM_SET"
                                                         : "FORM_FLATTEN";
  pc_json_pair pairs[] = {
      {"after", rect_to_json(c.after)},
      {"annotation_id", pc_json_string(c.id)},
      {"before", rect_to_json(c.before)},
      {"type", pc_json_string(type_str)},
      {nullptr, nullptr},
  };
  pc_json_value* obj = pc_json_object(pairs);
  if (c.type == Command::FORM_SET) {
    // The form payload is emitted on the FORM_SET command itself so a sidecar log
    // round-trips fill/undo without relying on the extras mechanism (story 7.2).
    pc_json_object_put(obj, "field_name", pc_json_string(c.form_field_name));
    pc_json_object_put(obj, "old_value", pc_json_string(c.form_old_value));
    pc_json_object_put(obj, "new_value", pc_json_string(c.form_new_value));
  }
  if (c.type == Command::FORM_FLATTEN) {
    pc_json_object_put(obj, "flattened_fields", fields_to_json(c.flat_fields, c.flat_count));
  }
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
  } else if (std::strcmp(type_str, "FORM_SET") == 0) {
    c.type = Command::FORM_SET;
  } else if (std::strcmp(type_str, "FORM_FLATTEN") == 0) {
    c.type = Command::FORM_FLATTEN;
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
  if (c.type == Command::FORM_SET) {
    const char* field_name = pc_json_as_string(pc_json_object_get(obj, "field_name"));
    const char* old_value = pc_json_as_string(pc_json_object_get(obj, "old_value"));
    const char* new_value = pc_json_as_string(pc_json_object_get(obj, "new_value"));
    if (!field_name || !old_value || !new_value) {
      return status_err(PC_ERR_ARGUMENT, "malformed FORM_SET JSON");
    }
    std::strncpy(c.form_field_name, field_name, sizeof(c.form_field_name) - 1);
    c.form_field_name[sizeof(c.form_field_name) - 1] = '\0';
    std::strncpy(c.form_old_value, old_value, sizeof(c.form_old_value) - 1);
    c.form_old_value[sizeof(c.form_old_value) - 1] = '\0';
    std::strncpy(c.form_new_value, new_value, sizeof(c.form_new_value) - 1);
    c.form_new_value[sizeof(c.form_new_value) - 1] = '\0';
  }
  if (c.type == Command::FORM_FLATTEN) {
    const pc_json_value* arr = pc_json_object_get(obj, "flattened_fields");
    if (!arr || pc_json_get_type(arr) != PC_JSON_ARRAY) {
      return status_err(PC_ERR_ARGUMENT, "malformed FORM_FLATTEN JSON");
    }
    size_t n = pc_json_array_size(arr);
    if (n) {
      c.flat_fields = static_cast<IrFormField*>(std::malloc(n * sizeof(IrFormField)));
      if (!c.flat_fields) {
        return status_err(PC_ERR_MEMORY, "OOM parsing flattened fields");
      }
      for (size_t i = 0; i < n; ++i) {
        const pc_json_value* fo = pc_json_array_at(arr, i);
        if (!fo || pc_json_get_type(fo) != PC_JSON_OBJECT) {
          return status_err(PC_ERR_ARGUMENT, "malformed FORM_FLATTEN JSON");
        }
        IrFormField& f = c.flat_fields[i];
        const char* s = pc_json_as_string(pc_json_object_get(fo, "name"));
        if (s) {
          std::strncpy(f.name, s, sizeof(f.name) - 1);
        }
        s = pc_json_as_string(pc_json_object_get(fo, "value"));
        if (s) {
          std::strncpy(f.value, s, sizeof(f.value) - 1);
        }
        s = pc_json_as_string(pc_json_object_get(fo, "default_value"));
        if (s) {
          std::strncpy(f.default_value, s, sizeof(f.default_value) - 1);
        }
        s = pc_json_as_string(pc_json_object_get(fo, "format"));
        if (s) {
          std::strncpy(f.format, s, sizeof(f.format) - 1);
        }
        f.type = static_cast<uint32_t>(pc_json_as_int(pc_json_object_get(fo, "type")));
        f.max_len = static_cast<uint32_t>(pc_json_as_int(pc_json_object_get(fo, "max_len")));
        f.flags = static_cast<uint32_t>(pc_json_as_int(pc_json_object_get(fo, "flags")));
        f.page_index = static_cast<uint32_t>(pc_json_as_int(pc_json_object_get(fo, "page_index")));
        f.rect = rect_from_json(pc_json_object_get(fo, "rect"));
      }
    }
    c.flat_count = static_cast<uint32_t>(n);
  }
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
    pc_command pc_cmd = {
        sizeof(pc_command), static_cast<pc_command_type>(c.type), "", c.before, c.after, "", ""};
    std::strncpy(pc_cmd.annotation_id, c.id, sizeof(pc_cmd.annotation_id) - 1);
    pc_cmd.annotation_id[sizeof(pc_cmd.annotation_id) - 1] = '\0';
    if (c.type == Command::FORM_SET) {
      // The form payload rides on the command itself; without this copy the replayed
      // apply would look up an empty field name and fail (story 7.2).
      std::strncpy(pc_cmd.form_field_name, c.form_field_name, sizeof(pc_cmd.form_field_name) - 1);
      pc_cmd.form_field_name[sizeof(pc_cmd.form_field_name) - 1] = '\0';
      std::strncpy(pc_cmd.form_new_value, c.form_new_value, sizeof(pc_cmd.form_new_value) - 1);
      pc_cmd.form_new_value[sizeof(pc_cmd.form_new_value) - 1] = '\0';
    }
    s = pc_txn_apply(txn, &pc_cmd);
    if (s.code != PC_ERR_NONE) {
      return s;
    }
    // Restore unknown keys onto the applied command (apply built a fresh Command).
    if (c.extras && !txn->undo.empty()) {
      pc_json_free(txn->undo.back().extras);
      txn->undo.back().extras = pc_json_clone(c.extras);
    }
    // The FORM_FLATTEN snapshot must also ride back onto the applied command: apply
    // stole the doc's CURRENT fields (possibly a different set than the log recorded),
    // while undo of the replayed log promises the snapshot's fields (story 7.4).
    if (c.type == Command::FORM_FLATTEN && !txn->undo.empty()) {
      std::free(txn->undo.back().flat_fields);
      txn->undo.back().flat_fields = nullptr;
      txn->undo.back().flat_count = c.flat_count;
      if (c.flat_count) {
        txn->undo.back().flat_fields =
            static_cast<IrFormField*>(std::malloc(c.flat_count * sizeof(IrFormField)));
        if (txn->undo.back().flat_fields) {
          for (uint32_t i = 0; i < c.flat_count; ++i) {
            txn->undo.back().flat_fields[i] = c.flat_fields[i];
          }
        } else {
          txn->undo.back().flat_count = 0;
        }
      }
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