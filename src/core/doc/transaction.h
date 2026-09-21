#ifndef TYNY_CORE_DOC_TRANSACTION_H
#define TYNY_CORE_DOC_TRANSACTION_H

#include <cstdint>

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"

// Minimal annotation element in the IR: identity plus geometry only. Form rules, page
// anchoring and rendering extras are D-2 (Epic 5/6); the undo log only needs id + rect today
// (R18.1, R-M8).
struct IrAnnotation {
  char id[11];  // RFC 4648 base32, 10 chars + NUL (R2.3, pc_sidecar_validate_annotation_id)
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
};

#endif