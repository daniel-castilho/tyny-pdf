#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfcore/sidecar.h"
#include "pdfcore/status.h"

static pc_status make_status(uint32_t code, const char* detail) {
  pc_status s = {};
  s.size = sizeof(pc_status);
  s.code = code;
  s.detail = detail;
  return s;
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
          p++;
          break;
        }
        depth--;
      } else if (depth == 0 && (*p == ',' || *p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')) {
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
  char* val = extract_json_value(content, key);
  if (!val)
    return nullptr;
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
  if (!doc_path || !out_sidecar)
    return make_status(PC_ERR_ARGUMENT, "null argument");

  char path[4096];
  snprintf(path, sizeof(path), "%s.tynypdf.json", doc_path);

  size_t size;
  char* content = read_file(path, &size);
  if (!content)
    return make_status(PC_ERR_IO, "sidecar not found");

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

  s->annotations_json = extract_json_value(content, "annotations");
  if (!s->annotations_json)
    s->annotations_json = strdup("[]");

  s->unknown_json = extract_json_value(content, "unknown");
  if (!s->unknown_json)
    s->unknown_json = strdup("{}");

  s->is_stale = extract_json_int(content, "is_stale");
  s->stale_reason = (pc_staleness_reason)extract_json_int(content, "stale_reason");

  free(content);

  // R2.2: a future format_version is loaded read-only; the caller may render but not mutate.
  if (s->format_version > PC_SIDECAR_FORMAT_VERSION_SUPPORTED) {
    static char version_detail[128];
    snprintf(version_detail, sizeof(version_detail), "format_version %d > supported %d",
             s->format_version, PC_SIDECAR_FORMAT_VERSION_SUPPORTED);
    s->read_only = 1;
    *out_sidecar = s;
    return make_status(PC_ERR_VERSION, version_detail);
  }

  // R5.1/R5.2: staleness is informational at read time; the writer enforces it.
  pc_status stale = pc_sidecar_check_staleness(doc_path, s);
  (void)stale;

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
  free(sidecar->stale_detail);
  free(sidecar);
}

pc_status pc_sidecar_validate_annotation_id(const char* annotation_id) {
  if (!annotation_id)
    return make_status(PC_ERR_ARGUMENT, "null argument");

  size_t len = strlen(annotation_id);
  if (len != 10)
    return make_status(PC_ERR_ARGUMENT, "annotation ID must be 10 characters");

  // RFC 4648 lowercase base32 alphabet: a-z plus digits 2-7 (0, 1, 8, 9 excluded).
  const char* base32_lower = "abcdefghijklmnopqrstuvwxyz234567";
  for (size_t i = 0; i < len; ++i) {
    char c = annotation_id[i];
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