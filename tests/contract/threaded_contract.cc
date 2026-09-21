// R-M2 (ADR-0011): the engine SHALL NOT serialize rendering across threads on a process-wide
// lock array. Each thread works on its OWN document/context (the per-doc fz_locks_context in
// mupdf_backend.cc); 10 threads x 160 page-renders (1600 renders, each thread its own
// long160.pdf document) must finish at least min(4.0, 0.75 x logical cores) times faster than
// the identical 1600 renders done sequentially. The 4.0 floor is the R-M2 number measured at
// setup on the 6-core development box (5.8x observed); the core clamp keeps the check honest on
// GitHub's 4-core runner, where 4.0x is physically unattainable and a flaky required context
// would block merges for the wrong reason. With the old process-wide fz_locks_default every
// context blocked on the same mutex array and the ratio collapsed to ~1x (mupdf-rs issue #260,
// 13.3x figure) - that collapse fails the gate on every machine. This test pins the documented
// failure mode, not a benchmark: it runs against the mupdf backend only.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <thread>
#include <vector>

#include "pdfcore/backend.h"
#include "pdfcore/page.h"
#include "pdfcore/status.h"

static const pc_backend_api* g_api = nullptr;

#ifndef TEST_FIXTURE_DIR
#define TEST_FIXTURE_DIR "tests/fixtures"
#endif

static const char* fixture_path(void) {
  static char buf[4096];
  snprintf(buf, sizeof(buf), "%s/long160.pdf", TEST_FIXTURE_DIR);
  return buf;
}

static int64_t now_ms(void) {
  return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

static uint32_t render_all_pages(void* doc, uint32_t count) {
  uint32_t failures = 0;
  for (uint32_t i = 0; i < count; ++i) {
    void* page = nullptr;
    pc_status s = g_api->page_get(doc, i, &page);
    if (s.code != PC_ERR_NONE || !page) {
      ++failures;
      continue;
    }
    pc_render_params params = {};
    params.dpi = 72;
    params.clip = {0, 0, 100, 100};
    params.render_annots = 0;
    params.render_text = 1;
    pc_pixmap pixmap = {};
    s = g_api->page_render(page, &params, &pixmap);
    if (s.code != PC_ERR_NONE) {
      ++failures;
    }
    g_api->pixmap_free(&pixmap);
    g_api->page_free(page);
  }
  return failures;
}

// R-M2 - same total work on one thread versus 10 threads with per-doc contexts. Each of the 10
// threads renders all 160 pages of its OWN document; the single-threaded baseline runs the same
// 10 x 160 renders sequentially.
static int test_threaded_parallelism(void) {
  const uint32_t kThreads = 10;
  const uint32_t kPages = 160;

  // Single-threaded baseline: 10 sequential iterations of open + render-all + close.
  int64_t t0 = now_ms();
  for (uint32_t it = 0; it < kThreads; ++it) {
    void* doc = nullptr;
    pc_status s = g_api->doc_open(fixture_path(), nullptr, &doc);
    if (s.code != PC_ERR_NONE || !doc) {
      fprintf(stderr, "doc_open failed: code=%u\n", s.code);
      return 1;
    }
    uint32_t fails = render_all_pages(doc, kPages);
    g_api->doc_close(doc);
    if (fails != 0) {
      fprintf(stderr, "%u renders failed (single path)\n", fails);
      return 1;
    }
  }
  int64_t single_ms = now_ms() - t0;

  // Parallel: 10 threads, each its own document (its own context and lock set).
  std::vector<std::thread> threads;
  std::vector<uint32_t> thread_fails(kThreads, 0);
  int64_t t1 = now_ms();
  for (uint32_t t = 0; t < kThreads; ++t) {
    threads.emplace_back([t, &thread_fails]() {
      void* d = nullptr;
      pc_status st = g_api->doc_open(fixture_path(), nullptr, &d);
      if (st.code != PC_ERR_NONE || !d) {
        thread_fails[t] = 1;
        return;
      }
      thread_fails[t] = render_all_pages(d, kPages);
      g_api->doc_close(d);
    });
  }
  for (auto& th : threads) {
    th.join();
  }
  int64_t wall = now_ms() - t1;

  uint32_t fails = 0;
  for (uint32_t t = 0; t < kThreads; ++t) {
    fails += thread_fails[t];
  }

  printf("threaded_contract: single=%lldms wall=%lldms failures=%u\n", (long long)single_ms,
         (long long)wall, fails);
  if (fails != 0) {
    fprintf(stderr, "%u renders failed\n", fails);
    return 1;
  }
  if (single_ms <= 0 || wall <= 0) {
    fprintf(stderr, "R-M2 measured unusable times (single=%lldms parallel=%lldms)\n",
            (long long)single_ms, (long long)wall);
    return 1;
  }
  double speedup = (double)single_ms / (double)wall;
  unsigned cores = std::thread::hardware_concurrency();
  double target = (0.75 * cores) < 4.0 ? (0.75 * cores) : 4.0;
  if (speedup < target) {
    fprintf(stderr,
            "R-M2 violated: no parallelism with per-doc lock sets "
            "(single=%lldms parallel=%lldms speedup=%.2fx target=%.2fx cores=%u)\n",
            (long long)single_ms, (long long)wall, speedup, target, cores);
    return 1;
  }
  printf("threaded_contract: speedup %.2fx (target %.2fx, %u cores)\n", speedup, target, cores);
  return 0;
}

int main(void) {
  g_api = pc_mupdf_backend_get_api();
  int rc = test_threaded_parallelism();
  if (rc != 0) {
    fprintf(stderr, "FAIL: threaded contract\n");
    return 1;
  }
  printf("All tests passed\n");
  return 0;
}