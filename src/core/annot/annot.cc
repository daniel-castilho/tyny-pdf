// R41.x (abi 1.2, story 6.4) - annotations as commands in the transaction log.
// The IR layer owns annotation semantics; the backend only renders them.

#include "pdfcore/annot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/backend.h"
#include "pdfcore/geom.h"
#include "pdfcore/selection.h"
#include "pdfcore/status.h"
#include "pdfcore/transaction.h"

static inline pc_status ok_status(void) {
  pc_status s = {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  return s;
}

static inline pc_status arg_err(const char* detail) {
  pc_status s = {sizeof(pc_status), PC_ERR_ARGUMENT, 0, detail};
  return s;
}

static inline pc_status cap_err(const char* detail) {
  pc_status s = {sizeof(pc_status), PC_ERR_CAPABILITY, 0, detail};
  return s;
}

// Generate a 10-char base32 ID (RFC 4648, no padding)
static void generate_annot_id(char out[11]) {
  static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  static uint32_t counter = 1;
  uint32_t v = counter++;
  for (int i = 9; i >= 0; --i) {
    out[i] = alphabet[v & 0x1F];
    v >>= 5;
  }
  out[10] = '\0';
}

pc_status pc_annot_cmd_add_from_selection(const pc_selection_result* selection,
                                          const uint8_t color[3], pc_annot_type type,
                                          pc_command* cmd) {
  (void)type;  // stored in IR when command is applied
  if (!selection || !color || !cmd) {
    return arg_err("null argument");
  }

  // Build annotation from selection
  pc_annot annot = {};
  annot.type = type;
  annot.page_index = selection->page_index;
  // Convert quad to rect (bounding box)
  double min_x = selection->quad.ul_x;
  double max_x = selection->quad.ul_x;
  double min_y = selection->quad.ul_y;
  double max_y = selection->quad.ul_y;
  double xs[4] = {selection->quad.ul_x, selection->quad.ur_x, selection->quad.ll_x,
                  selection->quad.lr_x};
  double ys[4] = {selection->quad.ul_y, selection->quad.ur_y, selection->quad.ll_y,
                  selection->quad.lr_y};
  for (int i = 0; i < 4; ++i) {
    if (xs[i] < min_x)
      min_x = xs[i];
    if (xs[i] > max_x)
      max_x = xs[i];
    if (ys[i] < min_y)
      min_y = ys[i];
    if (ys[i] > max_y)
      max_y = ys[i];
  }
  annot.rect = {min_x, min_y, max_x, max_y};
  annot.quad = selection->quad;
  annot.byte_offset = selection->byte_offset;
  annot.byte_len = selection->byte_len;
  memcpy(annot.color, color, 3);
  annot.flags = 0;
  annot.payload = nullptr;
  annot.payload_size = 0;

  // Fill pc_command
  cmd->size = sizeof(pc_command);
  cmd->type = PC_CMD_ADD_ANNOT;
  generate_annot_id(cmd->annotation_id);
  cmd->before = pc_rect{0, 0, 0, 0};  // "did not exist"
  cmd->after = annot.rect;            // target rect
  // Note: other annot fields (color, type, quad, byte range) are not in pc_command
  // They would be stored in the IR separately when pc_txn_apply processes the command.

  return ok_status();
}

pc_status pc_annot_cmd_delete(const char annotation_id[11], pc_command* cmd) {
  if (!annotation_id || !cmd) {
    return arg_err("null argument");
  }

  cmd->size = sizeof(pc_command);
  cmd->type = PC_CMD_DELETE;
  memcpy(cmd->annotation_id, annotation_id, 11);
  cmd->before = pc_rect{0, 0, 0, 0};
  cmd->after = pc_rect{0, 0, 0, 0};

  return ok_status();
}

pc_status pc_annot_cmd_modify(const char annotation_id[11], const pc_rect* new_rect,
                              const uint8_t color[3], uint32_t flags, pc_command* cmd) {
  (void)color;
  (void)flags;  // stored in IR when command is applied
  if (!annotation_id || !cmd) {
    return arg_err("null argument");
  }

  cmd->size = sizeof(pc_command);
  cmd->type = PC_CMD_MOVE;  // Reuse MOVE for modify (rect/color/flags change)
  memcpy(cmd->annotation_id, annotation_id, 11);
  cmd->before = pc_rect{0, 0, 0, 0};
  cmd->after = new_rect ? *new_rect : pc_rect{0, 0, 0, 0};
  // Color/flags not in pc_command - stored in IR separately

  return ok_status();
}

pc_status pc_annot_list_page(const pc_backend_api* api, void* backend_page,
                             pc_annot_list* out_list) {
  (void)api;
  (void)backend_page;
  // Return empty list for now - real implementation reads from IR
  out_list->items = nullptr;
  out_list->count = 0;
  return ok_status();
}

void pc_annot_list_free(pc_annot_list* list) {
  if (list && list->items) {
    free(list->items);
    list->items = nullptr;
    list->count = 0;
  }
}

pc_status pc_annot_export_sidecar(const pc_backend_api* api, void* backend_doc,
                                  void* sidecar_writer) {
  (void)api;
  (void)backend_doc;
  (void)sidecar_writer;
  // Real implementation: iterate all pages, collect annotations, write to sidecar
  return ok_status();
}

pc_status pc_annot_import_sidecar(const pc_backend_api* api, void* backend_doc,
                                  const void* sidecar_reader, pc_txn* txn) {
  (void)api;
  (void)backend_doc;
  (void)sidecar_reader;
  (void)txn;
  // Real implementation: read annotations from sidecar, replay transaction log
  return ok_status();
}

// R44.1 (story 6.5) - re-anchor annotations after document text changes.
// Returns the new byte_offset/byte_len in *out_offset/*out_len and confidence in *out_confidence
// (0.0-1.0). If confidence < 0.5, the annotation is flagged as "detached" (out_detached=1). Uses
// byte offset as primary anchor, then fuzzy text match within ±64 bytes window.
pc_status pc_annot_reanchor(const pc_backend_api* api, void* backend_page, uint32_t old_offset,
                            uint32_t old_len, uint32_t* out_offset, uint32_t* out_len,
                            float* out_confidence, int* out_detached) {
  if (!api || !backend_page || !out_offset || !out_len || !out_confidence || !out_detached) {
    return arg_err("null argument");
  }

  if (!api->page_text_layout || !api->page_text_layout_free) {
    return cap_err("text layout not available");
  }

  char* utf8 = nullptr;
  pc_text_box* boxes = nullptr;
  uint32_t count = 0;
  pc_status s = api->page_text_layout(backend_page, &utf8, &boxes, &count);
  if (s.code != PC_ERR_NONE) {
    if (utf8)
      api->page_text_layout_free(utf8, nullptr);
    if (boxes)
      api->page_text_layout_free(nullptr, boxes);
    return s;
  }

  if (count == 0 || !utf8 || !utf8[0]) {
    if (utf8)
      api->page_text_layout_free(utf8, nullptr);
    if (boxes)
      api->page_text_layout_free(nullptr, boxes);
    *out_offset = old_offset;
    *out_len = old_len;
    *out_confidence = 0.0f;
    *out_detached = 1;
    return ok_status();
  }

  size_t text_len = strlen(utf8);
  if (old_offset > text_len) {
    if (utf8)
      api->page_text_layout_free(utf8, nullptr);
    if (boxes)
      api->page_text_layout_free(nullptr, boxes);
    *out_offset = (uint32_t)text_len;
    *out_len = 0;
    *out_confidence = 0.0f;
    *out_detached = 1;
    return ok_status();
  }

  // Extract old text
  uint32_t old_end = old_offset + old_len;
  if (old_end > text_len)
    old_end = (uint32_t)text_len;
  char* old_text = (char*)malloc(old_len + 1);
  if (!old_text) {
    if (utf8)
      api->page_text_layout_free(utf8, nullptr);
    if (boxes)
      api->page_text_layout_free(nullptr, boxes);
    return arg_err("allocation failed");
  }
  memcpy(old_text, utf8 + old_offset, old_len);
  old_text[old_len] = '\0';

  // Search window: ±64 bytes around old offset, clamped to text bounds
  uint32_t window_start = (old_offset >= 64) ? old_offset - 64 : 0;
  uint32_t window_end = old_end + 64;
  if (window_end > text_len)
    window_end = (uint32_t)text_len;

  // Simple fuzzy match: find best match in window
  uint32_t best_offset = old_offset;
  float best_score = 0.0f;
  uint32_t best_len = old_len;

  for (uint32_t pos = window_start; pos + old_len <= window_end; ++pos) {
    // Quick length check
    if (pos + old_len > text_len)
      break;

    // Score: count matching characters
    float score = 0.0f;
    for (uint32_t i = 0; i < old_len && pos + i < text_len; ++i) {
      if (utf8[pos + i] == old_text[i])
        score += 1.0f;
    }
    score /= (float)old_len;

    if (score > best_score) {
      best_score = score;
      best_offset = pos;
    }
  }

  // Try to extend match to full words/graphemes
  uint32_t best_end = best_offset + old_len;
  if (best_end < text_len && utf8[best_end] != ' ' && utf8[best_end] != '\n') {
    while (best_end < text_len && utf8[best_end] != ' ' && utf8[best_end] != '\n') {
      best_end++;
    }
  }
  if (best_offset > 0 && utf8[best_offset - 1] != ' ' && utf8[best_offset - 1] != '\n') {
    while (best_offset > 0 && utf8[best_offset - 1] != ' ' && utf8[best_offset - 1] != '\n') {
      best_offset--;
    }
  }
  best_len = best_end - best_offset;

  free(old_text);
  api->page_text_layout_free(utf8, boxes);

  *out_offset = best_offset;
  *out_len = best_len;
  *out_confidence = best_score;
  *out_detached = (best_score < 0.5f) ? 1 : 0;

  return ok_status();
}