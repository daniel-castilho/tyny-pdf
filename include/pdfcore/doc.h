#ifndef PDFCORE_DOC_H
#define PDFCORE_DOC_H

#include <stddef.h>
#include <stdint.h>

#include "geom.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_doc pc_doc;

/// Open a document through the active backend and build the IR (R7.1). Returns PC_ERR_NONE and
/// sets *out_doc, or PC_ERR_IO, PC_ERR_PASSWORD, PC_ERR_CORRUPT, PC_ERR_MEMORY. The caller owns
/// *out_doc and must release it with pc_doc_close.
pc_status pc_doc_open(const char* path, const char* password, pc_doc** out_doc);

/// Page count recorded in the IR at open time (R7.2).
uint32_t pc_doc_page_count(const pc_doc* doc);

/// MediaBox/CropBox/rotation of one page (R7.3). PC_ERR_NONE or PC_ERR_RANGE.
pc_status pc_doc_page_get_box(const pc_doc* doc, uint32_t page_index, pc_page_box* out);

/// Hex-encoded SHA-256 of the source file, stable across opens (R7.4).
const char* pc_doc_sha256(const pc_doc* doc);

/// Free the IR only; engine memory belongs to the backend (R7.5, R-M6).
void pc_doc_close(pc_doc* doc);

/// R-M5: 1 when the backend declares `capability`, 0 otherwise. Never a guess, so an
/// unsupported capability is reported as unsupported and not as an empty result.
int pc_doc_has_capability(const pc_doc* doc, uint32_t capability);

/// R-M5: fill out_rects (up to capacity elements) and set *out_count. When the backend does not
/// declare the capability, returns PC_ERR_CAPABILITY with *out_count = 0. PC_ERR_ARGUMENT for
/// null out_count.
pc_status pc_doc_find_tables(const pc_doc* doc, pc_rect* out_rects, size_t* out_count,
                             size_t capacity);

#ifdef __cplusplus
}
#endif

#endif