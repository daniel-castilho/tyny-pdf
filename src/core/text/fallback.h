#ifndef PDFCORE_TEXT_FALLBACK_H
#define PDFCORE_TEXT_FALLBACK_H

#include <stdint.h>

#include "pdfcore/backend.h"
#include "pdfcore/status.h"
#include "pdfcore/text.h"

namespace pdfcore {

// R20.1/R20.2: split a UTF-8 string into font-fallback runs, per the public pc_text_fallback_runs
// contract. Lives in src/core (R-M8): the policy is ours, the engine only answers face coverage.
// All-or-nothing on missing glyphs - a hostile PDF reports PC_ERR_LIMIT, never partial runs.
pc_status fallback_runs(const pc_backend_api& backend, void* backend_doc, const char* utf8,
                        pc_text_run** out_runs, uint32_t* out_count);

void run_free(pc_text_run* runs, uint32_t count);

}  // namespace pdfcore

#endif