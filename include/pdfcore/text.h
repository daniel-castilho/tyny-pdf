#ifndef PDFCORE_TEXT_H
#define PDFCORE_TEXT_H

#include <stdint.h>

#include "backend.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/// One font-fallback run: a maximal substring whose codepoints are all covered by `face`
/// (the first face in the backend's order that draws them). `utf8` is caller-owned memory
/// returned by pc_text_fallback_runs and freed with pc_text_run_free, never with free() (R-M6).
typedef struct pc_text_run {
  uint32_t size;
  uint32_t face;
  char* utf8;
  uint32_t byte_len;
} pc_text_run;

/// Split `utf8` into font-fallback runs (R20.1): every codepoint is UTF-8 decoded, then each
/// face in the backend's declaration order is asked whether it draws it; the first face that
/// says yes owns the run, and a maximal run of codepoints with the same winning face is emitted.
///
/// Returns PC_ERR_NONE with *out_runs (caller-owned, freed with pc_text_run_free) and
/// *out_count set. The caller owns *out_runs.
///
/// Failure is all-or-nothing: a codepoint no face draws returns PC_ERR_LIMIT with a detail of
/// "missing glyph U+XXXX" and leaves *out_runs NULL (no partial runs, never tofu) (R20.2).
///
/// Errors:
///  - PC_ERR_ARGUMENT: null backend / backend_doc / utf8 / out_runs / out_count, or invalid UTF-8.
///  - PC_ERR_CAPABILITY: the backend does not declare PC_CAP_FACE_COVERAGE, or declares zero faces
///    (R-M5).
///  - PC_ERR_LIMIT: at least one codepoint has no covering face; detail names the first one.
pc_status pc_text_fallback_runs(const pc_backend_api* backend, void* backend_doc, const char* utf8,
                                pc_text_run** out_runs, uint32_t* out_count);

/// Free an array returned by pc_text_fallback_runs: each run's utf8, then the array. Safe with
/// NULL runs (byte_len is trusted only for runs this array owns).
void pc_text_run_free(pc_text_run* runs, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif