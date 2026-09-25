#ifndef PDFCORE_BACKEND_H
#define PDFCORE_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#include "doc.h"
#include "forms.h"
#include "page.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Frozen backend vtable ABI (ADR-0011 R-M3). Entries are append-only; bumping abi_major is an
/// explicit project decision that ships a migration note.
/// abi 1.2 (story 6.1): appended PC_CAP_TEXT_LAYOUT + page_text_layout/page_text_layout_free.
/// abi 1.3 (story 7.1): appended PC_CAP_FORMS + form_list_fields/form_list_free/
/// form_fdf_export/form_fdf_import/form_fdf_free.
/// abi 1.4 (story 7.4): appended form_flatten (bake widgets + save) and
/// pc_render_params.render_widgets.
#define PC_BACKEND_API_VERSION_MAJOR 1
#define PC_BACKEND_API_VERSION_MINOR 4

/// Document capabilities a backend declares rather than guesses (R-M5). A backend that does not
/// declare PC_CAP_TABLES MUST answer doc_find_tables with PC_ERR_CAPABILITY, never an empty list.
#define PC_CAP_TABLES 1u
/// Declares font fallback coverage: face_count lists the probe faces and face_coverage answers
/// per face whether it draws a codepoint. A backend that does not declare PC_CAP_FACE_COVERAGE
/// MUST answer face_count with 0 (R-M5) - the caller reports "not supported", never tofu.
#define PC_CAP_FACE_COVERAGE 2u
/// Declares text layout extraction (abi 1.2, story 6.1): page_text_layout returns one page's
/// text as UTF-8 plus per-cluster quads in user-space points, copied out into our value types
/// (R-M4). A backend that does not declare it MUST answer page_text_layout with
/// PC_ERR_CAPABILITY (R-M5) - the caller reports "not supported", never an empty layout.
#define PC_CAP_TEXT_LAYOUT 3u
/// Declares AcroForm support (abi 1.3, story 7.1). The backend provides field enumeration,
/// FDF export/import, and field value operations.
#define PC_CAP_FORMS 4u

typedef struct pc_backend_api pc_backend_api;

struct pc_backend_api {
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t struct_size;

  /// Open a document. Returns PC_ERR_NONE with *out_backend_doc set, else PC_ERR_IO,
  /// PC_ERR_PASSWORD, PC_ERR_CORRUPT or PC_ERR_MEMORY. The caller owns *out_backend_doc
  /// until doc_close (R-M2, R-M6).
  pc_status (*doc_open)(const char* path, const char* password, void** out_backend_doc);
  /// Close a document and free every engine object it owns (R-M6).
  void (*doc_close)(void* backend_doc);
  /// Page count. A non-positive result means 0 pages; errors are reported at open time.
  uint32_t (*doc_page_count)(void* backend_doc);
  /// Load one page into *out_backend_page. PC_ERR_NONE, PC_ERR_RANGE, PC_ERR_MEMORY,
  /// PC_ERR_CORRUPT.
  pc_status (*page_get)(void* backend_doc, uint32_t index, void** out_backend_page);
  /// Page box without a persistent page handle. PC_ERR_NONE, PC_ERR_RANGE, PC_ERR_ARGUMENT.
  pc_status (*page_get_box)(void* backend_doc, uint32_t index, pc_page_box* out);
  /// Render into the caller-owned pc_pixmap. PC_ERR_NONE, PC_ERR_ARGUMENT, PC_ERR_MEMORY,
  /// PC_ERR_CORRUPT. The pixmap buffer is freed with pixmap_free, never with free() (R-M6).
  pc_status (*page_render)(void* backend_page, const pc_render_params* params,
                           pc_pixmap* out_pixmap);
  void (*page_free)(void* backend_page);
  void (*pixmap_free)(pc_pixmap* pixmap);
  const char* (*get_last_error)(void* backend_doc);

  /// R-M5 capability declaration. Returns 1 when the backend supports `capability`, 0 otherwise.
  /// Added at abi 1.1; a struct with struct_size below that offset reports 0 (no capabilities).
  int (*doc_has_capability)(void* backend_doc, uint32_t capability);
  /// R-M5 capability use. An unsupported capability returns PC_ERR_CAPABILITY with *out_count
  /// left at 0 - the caller shows "not supported by this backend", never an empty "no tables".
  /// Supported capabilities return PC_ERR_NONE. PC_ERR_ARGUMENT for null out_count.
  pc_status (*doc_find_tables)(void* backend_doc, pc_rect* out_rects, size_t* out_count,
                               size_t capacity);

  /// Face count for font fallback (R-M5, PC_CAP_FACE_COVERAGE). Returns 0 when the backend
  /// declares no face coverage. The faces are backend-defined, 0-based, stable per build, and
  /// every entry names the index in detail strings.
  uint32_t (*face_count)(void* backend_doc);
  /// Per-face codepoint coverage. `out_has` is 1 when `face` draws `codepoint`, 0 otherwise.
  /// PC_ERR_NONE, PC_ERR_ARGUMENT for null out_has or a non-negative face index, PC_ERR_RANGE
  /// for an out-of-range face (>= face_count). Added at abi 1.1.
  pc_status (*face_coverage)(void* backend_doc, uint32_t face, uint32_t codepoint, int* out_has);

  /// Text layout of one page (abi 1.2, story 6.1, PC_CAP_TEXT_LAYOUT). Sets *out_utf8 to a
  /// NUL-terminated buffer holding the page's text and *out_boxes to one pc_text_box per
  /// cluster, with byte ranges indexing *out_utf8. Both buffers are backend memory; the caller
  /// copies what it keeps and releases both with page_text_layout_free (R-M6). Quads are in
  /// user-space points, untransformed by DPI (the caller scales via pc_page_box, R8.1).
  /// PC_ERR_NONE, PC_ERR_ARGUMENT, PC_ERR_MEMORY, PC_ERR_CORRUPT, or PC_ERR_CAPABILITY when
  /// the backend does not declare PC_CAP_TEXT_LAYOUT (R-M5) - never an empty layout.
  pc_status (*page_text_layout)(void* backend_page, char** out_utf8, pc_text_box** out_boxes,
                                uint32_t* out_count);
  /// Release both buffers returned by page_text_layout. Safe with NULLs (R-M6: the backend
  /// allocated them, the backend frees them). Added at abi 1.2.
  void (*page_text_layout_free)(char* utf8, pc_text_box* boxes);

  /// Form fields enumeration (abi 1.3, story 7.1). Returns all form fields in the document.
  /// PC_ERR_NONE, PC_ERR_ARGUMENT, PC_ERR_CAPABILITY when backend doesn't support forms.
  pc_status (*form_list_fields)(const pc_backend_api* api, void* backend_doc,
                                pc_form_list* out_list);
  /// Free a form list returned by form_list_fields. Safe with NULL.
  void (*form_list_free)(pc_form_list* list);

  /// Export form fields to FDF (Forms Data Format).
  /// Returns PC_ERR_NONE with *out_fdf set to malloc'd UTF-8, PC_ERR_ARGUMENT for null args,
  /// PC_ERR_CAPABILITY when backend doesn't support forms.
  pc_status (*form_fdf_export)(const pc_backend_api* api, void* backend_doc, pc_fdf* out_fdf);
  /// Import form fields from FDF.
  /// Returns PC_ERR_NONE, PC_ERR_ARGUMENT for invalid FDF, PC_ERR_CAPABILITY when unsupported.
  pc_status (*form_fdf_import)(const pc_backend_api* api, void* backend_doc, const char* fdf_data,
                               size_t fdf_size);
  /// Free FDF data returned by form_fdf_export. Safe with NULL.
  void (*form_fdf_free)(pc_fdf* fdf);
  /// abi 1.4 (story 7.4): bake widget appearances into static page content, remove the
  /// interactive form objects, and save the result to out_path (a NEW file - the source is
  /// never modified in place). PC_ERR_NONE, PC_ERR_CAPABILITY when the backend does not
  /// declare PC_CAP_FORMS or the document is not a PDF, PC_ERR_ARGUMENT, PC_ERR_IO.
  pc_status (*form_flatten)(const pc_backend_api* api, void* backend_doc, const char* out_path);
};

#define PC_BACKEND_API_INIT \
  { PC_BACKEND_API_VERSION_MAJOR, PC_BACKEND_API_VERSION_MINOR, sizeof(pc_backend_api) }

extern pc_backend_api pc_null_backend_api;
const pc_backend_api* pc_null_backend_get_api(void);

extern pc_backend_api pc_mupdf_backend_api;
const pc_backend_api* pc_mupdf_backend_get_api(void);

#ifdef __cplusplus
}
#endif

#endif