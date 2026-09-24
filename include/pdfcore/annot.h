// Annotation module for Tyny PDF — story 6.4.
// Platform-neutral C ABI: annotations as commands in the transaction log.

#pragma once

#include <pdfcore/geom.h>
#include <pdfcore/selection.h>
#include <pdfcore/status.h>
#include <pdfcore/transaction.h>

#ifdef __cplusplus
extern "C" {
#endif

// Annotation types (matches PDF annotation subtypes)
typedef enum pc_annot_type {
  PC_ANNOT_HIGHLIGHT = 0,
  PC_ANNOT_UNDERLINE = 1,
  PC_ANNOT_STRIKETHROUGH = 2,
  PC_ANNOT_SQUIGGLY = 3,
  PC_ANNOT_FREETEXT = 4,
  PC_ANNOT_LINK = 5,
} pc_annot_type;

// One annotation: geometry in page user-space points, color (sRGB), and payload.
// The annotation_id in pc_command.annotation_id carries the 10-char base32 ID.
typedef struct pc_annot {
  pc_annot_type type;
  uint32_t page_index;   // target page
  pc_rect rect;          // annotation rectangle in page user-space points
  pc_quad quad;          // for text markup: union of hit quads
  uint32_t byte_offset;  // for text markup: start byte in page text
  uint32_t byte_len;     // for text markup: length in bytes
  uint8_t color[3];      // sRGB color (0..255 each)
  uint32_t flags;        // PDF annotation flags (hidden, print, etc.)
  // Opaque payload for type-specific data (link URL, freetext content, etc.)
  void* payload;
  uint32_t payload_size;
} pc_annot;

// Build a pc_command for adding an annotation from a selection.
// The command's annotation_id is generated and written into cmd->annotation_id.
// Returns PC_ERR_NONE on success, PC_ERR_ARGUMENT for null pointers.
pc_status pc_annot_cmd_add_from_selection(const pc_selection_result* selection,
                                          const uint8_t color[3], pc_annot_type type,
                                          pc_command* cmd);

// Build a pc_command for deleting an annotation by its 10-char base32 ID.
pc_status pc_annot_cmd_delete(const char annotation_id[11], pc_command* cmd);

// Build a pc_command for modifying an annotation (rect, color, flags).
// The annotation is identified by its 10-char base32 ID in annotation_id.
pc_status pc_annot_cmd_modify(const char annotation_id[11], const pc_rect* new_rect,
                              const uint8_t color[3], uint32_t flags, pc_command* cmd);

// Opaque annotation list for enumeration (caller frees with pc_annot_list_free).
typedef struct pc_annot_list {
  pc_annot* items;
  uint32_t count;
} pc_annot_list;

// Get all annotations on a page (read-only, uses IR, no transaction).
// Returns PC_ERR_CAPABILITY if backend doesn't support annotations.
pc_status pc_annot_list_page(const pc_backend_api* api, void* backend_page,
                             pc_annot_list* out_list);

// Free list returned by pc_annot_list_page.
void pc_annot_list_free(pc_annot_list* list);

// Export annotations to sidecar (appends to the sidecar JSON).
pc_status pc_annot_export_sidecar(const pc_backend_api* api, void* backend_doc,
                                  void* sidecar_writer);

// Import annotations from sidecar (replays transaction log via pc_txn_from_json).
pc_status pc_annot_import_sidecar(const pc_backend_api* api, void* backend_doc,
                                  const void* sidecar_reader, pc_txn* txn);

// R44.1 (story 6.5) - re-anchor annotation after document text changes.
// old_offset/old_len: original byte range in the page's text.
// out_offset/out_len: new byte range after re-anchoring.
// out_confidence: 0.0-1.0, based on text match quality.
// out_detached: 1 if confidence < 0.5 (annotation flagged as detached).
// Returns PC_ERR_NONE, PC_ERR_ARGUMENT, PC_ERR_CAPABILITY, PC_ERR_MEMORY.
pc_status pc_annot_reanchor(const pc_backend_api* api, void* backend_page, uint32_t old_offset,
                            uint32_t old_len, uint32_t* out_offset, uint32_t* out_len,
                            float* out_confidence, int* out_detached);

#ifdef __cplusplus
}
#endif