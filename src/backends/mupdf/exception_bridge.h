#ifndef PDFCORE_MUPDF_EXCEPTION_BRIDGE_H
#define PDFCORE_MUPDF_EXCEPTION_BRIDGE_H
// The ONLY file allowed to contain fz_try/fz_catch/fz_always (grep gate, ADR-0011 R-M2 §3.5).
// Engine exceptions are translated into pc_status exactly here (ADR-0003: hostile input reports,
// never traps).
#include <mupdf/fitz.h>

#include <utility>

#include "pdfcore/status.h"

namespace mupdf {
template <typename Body>
pc_status run_guarded(fz_context* ctx, Body&& body, const char* fallback_detail) {
  pc_status result{sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  fz_var(result);
  fz_try(ctx) {
    result = body();
  }
  fz_catch(ctx) {
    result = {sizeof(pc_status), PC_ERR_CORRUPT, 0, fallback_detail};
    const char* msg = fz_caught_message(ctx);
    if (msg && *msg)
      result.detail = msg;
  }
  return result;
}
}  // namespace mupdf

#endif