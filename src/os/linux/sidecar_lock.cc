#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "pdfcore/sidecar.h"
#include "pdfcore/status.h"

static pc_status make_status(uint32_t code, const char* detail) {
  pc_status s = {};
  s.size = sizeof(pc_status);
  s.code = code;
  s.detail = detail;
  return s;
}

static int write_all(int fd, const void* buf, size_t count) {
  const char* p = (const char*)buf;
  size_t written = 0;
  while (written < count) {
    ssize_t n = write(fd, p + written, count - written);
    if (n <= 0)
      return -1;
    written += n;
  }
  return 0;
}

pc_status pc_sidecar_try_lock(const char* doc_path) {
  if (!doc_path) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }

  char lock_path[4096];
  snprintf(lock_path, sizeof(lock_path), "%s.tynypdf.lock", doc_path);

  int fd = open(lock_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd < 0) {
    return make_status(PC_ERR_STATE, "lock exists");
  }

  char lock_content[128];
  int pid = getpid();
  int len = snprintf(lock_content, sizeof(lock_content), "pid=%d time=%" PRId64 "\n", pid,
                     (int64_t)time(nullptr));
  write_all(fd, lock_content, len);
  fsync(fd);
  close(fd);

  return make_status(PC_ERR_NONE, nullptr);
}

void pc_sidecar_unlock(const char* doc_path) {
  if (!doc_path)
    return;
  char lock_path[4096];
  snprintf(lock_path, sizeof(lock_path), "%s.tynypdf.lock", doc_path);
  unlink(lock_path);
}