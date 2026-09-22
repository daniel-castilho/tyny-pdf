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

/// One grapheme cluster start in a UTF-8 buffer: the byte offset where a UAX
/// #29 extended grapheme cluster begins.
///
/// Story 4.4 (R21.x): break positions over the golden pt-BR corpus. The array
/// is ascending, caller-owned, and freed with pc_text_boundary_free.
typedef struct pc_text_boundary {
  uint32_t byte_offset;    ///< byte offset where the cluster starts
  uint32_t cluster_index;  ///< 0-based index of the cluster
} pc_text_boundary;

/// Split `utf8` into UAX #29 extended grapheme clusters (R21.1) with the pt-BR
/// exceptions the golden corpus pins (R21.2): a combining sequence
/// (`e + U+0301`) is ONE cluster, never two; a letter followed by a hyphen
/// (`guarda-`) keeps the hyphen in the cluster; an ABNT2 dead-key composition
/// (`~ + a` -> `a with tilde`) is one cluster (R23.3). Invalid UTF-8 is
/// rejected as a whole, never half-decoded into a plausible wrong boundary
/// (ADR-0003).
///
/// Ownership: `*out_boundaries` is a caller-owned array with one entry per
/// cluster (the byte offset where that cluster starts); release it with
/// pc_text_boundary_free. For a non-empty buffer the first entry is always
/// byte_offset 0; an empty buffer returns PC_ERR_NONE with zero entries and a
/// null array.
///
/// Errors:
///  - PC_ERR_ARGUMENT: null utf8 / out_boundaries / out_count.
///  - PC_ERR_ARGUMENT: invalid UTF-8 (overlong, surrogate, truncated or out
///    of range).
pc_status pc_text_break_positions(const char* utf8, uint32_t utf8_len,
                                  pc_text_boundary** out_boundaries, uint32_t* out_count);

/// Free an array returned by pc_text_break_positions. Safe with NULL.
void pc_text_boundary_free(pc_text_boundary* boundaries, uint32_t count);

/// Move the caret one grapheme cluster left from `byte_pos` (R23.1): a
/// combining sequence (`e + U+0301`) is crossed in ONE step, not one step per
/// codepoint (R23.2), and an ABNT2 `~ + a` composition is never split (R23.3).
/// At byte 0 the answer is 0 (the caret stays at the start).
///
/// Errors:
///  - PC_ERR_ARGUMENT: null utf8 / out_left, or byte_pos beyond the buffer.
///  - PC_ERR_ARGUMENT: invalid UTF-8.
pc_status pc_caret_left(const char* utf8, uint32_t utf8_len, uint32_t byte_pos, uint32_t* out_left);

/// Move the caret one grapheme cluster right, the mirror of pc_caret_left.
/// At the end of the buffer `*out_right` equals utf8_len (no overflow).
pc_status pc_caret_right(const char* utf8, uint32_t utf8_len, uint32_t byte_pos,
                         uint32_t* out_right);

#ifdef __cplusplus
}
#endif

#endif