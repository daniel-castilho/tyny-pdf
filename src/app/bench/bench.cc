// Viewer benchmark mode (story 1.5, R31.1). Windows-only.
// src/app/bench/bench.cc
// Drives the viewer loop headless and emits forward/return frame_ms and
// peak_rss_kib as JSON for tools/bench-measure.sh:
//   - forward pass: pages 0..N, TYNYPDF_BENCH_TILES tiles per page;
//   - return pass:  pages N..0, same strip  (R30.2: no RSS growth);
//   - full-region pass: page 0's 16x12 grid (the 4000x3000 R15.1 region), all
//     tiles warmed first so the recorded frames are the blit alone.
// Frames are pumped with pc_window_request_redraw and the session ends when
// the TYNYPDF_FRAMES budget (set here) is exhausted. A selftest knob like
// TYNYPDF_WHEELS: the interactive product never reads it.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// Windows headers first to get proper type definitions
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on

#include <sys/stat.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pdfcore/backend.h"
#include "pdfcore/status.h"
#include "pdfcore/window.h"
#include "viewer/d2d_blit.h"
#include "viewer/viewer.h"

namespace tynypdf {
namespace bench {

namespace {

double qpc_now_ms() {
  static LARGE_INTEGER freq = {};
  if (freq.QuadPart == 0) {
    QueryPerformanceFrequency(&freq);
  }
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
}

uint64_t current_working_set_kib() {
  PROCESS_MEMORY_COUNTERS pmc = {};
  if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    return 0;
  }
  return pmc.WorkingSetSize / 1024;
}

struct bench_session {
  viewer::viewer_state vst;
  pc_window* window = nullptr;
  viewer::d2d_scratch_state scratch;
  void* d2d = nullptr;  // ID2D1DeviceContext* handed by the render callback

  uint32_t tiles = 3;  // strip width (tiles per page)
  uint32_t rows = 1;   // strip height
  uint32_t full_frames = 24;
  uint32_t settle_frames = 32;  // unrecorded blits after the flip's upload burst

  int phase = 0;       // 0 fwd, 1 ret, 2 full
  int page = 0;        // current page in fwd/ret
  int steps_left = 0;  // pages remaining in the current pass
  bool full_prepared = false;
  int settle_left = 0;  // unrecorded full-region blits after the warmup

  double t0_ms = 0.0;

  std::vector<double> fwd_ms;
  std::vector<double> ret_ms;
  std::vector<double> full_ms;
  std::vector<uint64_t> fwd_ws_kib;
  std::vector<uint64_t> ret_ws_kib;
  std::vector<uint64_t> full_ws_kib;
};

void set_env_int(const char* name, int value) {
  char buf[32];
  _snprintf(buf, sizeof(buf), "%d", value);
  _putenv_s(name, buf);
}

// Draw the strip the current frame demanded (D2D when present, else no-op).
pc_status draw_strip(viewer::viewer_state* vst, bench_session* b, int page, int32_t col0,
                     int32_t row0, uint32_t cols, uint32_t rows) {
  return viewer::viewer_draw(
      vst, (uint32_t)page, col0, row0, cols, rows,
      [](const viewer::tile_payload* payload, int32_t col, int32_t row,
         void* user_data) -> pc_status {
        bench_session* s = static_cast<bench_session*>(user_data);
        if (!s->d2d) {
          return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
        }
        return d2d_blit_tile(s->d2d, payload, col, row, &s->scratch);
      },
      b);
}

pc_status bench_render_cb(void* d2d_context, void* user_data) {
  bench_session* b = static_cast<bench_session*>(user_data);
  b->d2d = d2d_context;

  const double t = qpc_now_ms();
  double frame_ms = t - b->t0_ms;
  b->t0_ms = t;
  const uint64_t ws = current_working_set_kib();

  pc_status st = {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};

  if (b->phase == 0 && b->steps_left > 0) {
    // Forward pass: page 0..N, strip rows x tiles.
    st = viewer::viewer_demand(&b->vst, (uint32_t)b->page, 0, 0, b->tiles, b->rows);
    if (st.code == PC_ERR_NONE) {
      st = draw_strip(&b->vst, b, b->page, 0, 0, b->tiles, b->rows);
    }
    b->fwd_ms.push_back(frame_ms);
    b->fwd_ws_kib.push_back(ws);
    if (st.code != PC_ERR_NONE) {
      return st;
    }
    if (--b->steps_left == 0) {
      b->phase = 1;
      b->page = (int)b->vst.page_count - 1;
      b->steps_left = (int)b->vst.page_count;
    } else {
      b->page++;
    }
  } else if (b->phase == 1 && b->steps_left > 0) {
    // Return pass: page N-1..0, so the last page scrolled forward is the
    // first re-demanded going back (R30.2 measures return-pass growth).
    st = viewer::viewer_demand(&b->vst, (uint32_t)b->page, 0, 0, b->tiles, b->rows);
    if (st.code == PC_ERR_NONE) {
      st = draw_strip(&b->vst, b, b->page, 0, 0, b->tiles, b->rows);
    }
    b->ret_ms.push_back(frame_ms);
    b->ret_ws_kib.push_back(ws);
    if (st.code != PC_ERR_NONE) {
      return st;
    }
    if (--b->steps_left == 0) {
      b->phase = 2;
      b->page = 0;
      b->full_prepared = false;
    } else {
      b->page--;
    }
  } else if (b->phase == 2) {
    if (!b->full_prepared) {
      // Warm the full 16x12 grid and DRAW it once so bitmap creation and the
      // 48MB upload burst happen here. The burst stalls the next few presents
      // (GPU backpressure); the settle window below lets that drain so the
      // recorded frames measure the sustained blit a 60fps loop must deliver
      // (R15.1). The flip transient stays visible in the fwd/ret frame runs.
      st = viewer::viewer_demand(&b->vst, 0, 0, 0, 16, 12);
      if (st.code == PC_ERR_NONE) {
        st = draw_strip(&b->vst, b, 0, 0, 0, 16, 12);
      }
      b->full_prepared = true;
      b->settle_left = (int)b->settle_frames;
      if (st.code != PC_ERR_NONE) {
        return st;
      }
      pc_window_request_redraw(b->window);
      return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
    }
    if (b->settle_left > 0) {
      st = draw_strip(&b->vst, b, 0, 0, 0, 16, 12);
      if (st.code != PC_ERR_NONE) {
        return st;
      }
      b->settle_left--;
      pc_window_request_redraw(b->window);
      return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
    }
    st = draw_strip(&b->vst, b, 0, 0, 0, 16, 12);
    b->full_ms.push_back(frame_ms);
    b->full_ws_kib.push_back(ws);
    if (st.code != PC_ERR_NONE) {
      return st;
    }
  }

  // Keep the message loop pumping; TYNYPDF_FRAMES stops the session.
  pc_window_request_redraw(b->window);
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

void emit_metric(FILE* f, const char* name, const std::vector<double>& values, bool* first) {
  if (values.empty()) {
    return;
  }
  double sum = 0.0;
  double min = values[0];
  double max = values[0];
  for (double v : values) {
    sum += v;
    min = min < v ? min : v;
    max = max > v ? max : v;
  }
  double mean = sum / (double)values.size();
  double var = 0.0;
  for (double v : values) {
    var += (v - mean) * (v - mean);
  }
  double stdev = values.size() > 1 ? sqrt(var / (double)(values.size() - 1)) : 0.0;
  fprintf(f,
          "    %s\"%s\": {\"mean\": %.3f, \"stdev\": %.3f, \"min\": %.3f, \"max\": %.3f,"
          " \"runs\": [",
          *first ? "" : ",\n", name, mean, stdev, min, max);
  *first = false;
  for (size_t i = 0; i < values.size(); ++i) {
    fprintf(f, "%s%.3f", i ? ", " : "", values[i]);
  }
  fprintf(f, "]}");
}

void emit_ws_metric(FILE* f, const char* name, const std::vector<uint64_t>& values, bool* first) {
  if (values.empty()) {
    return;
  }
  double sum = 0.0;
  double min = (double)values[0];
  double max = (double)values[0];
  for (uint64_t v : values) {
    sum += (double)v;
    min = min < (double)v ? min : (double)v;
    max = max > (double)v ? max : (double)v;
  }
  double mean = sum / (double)values.size();
  fprintf(f, "    %s\"%s\": {\"mean_kib\": %.0f, \"min\": %.0f, \"max_kib\": %.0f, \"runs\": [",
          *first ? "" : ",\n", name, mean, min, max);
  *first = false;
  for (size_t i = 0; i < values.size(); ++i) {
    fprintf(f, "%s%llu", i ? ", " : "", (unsigned long long)values[i]);
  }
  fprintf(f, "]}");
}

int write_json(bench_session* b, const char* pdf_path, const char* out_path) {
  // The present budget ends the run with a WM_CLOSE whose DestroyWindow lands
  // inside the NEXT recorded interval (a deterministic ~11ms teardown, not a
  // blit). Keep it out of full_frame_ms and report it separately so the
  // steady-state metric stays comparable across runs (R31.1).
  double teardown_ms = 0.0;
  while (b->full_ms.size() > b->full_frames) {
    teardown_ms = b->full_ms.back();
    b->full_ms.pop_back();
    b->full_ws_kib.pop_back();
  }

  FILE* f = fopen(out_path, "wb");
  if (!f) {
    fprintf(stderr, "bench: cannot write %s\n", out_path);
    return 1;
  }
  struct _stat64 stbuf = {};
  unsigned long long size_bytes = 0;
  if (_stat64(pdf_path, &stbuf) == 0) {
    size_bytes = (unsigned long long)stbuf.st_size;
  }
  fprintf(f,
          "{\n  \"target\": \"tynypdf\",\n  \"bench\": \"viewer\",\n"
          "  \"pdf_file\": \"%s\",\n  \"pdf_size_bytes\": %llu,\n"
          "  \"page_count\": %u,\n  \"tiles_per_page\": %u,\n  \"full_settle_frames\": %u,\n"
          "  \"full_teardown_ms\": %.3f,\n"
          "  \"metrics\": {\n",
          pdf_path, size_bytes, b->vst.page_count, b->tiles, b->settle_frames, teardown_ms);
  bool first = true;
  emit_metric(f, "fwd_frame_ms", b->fwd_ms, &first);
  emit_metric(f, "ret_frame_ms", b->ret_ms, &first);
  emit_metric(f, "full_frame_ms", b->full_ms, &first);
  emit_ws_metric(f, "fwd_peak_rss_kib", b->fwd_ws_kib, &first);
  emit_ws_metric(f, "ret_peak_rss_kib", b->ret_ws_kib, &first);
  emit_ws_metric(f, "full_peak_rss_kib", b->full_ws_kib, &first);
  fprintf(f, "\n  }\n}\n");
  fclose(f);
  return 0;
}

}  // namespace

int bench_main(const char* pdf_path) {
  if (!pdf_path || !*pdf_path) {
    fprintf(stderr, "tynypdf: --bench requires a PDF path\n");
    return 2;
  }

  bench_session b;

  const char* tiles_env = getenv("TYNYPDF_BENCH_TILES");
  if (tiles_env) {
    b.tiles = (uint32_t)atoi(tiles_env);
  }
  const char* rows_env = getenv("TYNYPDF_BENCH_ROWS");
  if (rows_env) {
    b.rows = (uint32_t)atoi(rows_env);
  }
  const char* full_env = getenv("TYNYPDF_BENCH_FULL_FRAMES");
  if (full_env) {
    b.full_frames = (uint32_t)atoi(full_env);
  }
  const char* settle_env = getenv("TYNYPDF_BENCH_SETTLE");
  if (settle_env) {
    b.settle_frames = (uint32_t)atoi(settle_env);
  }
  const char* json_env = getenv("TYNYPDF_BENCH_JSON");
  const char* json_path = json_env && *json_env ? json_env : "build/bench.json";
  const char* ui_log_env = getenv("TYNYPDF_UI_LOG");
  _putenv_s("TYNYPDF_UI_LOG", ui_log_env && *ui_log_env ? ui_log_env : "build/tynypdf.bench.log");

  // Select the engine: mupdf when linked, null otherwise.
  const pc_backend_api* api = nullptr;
#ifdef PC_HAVE_MUPDF
  api = pc_mupdf_backend_get_api();
#else
  api = pc_null_backend_get_api();
#endif

  pc_status st = viewer::viewer_open(&b.vst, api, pdf_path, 72, 256, 0);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "tynypdf: open failed: %u %s\n", st.code, st.detail ? st.detail : "-");
    return 1;
  }
  if (b.vst.page_count == 0) {
    fprintf(stderr, "tynypdf: --bench needs a non-empty document\n");
    viewer::viewer_close(&b.vst);
    return 1;
  }

  // One present per page, forward then return; then the full-region phase's
  // warmup, settle window and measured frames.
  int budget = (int)b.vst.page_count * 2 + 1 + (int)b.settle_frames + (int)b.full_frames;
  set_env_int("TYNYPDF_FRAMES", budget);

  b.t0_ms = qpc_now_ms();
  b.phase = 0;
  b.steps_left = (int)b.vst.page_count;

  pc_window_params params = {};
  params.width = 1024;
  params.height = 768;
  params.title = L"Tyny PDF";
  params.user_data = &b;
  pc_window_callbacks callbacks = {};
  callbacks.render = &bench_render_cb;

  pc_window* window = nullptr;
  st = pc_window_create(&params, &callbacks, &window);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "tynypdf: window create failed: %u %s\n", st.code, st.detail ? st.detail : "-");
    viewer::viewer_close(&b.vst);
    return 1;
  }
  b.window = window;

  st = pc_window_run(window);
  if (st.code != PC_ERR_NONE) {
    fprintf(stderr, "tynypdf: message loop failed: %u %s\n", st.code, st.detail ? st.detail : "-");
  }
  pc_window_destroy(window);

  int rc = write_json(&b, pdf_path, json_path);
  printf("bench: wrote %s (pages=%u fwd=%zu ret=%zu full=%zu)\n", json_path, b.vst.page_count,
         b.fwd_ms.size(), b.ret_ms.size(), b.full_ms.size());

  viewer::d2d_scratch_release(&b.scratch);
  viewer::viewer_close(&b.vst);
  return rc;
}

}  // namespace bench
}  // namespace tynypdf