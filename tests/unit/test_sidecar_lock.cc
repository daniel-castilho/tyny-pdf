// R3.2: .tynypdf.lock freshness check (5 minutes)
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "pdfcore/sidecar.h"
#include "pdfcore/status.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(a, b)                                                                            \
  do {                                                                                             \
    if ((a) != (b)) {                                                                              \
      fprintf(stderr, "FAIL: %s:%d: %s == %s (%d != %d)\n", __FILE__, __LINE__, #a, #b, (a), (b)); \
      tests_failed++;                                                                              \
    } else {                                                                                       \
      tests_passed++;                                                                              \
    }                                                                                              \
  } while (0)

#define ASSERT_NE(a, b)                                                       \
  do {                                                                        \
    if ((a) == (b)) {                                                         \
      fprintf(stderr, "FAIL: %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
      tests_failed++;                                                         \
    } else {                                                                  \
      tests_passed++;                                                         \
    }                                                                         \
  } while (0)

#define ASSERT_STRSTR(haystack, needle)                                                          \
  do {                                                                                           \
    if (!(haystack) || !(needle) || strstr((haystack), (needle)) == nullptr) {                   \
      fprintf(stderr, "FAIL: %s:%d: strstr('%s', '%s') failed\n", __FILE__, __LINE__, #haystack, \
              #needle);                                                                          \
      tests_failed++;                                                                            \
    } else {                                                                                     \
      tests_passed++;                                                                            \
    }                                                                                            \
  } while (0)

#define ASSERT_TRUE(expr)                                              \
  do {                                                                 \
    if (!(expr)) {                                                     \
      fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
      tests_failed++;                                                  \
    } else {                                                           \
      tests_passed++;                                                  \
    }                                                                  \
  } while (0)

#define ASSERT_FALSE(expr)                                             \
  do {                                                                 \
    if (expr) {                                                        \
      fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
      tests_failed++;                                                  \
    } else {                                                           \
      tests_passed++;                                                  \
    }                                                                  \
  } while (0)

// Free string fields of stack-allocated pc_sidecar (NOT pc_sidecar_free which frees the struct
// itself)
#define FREE_SC_FIELDS(sc)       \
  do {                           \
    free((sc).document_sha256);  \
    free((sc).document_path);    \
    free((sc).annotations_json); \
    free((sc).unknown_json);     \
  } while (0)

#define ASSERT_GE(a, b)                                                                           \
  do {                                                                                            \
    if ((a) < (b)) {                                                                              \
      fprintf(stderr, "FAIL: %s:%d: %s >= %s (%d < %d)\n", __FILE__, __LINE__, #a, #b, (a), (b)); \
      tests_failed++;                                                                             \
    } else {                                                                                      \
      tests_passed++;                                                                             \
    }                                                                                             \
  } while (0)

static char* make_temp_doc() {
  char* tmpl = strdup("/tmp/tynypdf_test_XXXXXX");
  int fd = mkstemp(tmpl);
  if (fd < 0) {
    free(tmpl);
    return nullptr;
  }
  close(fd);
  return tmpl;
}

static void cleanup_doc(const char* doc) {
  if (!doc)
    return;
  char path[4096];
  snprintf(path, sizeof(path), "%s.tynypdf.json", doc);
  unlink(path);
  snprintf(path, sizeof(path), "%s.tynypdf.lock", doc);
  unlink(path);
  unlink(doc);
  free((void*)doc);
}

int main(void) {
  // Test 1: FreshLockRejectsWrite
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    pc_status s = pc_sidecar_try_lock(doc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup("abc123");
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 1;
    sc.annotations_json = strdup("[]");
    sc.unknown_json = strdup("{}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_STATE);
    ASSERT_STRSTR(s.detail, "locked");

    FREE_SC_FIELDS(sc);

    pc_sidecar_unlock(doc);

    sc.document_sha256 = strdup("abc123");
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 1;
    sc.annotations_json = strdup("[]");
    sc.unknown_json = strdup("{}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    FREE_SC_FIELDS(sc);
    cleanup_doc(doc);
  }

  // Test 2: StaleLockAllowsWrite
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s.tynypdf.lock", doc);

    int fd = open(lock_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);
    time_t old_time = time(nullptr) - 400;
    char lock_content[128];
    int len = snprintf(lock_content, sizeof(lock_content), "pid=999 time=%ld\n", (long)old_time);
    write(fd, lock_content, len);
    close(fd);

    struct timespec ts[2];
    ts[0].tv_sec = old_time;
    ts[0].tv_nsec = 0;
    ts[1].tv_sec = old_time;
    ts[1].tv_nsec = 0;
    utimensat(AT_FDCWD, lock_path, ts, 0);

    pc_sidecar sc = {};
    sc.format_version = 1;
    sc.document_sha256 = strdup("abc123");
    sc.document_path = strdup(doc);
    sc.modified_time = time(nullptr);
    sc.page_count = 1;
    sc.annotations_json = strdup("[]");
    sc.unknown_json = strdup("{}");
    sc.is_stale = 0;
    sc.stale_reason = PC_STALE_NONE;

    pc_status s = pc_sidecar_write(doc, &sc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    struct stat st;
    ASSERT_EQ(stat(lock_path, &st), -1);

    FREE_SC_FIELDS(sc);
    cleanup_doc(doc);
  }

  // Test 3: LockContainsPIDAndTimestamp
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    pc_status s = pc_sidecar_try_lock(doc);
    ASSERT_EQ(s.code, PC_ERR_NONE);

    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s.tynypdf.lock", doc);

    FILE* f = fopen(lock_path, "r");
    ASSERT_NE(f, nullptr);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    ASSERT_STRSTR(buf, "pid=");
    ASSERT_STRSTR(buf, "time=");

    pc_sidecar_unlock(doc);
    cleanup_doc(doc);
  }

  // Test 4: DoubleLockFails
  {
    char* doc = make_temp_doc();
    ASSERT_TRUE(doc != nullptr);

    pc_status s1 = pc_sidecar_try_lock(doc);
    ASSERT_EQ(s1.code, PC_ERR_NONE);

    pc_status s2 = pc_sidecar_try_lock(doc);
    ASSERT_EQ(s2.code, PC_ERR_STATE);

    pc_sidecar_unlock(doc);
    cleanup_doc(doc);
  }

  fprintf(stderr, "PASSED: %d, FAILED: %d\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}