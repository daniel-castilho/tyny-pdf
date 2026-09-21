#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "pdfcore/platform.h"
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

static int file_exists(const char* path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static int64_t file_mtime(const char* path) {
  struct stat st;
  if (stat(path, &st) != 0)
    return 0;
  return (int64_t)st.st_mtime;
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

  json_write_key_value_int(buf, &pos, cap, 2, "modified", sidecar->modified_time);
  json_write_key_value_int(buf, &pos, cap, 2, "pages", sidecar->page_count);
  json_write_key_value_str(buf, &pos, cap, 2, "path",
                           sidecar->document_path ? sidecar->document_path : "");
  json_write_key_value_str(buf, &pos, cap, 2, "sha256",
                           sidecar->document_sha256 ? sidecar->document_sha256 : "");

  json_append_str(buf, &pos, cap, "  ");
  json_append_str(buf, &pos, cap, "},\n");

  // format_version (writer never lowers: a caller that passes 0 gets the current 1)
  int format_version = sidecar->format_version ? sidecar->format_version : 1;
  json_write_key_value_int(buf, &pos, cap, 1, "format_version", format_version);

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

  json_append_str(buf, &pos, cap, "}\n");

  return (pos < cap) ? (int)pos : -1;
}

static pc_status pc_sidecar_write_internal(const char* doc_path, const pc_sidecar* sidecar,
                                           int force) {
  if (!doc_path || !sidecar) {
    return make_status(PC_ERR_ARGUMENT, "null argument");
  }

  if (!sidecar->document_path || strcmp(sidecar->document_path, doc_path) != 0) {
    return make_status(PC_ERR_ARGUMENT, "document path mismatch (R4.2)");
  }

  // R2.2: a sidecar from a future format_version is read-only; mutation is refused.
  if (sidecar->format_version > PC_SIDECAR_FORMAT_VERSION_SUPPORTED) {
    return make_status(PC_ERR_STATE, "read-only sidecar (future format_version)");
  }

  // R5.1: writes to a stale sidecar are disabled until the caller passes force explicitly.
  if (!force && sidecar->is_stale) {
    const char* detail = sidecar->stale_detail ? sidecar->stale_detail : "sidecar is stale";
    return make_status(PC_ERR_STATE, detail);
  }

  // Check lock (R3.2)
  char lock_path[4096];
  snprintf(lock_path, sizeof(lock_path), "%s.tynypdf.lock", doc_path);
  if (file_exists(lock_path)) {
    int64_t lock_age = time(nullptr) - file_mtime(lock_path);
    if (lock_age < 300) {
      return make_status(PC_ERR_STATE, "sidecar locked");
    }
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

pc_status pc_sidecar_write(const char* doc_path, const pc_sidecar* sidecar) {
  return pc_sidecar_write_internal(doc_path, sidecar, 0);
}

pc_status pc_sidecar_write_force(const char* doc_path, const pc_sidecar* sidecar) {
  return pc_sidecar_write_internal(doc_path, sidecar, 1);
}