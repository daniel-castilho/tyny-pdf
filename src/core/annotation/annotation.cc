#include "pdfcore/annotation.h"
#include "pdfcore/status.h"
#include "pdfcore/sha256.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cstdio>

/// Generate RFC 4648 base32 ID (10 chars, lowercase, no padding)
static void generate_annotation_id(char* out_id) {
  static const char* base32 = "abcdefghijklmnopqrstuvwxyz234567";
  static int seeded = 0;
  static uint64_t counter = 0;
  if (!seeded) {
    srand((unsigned int)time(nullptr));
    seeded = 1;
  }
  // Use time + random + counter for uniqueness
  uint64_t seed = (uint64_t)time(nullptr) * 1000000 + (uint64_t)rand() + counter++;
  // Debug: print ID being generated
  fprintf(stderr, "DEBUG gen_id: counter=%llu, seed=%llu\n", (unsigned long long)counter, (unsigned long long)seed);
  for (int i = 0; i < 10; ++i) {
    int idx = seed % 32;
    out_id[i] = base32[idx];
    seed /= 32;
    fprintf(stderr, "DEBUG gen_id: i=%d, idx=%d, char=%c, seed=%llu\n", i, idx, base32[idx], (unsigned long long)seed);
  }
  out_id[10] = '\0';
  fprintf(stderr, "DEBUG gen_id: final ID len=%zu: '%s'\n", strlen(out_id), out_id);
}

pc_annotation_list* pc_annotation_list_create(void) {
  pc_annotation_list* list = (pc_annotation_list*)calloc(1, sizeof(pc_annotation_list));
  if (!list) return nullptr;
  list->capacity = 16;
  list->annotations = (pc_annotation**)calloc(list->capacity, sizeof(pc_annotation*));
  if (!list->annotations) {
    free(list);
    return nullptr;
  }
  return list;
}

void pc_annotation_list_free(pc_annotation_list* list) {
  if (!list) return;
  for (size_t i = 0; i < list->count; ++i) {
    pc_annotation_free(list->annotations[i]);
  }
  free(list->annotations);
  free(list);
}

static int ensure_annotation_capacity(pc_annotation_list* list) {
  if (list->count >= list->capacity) {
    size_t new_cap = list->capacity * 2;
    pc_annotation** new_arr = (pc_annotation**)realloc(list->annotations, new_cap * sizeof(pc_annotation*));
    if (!new_arr) return 0;
    list->annotations = (pc_annotation**)list->annotations; // suppress unused warning
    list->annotations = (pc_annotation**)realloc(list->annotations, new_cap * sizeof(pc_annotation*));
    if (!list->annotations) return 0;
    list->capacity = new_cap;
  }
  return 1;
}

pc_status pc_annotation_list_add(pc_annotation_list* list, pc_annotation* annot) {
  if (!list || !annot) {
    pc_status s = {}; s.size = sizeof(pc_status); s.code = PC_ERR_ARGUMENT; s.detail = "null argument"; return s;
  }
  if (!ensure_annotation_capacity((pc_annotation_list*)list)) {
    pc_status s = {}; s.size = sizeof(pc_status); s.code = PC_ERR_MEMORY; s.detail = "OOM"; return s;
  }
  pc_annotation_list* list_mut = (pc_annotation_list*)list;
  list_mut->annotations[list_mut->count++] = annot;
  pc_status s = {}; s.size = sizeof(pc_status); s.code = PC_ERR_NONE; s.detail = nullptr; return s;
}

pc_status pc_annotation_list_remove(pc_annotation_list* list, const char* id) {
  if (!list || !id) {
    pc_status s = {}; s.size = sizeof(pc_status); s.code = PC_ERR_ARGUMENT; s.detail = "null argument"; return s;
  }
  // Debug: print ID being searched for
  fprintf(stderr, "DEBUG remove: searching for ID: %s, list count: %zu\n", id, list->count);
  for (size_t i = 0; i < list->count; ++i) {
    if (list->annotations[i] && strcmp(list->annotations[i]->id, id) == 0) {
      fprintf(stderr, "DEBUG remove: shifting, count before: %zu\n", list->count);
      fprintf(stderr, "DEBUG remove: before shift - index 0 ptr=%p, index 1 ptr=%p\n", (void*)list->annotations[0], (void*)list->annotations[1]);
      for (size_t j = i; j + 1 < list->count; ++j) {
        fprintf(stderr, "DEBUG remove: shifting j=%zu, moving index %zu (ptr=%p) to %zu\n", j, j+1, (void*)list->annotations[j+1], j);
        list->annotations[j] = list->annotations[j + 1];
      }
      list->count--;
      fprintf(stderr, "DEBUG remove: after shift, count=%zu, index 0 ptr=%p\n", list->count, (void*)list->annotations[0]);
      pc_status s = {}; s.size = sizeof(pc_status); s.code = PC_ERR_NONE; s.detail = nullptr; return s;
    }
  }
  pc_status s = {};
  s.size = sizeof(pc_status);
  s.code = PC_ERR_NOT_FOUND;
  s.detail = "annotation not found";
  return s;
}

pc_annotation* pc_annotation_list_find(pc_annotation_list* list, const char* id) {
  if (!list || !id) return nullptr;
  for (size_t i = 0; i < list->count; ++i) {
    if (list->annotations[i] && strcmp(list->annotations[i]->id, id) == 0) {
      return list->annotations[i];
    }
  }
  return nullptr;
}

size_t pc_annotation_list_count_for_page(pc_annotation_list* list, uint32_t page_index) {
  if (!list) return 0;
  size_t count = 0;
  for (size_t i = 0; i < list->count; ++i) {
    if (list->annotations[i] && list->annotations[i]->page_index == page_index) {
      count++;
    }
  }
  return count;
}

void pc_annotation_list_get_for_page(pc_annotation_list* list, uint32_t page_index, pc_annotation** out, size_t max_count) {
  if (!list || !out || max_count == 0) return;
  size_t j = 0;
  for (size_t i = 0; i < list->count && j < max_count; ++i) {
    if (list->annotations[i] && list->annotations[i]->page_index == page_index) {
      out[j++] = list->annotations[i];
    }
  }
}

pc_annotation* pc_annotation_create(pc_annotation_type type, uint32_t page_index, const pc_rect* rect) {
  pc_annotation* annot = (pc_annotation*)calloc(1, sizeof(pc_annotation));
  if (!annot) return nullptr;

  annot->type = type;
  annot->page_index = (uint32_t)page_index;
  if (rect) {
    annot->rect = *rect;
  } else {
    annot->rect.x0 = 0;
    annot->rect.y0 = 0;
    annot->rect.x1 = 0;
    annot->rect.y1 = 0;
  }

  // Generate RFC 4648 base32 ID (R2.3)
  generate_annotation_id(annot->id = (char*)malloc(11));

  // Default dates
  time_t now = time(nullptr);
  struct tm tm_info;
  gmtime_r(&now, &tm_info);
  char date_buf[32];
  strftime(date_buf, sizeof(date_buf), "%Y-%m-%dT%H:%M:%SZ", &tm_info);
  annot->creation_date = strdup(date_buf);
  annot->modification_date = strdup(date_buf);

  // Default color (yellow for highlights, red for others)
  if (type == PC_ANNOT_HIGHLIGHT) {
    annot->color[0] = 1.0f; annot->color[1] = 1.0f; annot->color[2] = 0.0f;
  } else {
    annot->color[0] = 1.0f; annot->color[1] = 0.0f; annot->color[2] = 0.0f;
  }
  annot->opacity = 0.5f;
  annot->border_width = 1.0f;
  annot->flags = PC_ANNOT_FLAG_PRINT;

  return annot;
}

void pc_annotation_free(pc_annotation* annot) {
  if (!annot) return;
  free(annot->id);
  free(annot->contents);
  free(annot->author);
  free(annot->creation_date);
  free(annot->modification_date);
  free(annot->custom_data);
  free(annot->quad_points);
  free(annot);
}

char* pc_annotation_to_json(const pc_annotation* annot) {
  if (!annot) return nullptr;
  size_t cap = 8192;  // Increased buffer size
  char* buf = (char*)malloc(cap);
  if (!buf) return nullptr;
  size_t pos = 0;

  auto append = [&](const char* s) -> int {
    size_t len = strlen(s);
    if (pos + len >= cap) return 0;
    memcpy(buf + pos, s, len);
    pos += len;
    return 1;
  };
  auto append_escaped = [&](const char* s) -> int {
    if (!s) s = "";
    if (pos + 1 >= cap) return 0;
    buf[pos++] = '"';
    for (const char* p = s; *p; ++p) {
      unsigned char c = *p;
      char esc[7] = {0};
      int elen = 0;
      switch (c) {
        case '"':  esc[0] = '\\'; esc[1] = '"'; elen = 2; break;
        case '\\': esc[0] = '\\'; esc[1] = '\\'; elen = 2; break;
        case '\b': esc[0] = '\\'; esc[1] = 'b'; elen = 2; break;
        case '\f': esc[0] = '\\'; esc[1] = 'f'; elen = 2; break;
        case '\n': esc[0] = '\\'; esc[1] = 'n'; elen = 2; break;
        case '\r': esc[0] = '\\'; esc[1] = 'r'; elen = 2; break;
        case '\t': esc[0] = '\\'; esc[1] = 't'; elen = 2; break;
        default:
          if (c < 0x20) { snprintf(esc, sizeof(esc), "\\u%04x", c); elen = 6; }
          break;
      }
      if (elen) {
        if (pos + elen >= cap) return 0;
        memcpy(buf + pos, esc, elen);
        pos += elen;
      } else {
        if (pos + 1 >= cap) return 0;
        buf[pos++] = *p;
      }
    }
    if (pos + 1 < cap) buf[pos++] = '"';
    return 1;
  };

  if (!append("{")) return nullptr;
  append("\"id\": "); append_escaped(annot->id); append(",");
  append("\"type\": "); // type as int for simplicity
  // We'll just output the type enum value
  char tmp[32];
  snprintf(tmp, sizeof(tmp), "%d", annot->type);
  append(tmp); append(",");
  append("\"page_index\": "); snprintf(tmp, sizeof(tmp), "%u", annot->page_index); append(tmp); append(",");
  append("\"rect\": {");
  append("\"x0\": "); snprintf(tmp, sizeof(tmp), "%.6g", annot->rect.x0); append(tmp); append(",");
  append("\"y0\": "); snprintf(tmp, sizeof(tmp), "%.6g", annot->rect.y0); append(tmp); append(",");
  append("\"x1\": "); snprintf(tmp, sizeof(tmp), "%.6g", annot->rect.x1); append(tmp); append(",");
  append("\"y1\": "); snprintf(tmp, sizeof(tmp), "%.6g", annot->rect.y1); append(tmp);
  append("},");
  append("\"contents\": "); append_escaped(annot->contents); append(",");
  append("\"author\": "); append_escaped(annot->author); append(",");
  append("\"creation_date\": "); append_escaped(annot->creation_date); append(",");
  append("\"modification_date\": "); append_escaped(annot->modification_date); append(",");
  append("\"flags\": "); snprintf(tmp, sizeof(tmp), "%u", annot->flags); append(tmp); append(",");
  append("\"color\": ["); snprintf(tmp, sizeof(tmp), "%.6g", annot->color[0]); append(tmp); append(",");
  snprintf(tmp, sizeof(tmp), "%.6g", annot->color[1]); append(tmp); append(",");
  snprintf(tmp, sizeof(tmp), "%.6g", annot->color[2]); append(tmp);
  append("],");
  append("\"opacity\": "); snprintf(tmp, sizeof(tmp), "%.6g", annot->opacity); append(tmp); append(",");
  append("\"border_width\": "); snprintf(tmp, sizeof(tmp), "%.6g", annot->border_width); append(tmp); append(",");
  append("\"custom_data\": "); append(annot->custom_data ? annot->custom_data : "{}");
  append("}");

  return buf;
}

pc_annotation* pc_annotation_from_json(const char* json) {
  // Simplified - would need proper JSON parser
  (void)json;
  return nullptr;
}

char* pc_annotation_list_to_json(const pc_annotation_list* list) {
  if (!list) return nullptr;
  size_t cap = 16384;  // Increased buffer size for list JSON
  char* buf = (char*)malloc(cap);
  if (!buf) return nullptr;
  size_t pos = 0;

  auto append = [&](const char* s) {
    size_t len = strlen(s);
    if (pos + len >= cap) return 0;
    memcpy(buf + pos, s, len);
    pos += len;
    return 1;
  };

  if (!append("[") ) return nullptr;
  for (size_t i = 0; i < list->count; ++i) {
    char* ann_json = pc_annotation_to_json(list->annotations[i]);
    if (!ann_json) { free(buf); return nullptr; }
    size_t len = strlen(ann_json);
    if (pos + len >= cap) { free(ann_json); free(buf); return nullptr; }
    memcpy(buf + pos, ann_json, len);
    pos += len;
    free(ann_json);
    if (i + 1 < list->count) {
      if (pos + 1 >= cap) { free(buf); return nullptr; }
      buf[pos++] = ',';
    }
  }
  if (pos + 1 >= cap) { free(buf); return nullptr; }
  buf[pos++] = ']';
  buf[pos] = '\0';
  return buf;
}

pc_annotation_list* pc_annotation_list_from_json(const char* json) {
  (void)json;
  return nullptr;
}