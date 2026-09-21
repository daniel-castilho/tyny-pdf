#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "pdfcore/doc.h"
#include "pdfcore/sha256.h"
#include "pdfcore/sidecar.h"
#include "pdfcore/status.h"

static pc_status make_status(uint32_t code, const char* detail) {
  pc_status s = {};
  s.size = sizeof(pc_status);
  s.code = code;
  s.detail = detail;
  return s;
}

static int64_t file_mtime(const char* path) {
  struct stat st;
  if (stat(path, &st) != 0)
    return 0;
  return (int64_t)st.st_mtime;
}

static void format_rfc3339(char* buf, size_t cap, int64_t t) {
  time_t tt = (time_t)t;
  struct tm utc;
#if defined(_WIN32)
  gmtime_s(&utc, &tt);
#else
  gmtime_r(&tt, &utc);
#endif
  snprintf(buf, cap, "%04d-%02d-%02dT%02d:%02d:%02dZ", utc.tm_year + 1900, utc.tm_mon + 1,
           utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
}

static void set_stale(pc_sidecar* sidecar, pc_staleness_reason reason, const char* fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  sidecar->is_stale = 1;
  sidecar->stale_reason = reason;
  free(sidecar->stale_detail);
  sidecar->stale_detail = strdup(buf);
}

int pc_sidecar_is_stale(const pc_sidecar* sidecar) {
  return sidecar && sidecar->is_stale;
}

pc_status pc_sidecar_stale_report(const pc_sidecar* sidecar, char* buf, size_t cap) {
  if (!sidecar || !buf || cap == 0)
    return make_status(PC_ERR_ARGUMENT, "null argument");
  if (!sidecar->stale_detail) {
    buf[0] = '\0';
    return make_status(PC_ERR_NONE, nullptr);
  }
  snprintf(buf, cap, "%s", sidecar->stale_detail);
  return make_status(PC_ERR_NONE, nullptr);
}

pc_status pc_sidecar_compute_fingerprint(const char* doc_path, char** out_fingerprint) {
  if (!doc_path || !out_fingerprint)
    return make_status(PC_ERR_ARGUMENT, "null argument");

  FILE* f = fopen(doc_path, "rb");
  if (!f)
    return make_status(PC_ERR_IO, "cannot open document");

  pc_sha256 ctx;
  pc_sha256_init(&ctx);

  char buffer[8192];
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
    pc_sha256_update(&ctx, (const uint8_t*)buffer, bytes_read);
  }
  fclose(f);

  unsigned char hash[32];
  pc_sha256_final(&ctx, hash);

  *out_fingerprint = (char*)malloc(65);
  if (!*out_fingerprint)
    return make_status(PC_ERR_MEMORY, "OOM");
  for (int i = 0; i < 32; ++i) {
    snprintf(*out_fingerprint + i * 2, 3, "%02x", hash[i]);
  }
  (*out_fingerprint)[64] = '\0';

  return make_status(PC_ERR_NONE, nullptr);
}

pc_status pc_sidecar_compute_page_count(const char* doc_path, int* out_page_count) {
  if (!doc_path || !out_page_count)
    return make_status(PC_ERR_ARGUMENT, "null argument");

  pc_doc* doc = nullptr;
  pc_status s = pc_doc_open(doc_path, nullptr, &doc);
  if (s.code != PC_ERR_NONE)
    return s;

  *out_page_count = (int)pc_doc_page_count(doc);
  pc_doc_close(doc);
  return make_status(PC_ERR_NONE, nullptr);
}

pc_status pc_sidecar_check_staleness(const char* doc_path, pc_sidecar* sidecar) {
  if (!doc_path || !sidecar)
    return make_status(PC_ERR_ARGUMENT, "null argument");

  sidecar->is_stale = 0;
  sidecar->stale_reason = PC_STALE_NONE;
  free(sidecar->stale_detail);
  sidecar->stale_detail = nullptr;

  // R5.1: recorded page count vs the document's page count.
  int current_pages = 0;
  if (pc_sidecar_compute_page_count(doc_path, &current_pages).code == PC_ERR_NONE &&
      sidecar->page_count > 0 && current_pages != sidecar->page_count) {
    set_stale(sidecar, PC_STALE_PAGE_COUNT, "stale: sidecar pages %d vs document %d",
              sidecar->page_count, current_pages);
    return make_status(PC_ERR_STATE, sidecar->stale_detail);
  }

  // R5.2: fingerprint is the strong signal.
  char* current_fp = nullptr;
  pc_status fp_st = pc_sidecar_compute_fingerprint(doc_path, &current_fp);
  if (fp_st.code == PC_ERR_NONE && current_fp && sidecar->document_sha256 &&
      strcmp(sidecar->document_sha256, current_fp) != 0) {
    set_stale(sidecar, PC_STALE_FINGERPRINT,
              "stale: fingerprint mismatch (doc sha256 %.16s... vs sidecar %.16s...)", current_fp,
              sidecar->document_sha256);
    free(current_fp);
    return make_status(PC_ERR_STATE, sidecar->stale_detail);
  }
  free(current_fp);

  // R5.2: modification time is the weak signal; only a strictly newer document fires it.
  int64_t current_mtime = file_mtime(doc_path);
  if (current_mtime > sidecar->modified_time) {
    char doc_t[32];
    char sidecar_t[32];
    format_rfc3339(doc_t, sizeof(doc_t), current_mtime);
    format_rfc3339(sidecar_t, sizeof(sidecar_t), sidecar->modified_time);
    set_stale(sidecar, PC_STALE_MODIFIED_TIME, "stale: mtime newer (doc %s vs sidecar %s)", doc_t,
              sidecar_t);
    return make_status(PC_ERR_STATE, sidecar->stale_detail);
  }

  return make_status(PC_ERR_NONE, nullptr);
}