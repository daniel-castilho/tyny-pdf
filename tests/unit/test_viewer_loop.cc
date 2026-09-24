// Viewer loop contract test (story 1.5) — the platform-neutral present path
// against the null backend, so the same code the Windows viewer runs is
// exercised headless on Linux (ADR-0011 R-M10). Covers:
//   - open/pages, demand-miss-then-hit per page, page-flip invalidation
//   - tile payload shape (256x256 at 72 dpi, stride round-up)
//   - budget ceiling leaves a strip partial instead of over-committing
//   - range/argument errors are reported as pc_status, not traps (ADR-0003)
// R30.1
// tests/unit/test_viewer_loop.cc

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pdfcore/backend.h"
#include "pdfcore/budget.h"
#include "pdfcore/status.h"
#include "viewer/viewer.h"

namespace {

using tynypdf::viewer::pc_tile_mirror;
using tynypdf::viewer::tile_payload;
using tynypdf::viewer::viewer_demand;
using tynypdf::viewer::viewer_draw;
using tynypdf::viewer::viewer_draw_fn;
using tynypdf::viewer::viewer_open;
using tynypdf::viewer::viewer_state;

struct draw_tally {
  int count = 0;
  int bad_dims = 0;
  size_t non_zero = 0;
};

pc_status tally_draw(const tile_payload* payload, int32_t col, int32_t row, void* user_data) {
  (void)col;
  (void)row;
  draw_tally* t = static_cast<draw_tally*>(user_data);
  t->count++;
  if (payload->width != 256 || payload->height != 256 || payload->stride < 256 * 4) {
    t->bad_dims++;
  }
  const uint8_t* px = reinterpret_cast<const uint8_t*>(payload + 1);
  for (uint32_t i = 0; i < payload->height * payload->stride; ++i) {
    if (px[i]) {
      t->non_zero++;
    }
  }
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

int run_happy_path() {
  int errors = 0;
  pc_status st;
  viewer_state st_viewer;
  st = viewer_open(&st_viewer, pc_null_backend_get_api(), "fixture-ignored.pdf", 72, 256, 0);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: open\n");
    return 1;
  }
  if (st_viewer.page_count == 0) {
    fprintf(stderr, "FAIL: page_count\n");
    errors++;
  }

  // Page 0 strip: 3 misses -> 3 renders, 3 cached.
  st = viewer_demand(&st_viewer, 0, 0, 0, 3, 1);
  if (st.code != PC_ERR_NONE || st_viewer.cached_tiles != 3 || st_viewer.tiles_rendered != 3) {
    fprintf(stderr, "FAIL: first demand (cached=%u rendered=%llu)\n", st_viewer.cached_tiles,
            (unsigned long long)st_viewer.tiles_rendered);
    errors++;
  }

  // Same demand again: all hits, no new renders.
  st = viewer_demand(&st_viewer, 0, 0, 0, 3, 1);
  if (st.code != PC_ERR_NONE || st_viewer.cached_tiles != 3 || st_viewer.tiles_rendered != 3) {
    fprintf(stderr, "FAIL: second demand should hit (cached=%u rendered=%llu)\n",
            st_viewer.cached_tiles, (unsigned long long)st_viewer.tiles_rendered);
    errors++;
  }

  // Draw sees the 3 cached tiles with a sane 256x256 payload.
  draw_tally tally;
  st = viewer_draw(&st_viewer, 0, 0, 0, 3, 1, &tally_draw, &tally);
  if (st.code != PC_ERR_NONE || tally.count != 3 || tally.bad_dims != 0) {
    fprintf(stderr, "FAIL: draw (count=%d bad=%d)\n", tally.count, tally.bad_dims);
    errors++;
  }
  if (tally.non_zero == 0) {
    fprintf(stderr, "FAIL: tile bytes all zero\n");
    errors++;
  }

  // Page flip: a different page invalidates the old page's tiles, so the same
  // coords are rendered again (3 fresh renders). Old page data must not alias.
  st = viewer_demand(&st_viewer, 1, 0, 0, 3, 1);
  if (st.code != PC_ERR_NONE || st_viewer.cached_tiles != 3 || st_viewer.tiles_rendered != 6) {
    fprintf(stderr, "FAIL: page flip should re-render (cached=%u rendered=%llu)\n",
            st_viewer.cached_tiles, (unsigned long long)st_viewer.tiles_rendered);
    errors++;
  }

  // Out-of-range page is a reported error, never a trap.
  st = viewer_demand(&st_viewer, 999, 0, 0, 3, 1);
  if (st.code != PC_ERR_RANGE) {
    fprintf(stderr, "FAIL: out-of-range page should be PC_ERR_RANGE (code=%u)\n", st.code);
    errors++;
  }

  viewer_close(&st_viewer);
  return errors;
}

int run_budget_ceiling() {
  int errors = 0;
  pc_status st;
  viewer_state st_viewer;

  // Ceiling of 2 tiles: on a 3-tile strip the third demand is skipped, the
  // strip stays partial, and demand answers NONE (memory over commit is
  // reported, not a stall -- R30.2's ceiling).
  st = viewer_open(&st_viewer, pc_null_backend_get_api(), "fixture-ignored.pdf", 72, 2, 0);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: open (budget)\n");
    return 1;
  }
  st = viewer_demand(&st_viewer, 0, 0, 0, 3, 1);
  if (st.code != PC_ERR_NONE || st_viewer.cached_tiles != 2 || st_viewer.tiles_rendered != 2) {
    fprintf(stderr, "FAIL: ceiling should cap at 2 (cached=%u rendered=%llu)\n",
            st_viewer.cached_tiles, (unsigned long long)st_viewer.tiles_rendered);
    errors++;
  }
  // A re-demand still cannot exceed the ceiling.
  st = viewer_demand(&st_viewer, 0, 0, 0, 3, 1);
  if (st.code != PC_ERR_NONE || st_viewer.cached_tiles != 2) {
    fprintf(stderr, "FAIL: ceiling holds across demands (cached=%u)\n", st_viewer.cached_tiles);
    errors++;
  }
  viewer_close(&st_viewer);

  // max_tiles == 0 means unlimited (budget.h contract).
  st = viewer_open(&st_viewer, pc_null_backend_get_api(), "fixture-ignored.pdf", 72, 0, 0);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: open (unlimited)\n");
    return 1;
  }
  st = viewer_demand(&st_viewer, 0, 0, 0, 3, 1);
  if (st.code != PC_ERR_NONE || st_viewer.cached_tiles != 3) {
    fprintf(stderr, "FAIL: unlimited should cache all 3 (cached=%u)\n", st_viewer.cached_tiles);
    errors++;
  }
  viewer_close(&st_viewer);

  return errors;
}

int run_argument_errors() {
  int errors = 0;
  pc_status st;
  if (viewer_open(nullptr, pc_null_backend_get_api(), "x.pdf", 72, 16, 0).code != PC_ERR_ARGUMENT) {
    fprintf(stderr, "FAIL: null state\n");
    errors++;
  }
  viewer_state st_viewer;
  if (viewer_open(&st_viewer, nullptr, "x.pdf", 72, 16, 0).code != PC_ERR_ARGUMENT) {
    fprintf(stderr, "FAIL: null api\n");
    errors++;
  }
  if (viewer_open(&st_viewer, pc_null_backend_get_api(), nullptr, 72, 16, 0).code !=
      PC_ERR_ARGUMENT) {
    fprintf(stderr, "FAIL: null path\n");
    errors++;
  }
  st = viewer_open(&st_viewer, pc_null_backend_get_api(), "x.pdf", 72, 16, 0);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "FAIL: open\n");
    return errors + 1;
  }
  if (viewer_demand(nullptr, 0, 0, 0, 1, 1).code != PC_ERR_ARGUMENT) {
    fprintf(stderr, "FAIL: demand null state\n");
    errors++;
  }
  if (viewer_draw(&st_viewer, 0, 0, 0, 1, 1, nullptr, nullptr).code != PC_ERR_ARGUMENT) {
    fprintf(stderr, "FAIL: draw null callback\n");
    errors++;
  }
  viewer_close(&st_viewer);
  return errors;
}

}  // namespace

int main(void) {
  int errors = 0;
  errors += run_happy_path();
  errors += run_budget_ceiling();
  errors += run_argument_errors();
  if (errors) {
    fprintf(stderr, "viewer_loop: FAIL (%d errors)\n", errors);
    return 1;
  }
  printf("viewer_loop: PASS\n");
  return 0;
}