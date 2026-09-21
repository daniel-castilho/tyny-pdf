#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "pdfcore/platform.h"
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

static char* read_file(const char* path, size_t* out_size) {
  FILE* f = fopen(path, "rb");
  if (!f)
    return nullptr;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (size < 0) {
    fclose(f);
    return nullptr;
  }
  char* buf = (char*)malloc(size + 1);
  if (!buf) {
    fclose(f);
    return nullptr;
  }
  size_t read = fread(buf, 1, size, f);
  fclose(f);
  if (read != (size_t)size) {
    free(buf);
    return nullptr;
  }
  buf[size] = '\0';
  if (out_size)
    *out_size = size;
  return buf;
}

static int file_exists(const char* path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static int64_t file_mtime(const char* path) {
  struct stat st;
  if (stat(path, &st) != 0)
    return 0;
  return st.st_mtime;
}

// Canonical JSON serialization helpers (R1.1-R1.3)
static void json_append_str(char* buf, size_t* pos, size_t cap, const char* s) {
  size_t len = strlen(s);
  if (*pos + len >= cap)
    return;
  memcpy(buf + *pos, s, len);
  *pos += len;
}

static void json_append_int(char* buf, size_t* pos, size_t cap, int64_t v) {
  char tmp[32];
  int len = snprintf(tmp, sizeof(tmp), "%lld", (long long)v);
  if (*pos + len >= cap)
    return;
  memcpy(buf + *pos, tmp, len);
  *pos += len;
}

static void json_append_escaped(char* buf, size_t* pos, size_t cap, const char* s) {
  if (!s)
    s = "";
  json_append_str(buf, pos, cap, "\"");
  for (const char* p = s; *p; ++p) {
    unsigned char c = *p;
    char esc[7] = {0};
    int elen = 0;
    switch (c) {
      case '"':
        esc[0] = '\\';
        esc[1] = '"';
        elen = 2;
        break;
      case '\\':
        esc[0] = '\\';
        esc[1] = '\\';
        elen = 2;
        break;
      case '\b':
        esc[0] = '\\';
        esc[1] = 'b';
        elen = 2;
        break;
      case '\f':
        esc[0] = '\\';
        esc[1] = 'f';
        elen = 2;
        break;
      case '\n':
        esc[0] = '\\';
        esc[1] = 'n';
        elen = 2;
        break;
      case '\r':
        esc[0] = '\\';
        esc[1] = 'r';
        elen = 2;
        break;
      case '\t':
        esc[0] = '\\';
        esc[1] = 't';
        elen = 2;
        break;
      default:
        if (c < 0x20) {
          snprintf(esc, sizeof(esc), "\\u%04x", c);
          elen = 6;
        }
        break;
    }
    if (elen) {
      if (*pos + elen >= cap)
        return;
      memcpy(buf + *pos, esc, elen);
      *pos += elen;
    } else {
      if (*pos + 1 >= cap)
        return;
      buf[(*pos)++] = c;
    }
  }
  json_append_str(buf, pos, cap, "\"");
}

static void json_write_key_value_str(char* buf, size_t* pos, size_t cap, int indent,
                                     const char* key, const char* value) {
  for (int i = 0; i < indent; ++i) json_append_str(buf, pos, cap, "  ");
  json_append_escaped(buf, pos, cap, key);
  json_append_str(buf, pos, cap, ": ");
  json_append_escaped(buf, pos, cap, value);
  json_append_str(buf, pos, cap, ",\n");
}

static void json_write_key_value_int(char* buf, size_t* pos, size_t cap, int indent,
                                     const char* key, int64_t value) {
  for (int i = 0; i < indent; ++i) json_append_str(buf, pos, cap, "  ");
  json_append_escaped(buf, pos, cap, key);
  json_append_str(buf, pos, cap, ": ");
  json_append_int(buf, pos, cap, value);
  json_append_str(buf, pos, cap, ",\n");
}

// Serialize sidecar to canonical JSON (R1.1-R1.3: keys sorted, 2-space indent, LF, no trailing ws)
static int pc_sidecar_to_canonical_json(const pc_sidecar* sidecar, char* buf, size_t cap) {
  if (!sidecar || !buf || cap < 256)
    return -1;
  size_t pos = 0;

  // Root object - keys in alphabetical order: annotations, document, format_version, is_stale,
  // stale_reason, unknown
  json_append_str(buf, &pos, cap, "{\n");

  // annotations (alphabetically first)
  json_append_str(buf, &pos, cap, "  ");
  json_append_escaped(buf, &pos, cap, "annotations");
  json_append_str(buf, &pos, cap, ": ");
  json_append_str(buf, &pos, cap, sidecar->annotations_json ? sidecar->annotations_json : "[]");
  json_append_str(buf, &pos, cap, ",\n");

  // document (object with keys: modified, pages, path, sha256 - sorted)
  json_append_str(buf, &pos, cap, "  ");
  json_append_escaped(buf, &pos, cap, "document");
  json_append_str(buf, &pos, cap, ": {\n");

  // document keys sorted: modified, pages, path, sha256
  json_write_key_value_int(buf, &pos, cap, 2, "modified", sidecar->modified_time);
  json_write_key_value_int(buf, &pos, cap, 2, "pages", sidecar->page_count);
  json_write_key_value_str(buf, &pos, cap, 2, "path",
                           sidecar->document_path ? sidecar->document_path : "");
  json_write_key_value_str(buf, &pos, cap, 2, "sha256",
                           sidecar->document_sha256 ? sidecar->document_sha256 : "");

  json_append_str(buf, &pos, cap, "  ");
  json_append_str(buf, &pos, cap, "},\n");

  // format_version
  json_write_key_value_int(buf, &pos, cap, 1, "format_version", sidecar->format_version);

  // is_stale
  json_write_key_value_int(buf, &pos, cap, 1, "is_stale", sidecar->is_stale ? 1 : 0);

  // stale_reason
  json_write_key_value_int(buf, &pos, cap, 1, "stale_reason", (int)sidecar->stale_reason);

  // unknown (last alphabetically)
  json_append_str(buf, &pos, cap, "  ");
  json_append_escaped(buf, &pos, cap, "unknown");
  json_append_str(buf, &pos, cap, ": ");
  json_append_str(buf, &pos, cap, sidecar->unknown_json ? sidecar->unknown_json : "{}");
  json_append_str(buf, &pos, cap, "\n");

  // Close root
  json_append_str(buf, &pos, cap, "}\n");

  return (pos < cap) ? (int)pos : -1;
}

pc_status pc_sidecar_write(const char* doc_path, const pc_sidecar* sidecar) {
  if (!doc_path || !sidecar) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }

  if (!sidecar->document_path || strcmp(sidecar->document_path, doc_path) != 0) {
    return make_status(PC_ERR_ARGUMENT, "document path mismatch (R4.2)");
  }

  // Check lock (R3.2)
  char lock_path[4096];
  snprintf(lock_path, sizeof(lock_path), "%s.tynypdf.lock", doc_path);
  if (file_exists(lock_path)) {
    int64_t lock_age = time(nullptr) - file_mtime(lock_path);
    if (lock_age < 300) {
      return make_status(PC_ERR_STATE, "sidecar locked");
    }
    // Stale lock - remove it
    unlink(lock_path);
  }

  // Check excluded keys (R6.1, R6.2)
  const char* excluded_keys[] = {// R6.1: view state
                                 "open_page", "zoom", "sidebar", "scroll", "window_size",
                                 "scroll_position", "scroll_x", "scroll_y",
                                 // R6.2: document text, page geometry, credentials
                                 "page_geometry", "text_content", "credential", "passphrase",
                                 "document_text", "page_text", "font_data", "signing_key"};
  if (sidecar->unknown_json) {
    for (size_t i = 0; i < sizeof(excluded_keys) / sizeof(excluded_keys[0]); ++i) {
      if (strstr(sidecar->unknown_json, excluded_keys[i])) {
        return make_status(PC_ERR_ARGUMENT, "excluded key in sidecar");
      }
    }
  }

  // Build temp path
  char temp_path[4096];
  snprintf(temp_path, sizeof(temp_path), "%s.tynypdf.json.tmp.%08x", doc_path, (unsigned)rand());

  int fd = open(temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    return make_status(PC_ERR_IO, "failed to create temp file");
  }

  // Write canonical JSON (R1.1-R1.3)
  char json_buf[8192];
  int len = pc_sidecar_to_canonical_json(sidecar, json_buf, sizeof(json_buf));

  if (len < 0 || len >= (int)sizeof(json_buf)) {
    close(fd);
    unlink(temp_path);
    return make_status(PC_ERR_MEMORY, "JSON too large");
  }

  if (write_all(fd, json_buf, len) != 0) {
    close(fd);
    unlink(temp_path);
    return make_status(PC_ERR_IO, "write failed");
  }

  if (fsync(fd) != 0) {
    close(fd);
    unlink(temp_path);
    return make_status(PC_ERR_IO, "fsync failed");
  }

  close(fd);

  // Atomic rename
  char target_path[4096];
  snprintf(target_path, sizeof(target_path), "%s.tynypdf.json", doc_path);
  if (rename(temp_path, target_path) != 0) {
    unlink(temp_path);
    return make_status(PC_ERR_IO, "rename failed");
  }

  return make_status(PC_ERR_NONE, nullptr);
}

// Extract a JSON value (string, object, array, number, boolean, null) as raw string
static char* extract_json_value(const char* content, const char* key) {
  if (!content || !key)
    return nullptr;
  char marker[128];
  snprintf(marker, sizeof(marker), "\"%s\":", key);
  const char* p = strstr(content, marker);
  if (!p)
    return nullptr;
  p += strlen(marker);
  while (*p == ' ' || *p == '\t' || *p == '\n') p++;
  if (!*p)
    return nullptr;

  const char* start = p;
  int depth = 0;
  int in_string = 0;
  int escape = 0;

  while (*p) {
    if (in_string) {
      if (escape) {
        escape = 0;
      } else if (*p == '\\') {
        escape = 1;
      } else if (*p == '"') {
        in_string = 0;
      }
    } else {
      if (*p == '"') {
        in_string = 1;
      } else if (*p == '{' || *p == '[') {
        depth++;
      } else if (*p == '}' || *p == ']') {
        if (depth == 0) {
          p++;  // include closing brace/bracket
          break;
        }
        depth--;
      } else if (depth == 0 && (*p == ',' || *p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')) {
        // End of value at top level
        break;
      }
    }
    p++;
  }

  if (p <= start)
    return nullptr;
  size_t len = p - start;
  char* result = (char*)malloc(len + 1);
  if (!result)
    return nullptr;
  memcpy(result, start, len);
  result[len] = '\0';
  return result;
}

static char* extract_json_string(const char* content, const char* key) {
  // For backward compat - extracts string value only
  char* val = extract_json_value(content, key);
  if (!val)
    return nullptr;
  // If it's a quoted string, unquote it
  if (val[0] == '"') {
    size_t len = strlen(val);
    if (len >= 2 && val[len - 1] == '"') {
      val[len - 1] = '\0';
      memmove(val, val + 1, len);
    }
  }
  return val;
}

static int64_t extract_json_int64(const char* content, const char* key) {
  if (!content || !key)
    return 0;
  char marker[128];
  snprintf(marker, sizeof(marker), "\"%s\":", key);
  const char* p = strstr(content, marker);
  if (!p)
    return 0;
  p += strlen(marker);
  while (*p == ' ' || *p == '\t' || *p == '\n') p++;
  int64_t value = 0;
  int neg = 0;
  if (*p == '-') {
    neg = 1;
    p++;
  }
  if (*p < '0' || *p > '9')
    return 0;
  while (*p >= '0' && *p <= '9') {
    value = value * 10 + (*p - '0');
    p++;
  }
  return neg ? -value : value;
}

static int extract_json_int(const char* content, const char* key) {
  return (int)extract_json_int64(content, key);
}

pc_status pc_sidecar_read(const char* doc_path, pc_sidecar** out_sidecar) {
  if (!doc_path || !out_sidecar) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }

  char path[4096];
  snprintf(path, sizeof(path), "%s.tynypdf.json", doc_path);

  size_t size;
  char* content = read_file(path, &size);
  if (!content) {
    return make_status(PC_ERR_IO, "sidecar not found");
  }

  // Simplified parsing - in reality would use proper JSON parser
  pc_sidecar* s = (pc_sidecar*)calloc(1, sizeof(pc_sidecar));
  if (!s) {
    free(content);
    return make_status(PC_ERR_MEMORY, "OOM");
  }

  s->format_version = extract_json_int(content, "format_version");
  if (s->format_version == 0)
    s->format_version = 1;

  s->document_sha256 = extract_json_string(content, "sha256");
  if (!s->document_sha256)
    s->document_sha256 = strdup("");

  s->document_path = strdup(doc_path);

  s->page_count = extract_json_int(content, "pages");
  s->modified_time = extract_json_int64(content, "modified");

  // annotations_json and unknown_json can be objects/arrays - extract raw JSON value
  s->annotations_json = extract_json_value(content, "annotations");
  if (!s->annotations_json)
    s->annotations_json = strdup("[]");

  s->unknown_json = extract_json_value(content, "unknown");
  if (!s->unknown_json)
    s->unknown_json = strdup("{}");

  s->is_stale = extract_json_int(content, "is_stale");
  s->stale_reason = (pc_staleness_reason)extract_json_int(content, "stale_reason");

  free(content);

  // Check staleness against current document (R5.1, R5.2)
  pc_status s_stale = pc_sidecar_check_staleness(doc_path, s);
  if (s_stale.code != PC_ERR_NONE) {
    // If we can't check staleness, don't fail the read - just leave is_stale=0
  }

  *out_sidecar = s;
  return make_status(PC_ERR_NONE, nullptr);
}

void pc_sidecar_free(pc_sidecar* sidecar) {
  if (!sidecar)
    return;
  free(sidecar->document_sha256);
  free(sidecar->document_path);
  free(sidecar->annotations_json);
  free(sidecar->unknown_json);
  free(sidecar);
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

  // Write PID and timestamp
  char lock_content[128];
  int len = snprintf(lock_content, sizeof(lock_content), "pid=%d time=%lld\n", getpid(),
                     (long long)time(nullptr));
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

/// Compute SHA256 fingerprint of document
/// Returns PC_ERR_NONE on success, out_fingerprint must be freed by caller
pc_status pc_sidecar_compute_fingerprint(const char* doc_path, char** out_fingerprint) {
  if (!doc_path || !out_fingerprint) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }

  FILE* f = fopen(doc_path, "rb");
  if (!f) {
    return make_status(PC_ERR_IO, "cannot open document");
  }

  // Read file in chunks and compute SHA256
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

  // Convert to hex string
  *out_fingerprint = (char*)malloc(65);  // 64 hex chars + null
  if (!*out_fingerprint) {
    return make_status(PC_ERR_MEMORY, "OOM");
  }
  for (int i = 0; i < 32; ++i) {
    snprintf(*out_fingerprint + i * 2, 3, "%02x", hash[i]);
  }

  return make_status(PC_ERR_NONE, nullptr);
}

/// Compute page count of document (stub - integrates with backend)
/// Returns PC_ERR_NONE on success
pc_status pc_sidecar_compute_page_count(const char* doc_path, int* out_page_count) {
  if (!doc_path || !out_page_count) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }

  // Stub: return 1 page for any existing file
  // Real implementation would use MuPDF backend
  FILE* f = fopen(doc_path, "rb");
  if (!f) {
    return make_status(PC_ERR_IO, "cannot open document");
  }
  fclose(f);

  *out_page_count = 1;
  return make_status(PC_ERR_NONE, nullptr);
}

/// Check staleness of sidecar against current document
/// Sets is_stale and stale_reason on the sidecar
/// Returns PC_ERR_NONE on success, PC_ERR_IO if document not accessible
pc_status pc_sidecar_check_staleness(const char* doc_path, pc_sidecar* sidecar) {
  if (!doc_path || !sidecar) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }

  // Default: not stale
  sidecar->is_stale = 0;
  sidecar->stale_reason = PC_STALE_NONE;

  // Check page count (R5.1)
  int current_pages = 0;
  pc_status s = pc_sidecar_compute_page_count(doc_path, &current_pages);
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  if (sidecar->page_count > 0 && current_pages != sidecar->page_count) {
    sidecar->is_stale = 1;
    sidecar->stale_reason = PC_STALE_PAGE_COUNT;
    return make_status(PC_ERR_NONE, nullptr);
  }

  // Check fingerprint (R5.2 - strong signal)
  char* current_fp = nullptr;
  s = pc_sidecar_compute_fingerprint(doc_path, &current_fp);
  if (s.code != PC_ERR_NONE) {
    free(current_fp);
    return s;
  }
  if (sidecar->document_sha256 && current_fp && strcmp(sidecar->document_sha256, current_fp) != 0) {
    sidecar->is_stale = 1;
    sidecar->stale_reason = PC_STALE_FINGERPRINT;
    free(current_fp);
    return make_status(PC_ERR_NONE, nullptr);
  }
  free(current_fp);

  // Check modification time (R5.2 - weak signal)
  int64_t current_mtime = file_mtime(doc_path);
  if (sidecar->modified_time > 0 && current_mtime != sidecar->modified_time) {
    sidecar->is_stale = 1;
    sidecar->stale_reason = PC_STALE_MODIFIED_TIME;
    return make_status(PC_ERR_NONE, nullptr);
  }

  return make_status(PC_ERR_NONE, nullptr);
}

/// Validate an annotation ID per RFC 4648 (lowercase base32, 10 chars, no padding)
/// Returns PC_ERR_NONE if valid, PC_ERR_ARGUMENT if invalid
pc_status pc_sidecar_validate_annotation_id(const char* annotation_id) {
  if (!annotation_id) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }

  // Must be exactly 10 characters
  size_t len = strlen(annotation_id);
  if (len != 10) {
    return make_status(PC_ERR_ARGUMENT, "annotation ID must be 10 characters");
  }

  // RFC 4648 lowercase base32 alphabet: a-z, 2-7
  // (excluding 0, 1, 8, 9 to avoid ambiguity)
  const char* base32_lower = "abcdefghijklmnopqrstuvwxyz234567";
  for (size_t i = 0; i < len; ++i) {
    char c = annotation_id[i];
    // Check if character is in the base32 alphabet
    int found = 0;
    for (int j = 0; base32_lower[j] != '\0'; ++j) {
      if (base32_lower[j] == c) {
        found = 1;
        break;
      }
    }
    if (!found) {
      return make_status(PC_ERR_ARGUMENT,
                         "annotation ID must be lowercase RFC 4648 base32 without padding");
    }
  }

  return make_status(PC_ERR_NONE, nullptr);
}