#ifndef TYNY_CORE_DOC_TRANSACTION_H
#define TYNY_CORE_DOC_TRANSACTION_H

#include <cstdint>
#include <cstring>
#include <vector>

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/json.h"

// Minimal annotation element in the IR: identity plus geometry only. Form rules, page
// anchoring and rendering extras are D-2 (Epic 5/6); the undo log only needs id + rect today
// (R18.1, R-M8).
struct IrAnnotation {
  char id[11];  // RFC 4648 base32, 10 chars + NUL (R2.3, pc_sidecar_validate_annotation_id)
  pc_rect rect;
};

// Form field in the IR (R46.1): value types only, no engine pointers.
struct IrFormField {
  char name[128];           // UTF-8 field name, null-terminated
  char value[512];          // UTF-8 field value, null-terminated
  char default_value[512];  // UTF-8 default value, null-terminated
  uint32_t max_len;         // 0 = unlimited
  char format[128];         // Format string (e.g., "AFNumber_Keystroke")
  uint32_t flags;
  uint32_t type;  // pc_form_field_type
  uint32_t page_index;
  pc_rect rect;
};

// The document IR, shared by doc.cc (construction, ownership) and transaction.cc (mutation,
// hash). Single source of truth; transaction code borrows the doc and never owns it.
struct pc_doc {
  const pc_backend_api* backend;  // capability queries only; never reached for IR mutation
  void* backend_doc;              // engine handle, closed at pc_doc_open, stays null
  uint32_t page_count;
  pc_page_box* page_boxes;
  char sha256_hex[65];

  uint32_t annotation_count;
  uint32_t annotation_cap;
  IrAnnotation* annotations;

  uint32_t form_field_count;
  uint32_t form_field_cap;
  IrFormField* form_fields;
};

// Find a form field in the IR by name. Shared by transaction.cc (apply/undo) and forms.cc
// (fill/validate), story 7.2.
inline IrFormField* ir_form_field_find(pc_doc* doc, const char* name) {
  if (!doc || !name) {
    return nullptr;
  }
  for (uint32_t i = 0; i < doc->form_field_count; ++i) {
    if (std::strcmp(doc->form_fields[i].name, name) == 0) {
      return &doc->form_fields[i];
    }
  }
  return nullptr;
}

// One logged mutation. `before` is the IR snapshot captured at apply time (zero rect for
// ADD, meaning the element did not exist); `after` is the target state. Value types only,
// never an engine handle (R-M4, R-M8). Shared by transaction.cc and forms.cc (story 7.2).
struct Command {
  enum Type {
    ADD_ANNOT = 1,
    MOVE = 2,
    DELETE = 3,
    FORM_SET = 4,
    FORM_FLATTEN = 5
  } type = ADD_ANNOT;
  char id[11] = {};               // annotation id; empty for FORM_SET
  pc_rect before = {};            // annotation rect before
  pc_rect after = {};             // annotation rect after
  char form_field_name[64] = {};  // FORM_SET: target field name
  char form_old_value[512] = {};  // FORM_SET: value captured at apply time
  char form_new_value[512] = {};  // FORM_SET: target value
  // FORM_FLATTEN: the IR's field array, stolen at apply time so undo restores
  // editability in O(1) without a copy (story 7.4). Heap value types; owned.
  IrFormField* flat_fields = nullptr;
  uint32_t flat_count = 0;
  pc_json_value* extras = nullptr;  // unknown JSON keys, owned

  Command() = default;
  Command(const Command& o);
  Command& operator=(const Command& o);
  Command(Command&& o) noexcept;
  Command& operator=(Command&& o) noexcept;
  ~Command();
};

struct pc_txn {
  pc_doc* doc;
  std::vector<Command> undo;
  std::vector<Command> redo;
  uint32_t max_tiles;   // 0 = unlimited
  uint64_t max_bytes;   // 0 = unlimited
  uint64_t undo_bytes;  // running estimate of the undo stack
  pc_json_value* extras = nullptr;

  ~pc_txn();
};

#endif