// Platform-neutral viewer loop (story 1.5, R30.1).
// src/app/viewer/viewer.h
// No Windows header and no engine header (ADR-0011 R-M10): this TU compiles
// on Linux, where headless tests exercise the same code path the Windows
// viewer runs. The D2D blit of a tile lives in d2d_blit.cc (win-only).

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pdfcore/backend.h"
#include "pdfcore/budget.h"
#include "pdfcore/forms.h"
#include "pdfcore/render.h"
#include "pdfcore/selection.h"
#include "pdfcore/status.h"
#include "pdfcore/transaction.h"  // full pc_budget definition (value type, budget.h)
#include "pdfcore/uia.h"

namespace tynypdf {
namespace viewer {

// Mirror of the opaque pc_tile layout in src/render/tiles/tiles.cc. The ABI
// keeps pc_tile opaque (include/pdfcore/render.h), so the viewer replicates
// the struct exactly, as tests/unit/test_tiles.cc already does (R-M12). Keep
// the two definitions in step; tiles.cc documents that the layout is pinned.
struct pc_tile_mirror {
  uint32_t x;
  uint32_t y;
  uint32_t zoom;
  uint32_t ref_count;
  uint64_t size_bytes;
  uint64_t last_access;
  void* data;
};

// Header at the start of every tile->data buffer held by the cache. The
// buffer is malloc'd by the viewer, so cache eviction may free() it (R-M6:
// engine buffers are never cached; page_render output is copied out first).
// `page_gen` is the viewer's page generation: it changes exactly when the
// tile's content changes (a page flip resets the cache), so the blit layer
// can upload only on change and blit every frame after (R15.1).
struct tile_payload {
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint32_t page_gen;
};

// Viewer state. The Windows composition root owns one of these and the
// window's user_data points at it; headless tests own one too.
struct viewer_state {
  const pc_backend_api* api = nullptr;
  void* doc = nullptr;
  uint32_t page_count = 0;
  uint32_t dpi = 72;
  uint32_t max_tiles = 256;  // pc_budget.max_tiles ceiling (0 = unlimited)
  uint64_t max_bytes = 0;    // pc_budget.max_bytes ceiling (0 = unlimited)
  pc_tile_cache* cache = nullptr;
  pc_cachemap* map = nullptr;
  pc_budget budget = {};              // value type, caller-allocated POD (budget.h)
  uint32_t cached_tiles = 0;          // tiles currently cached (bound for the budget)
  uint64_t tiles_rendered = 0;        // page_render calls since open (cache misses)
  uint32_t active_page = UINT32_MAX;  // last demanded page; reset tiles on change
  uint32_t page_gen = 0;              // bumped on every tile reset; stamps tile_payload

  // Story 6.1 (R32.1, R32.4): selection state updated by click handler
  pc_selection_result selection = {};
  bool has_selection = false;

  // Story 6.2 (R33.2): caret position (byte offset in page text)
  uint32_t caret_pos = 0;
  bool has_caret = false;

  // Story 6.2 (R34.1): selection anchor for extend (byte offset where drag started)
  uint32_t selection_anchor = 0;

  // Story 7.3 (R51.x): forms state. The IR (pc_doc) is synthesized by pc_doc_open and
  // its fields loaded through the backend vtable (R48.2); fill/toggle run through the
  // same transaction log as every other mutation (R48.3). No fields -> Tab falls
  // through to the caret keys unchanged.
  pc_doc* ir = nullptr;
  pc_txn* txn = nullptr;
  pc_form_focus form_focus = {};
};

// Open `path` through `api` and allocate the cache, cachemap and budget.
// PC_ERR_NONE, PC_ERR_IO, PC_ERR_PASSWORD, PC_ERR_CORRUPT, PC_ERR_MEMORY.
pc_status viewer_open(viewer_state* st, const pc_backend_api* api, const char* path, uint32_t dpi,
                      uint32_t max_tiles, uint64_t max_bytes);

// Demand an cols x rows grid of tiles starting at (col0, row0) of `page`.
// Tiles already cached are hit; misses are rendered and inserted. Tile keys
// carry no page dimension, so demanding a different page first discards the
// whole cache (the previous page's tiles would otherwise alias the new page's
// coords). The budget ceiling is checked before an insert: PC_ERR_LIMIT leaves
// the strip partially filled rather than over-committing memory (R30.2's
// ceiling is enforced here and measured externally by tools/bench-measure.sh).
pc_status viewer_demand(viewer_state* st, uint32_t page, int32_t col0, int32_t row0, uint32_t cols,
                        uint32_t rows);

// Draw the same grid through `draw`, once per tile that is present in the
// cachemap. `user_data` is passed through to the draw callback.
typedef pc_status (*viewer_draw_fn)(const tile_payload* payload, int32_t col, int32_t row,
                                    void* user_data);
pc_status viewer_draw(viewer_state* st, uint32_t page, int32_t col0, int32_t row0, uint32_t cols,
                      uint32_t rows, viewer_draw_fn draw, void* user_data);

// Release the cache, cachemap and document.
void viewer_close(viewer_state* st);

// Click handler: converts device-space click to page-space hit-test via
// pc_selection_hit_test (R32.1). Updates st->selection and st->has_selection.
// Returns PC_ERR_NONE on hit, PC_ERR_RANGE on miss, PC_ERR_ARGUMENT for null.
pc_status viewer_on_click(viewer_state* st, uint32_t page, int x, int y, int modifiers);

// Keyboard handler: converts virtual key to caret/selection action (R33.2, R34.1).
// vk: Windows virtual-key code (VK_LEFT, VK_RIGHT, VK_HOME, VK_END, VK_CONTROL, VK_SHIFT).
// down: 1=press, 0=release. modifiers: bit 0=Shift, 1=Ctrl, 2=Alt.
// Story 7.3 (R51.1): VK_TAB commits the typing buffer and moves focus (Shift+TAB moves
// back), VK_SPACE toggles a checkbox (or types a space into a text field), VK_ESCAPE
// discards the buffer. These are handled before the caret keys and need no page text.
// Returns PC_ERR_NONE on handled, PC_ERR_ARGUMENT for null.
pc_status viewer_on_key(viewer_state* st, uint32_t page, int vk, int down, int modifiers);

// Text input handler (R51.1): one Unicode codepoint (from WM_CHAR) typed into the
// focused text field's buffer. PC_ERR_NONE, PC_ERR_LIMIT when the field's max_len
// rejects the character, PC_ERR_STATE when no text field is focused.
pc_status viewer_on_char(viewer_state* st, uint32_t codepoint);

// The forms-focus a11y announcement (R51.2): the exact string pc_form_focus_announce
// produces for the focused field, converted to UTF-16. PC_ERR_STATE when no field is
// focused - the caller keeps announcing the page state (R24.7) instead. out_cap is in
// wchar_t units; PC_UIA_ANNOUNCE_MAX is the intended capacity.
pc_status viewer_forms_announcement(viewer_state* st, wchar_t* out, size_t out_cap);

}  // namespace viewer
}  // namespace tynypdf