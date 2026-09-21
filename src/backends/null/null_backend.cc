#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "pdfcore/backend.h"
#include "pdfcore/page.h"
#include "pdfcore/status.h"

struct NullDoc {
  uint32_t page_count;
};

struct NullPage {
  uint32_t index;
};

static pc_status null_doc_open(const char* path, const char* password, void** out_backend_doc) {
  (void)path;
  (void)password;
  NullDoc* doc = (NullDoc*)std::malloc(sizeof(NullDoc));
  if (!doc)
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM"};
  doc->page_count = 5;
  *out_backend_doc = doc;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

static void null_doc_close(void* backend_doc) {
  std::free(backend_doc);
}

static uint32_t null_doc_page_count(void* backend_doc) {
  NullDoc* doc = static_cast<NullDoc*>(backend_doc);
  return doc->page_count;
}

static pc_status null_page_get(void* backend_doc, uint32_t index, void** out_backend_page) {
  (void)backend_doc;
  NullPage* page = (NullPage*)std::malloc(sizeof(NullPage));
  if (!page)
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM"};
  page->index = index;
  *out_backend_page = page;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

static pc_status null_page_render(void* backend_page, const pc_render_params* params,
                                  pc_pixmap* out_pixmap) {
  (void)backend_page;
  (void)params;
  NullPage* page = static_cast<NullPage*>(backend_page);
  uint32_t w = params->clip.x1 > 0 ? (uint32_t)(params->clip.x1 - params->clip.x0) : 100;
  uint32_t h = params->clip.y1 > 0 ? (uint32_t)(params->clip.y1 - params->clip.y0) : 100;
  uint32_t stride = (w * 4 + 3) & ~3;
  uint8_t* data = (uint8_t*)std::calloc(h * stride, 1);
  if (!data)
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM"};

  for (uint32_t y = 0; y < h; ++y) {
    for (uint32_t x = 0; x < w; ++x) {
      uint8_t* px = data + y * stride + x * 4;
      uint8_t v = (uint8_t)((page->index * 50 + x * 2 + y * 3) & 0xFF);
      px[0] = v;
      px[1] = v;
      px[2] = v;
      px[3] = 255;
    }
  }

  out_pixmap->width = w;
  out_pixmap->height = h;
  out_pixmap->stride = stride;
  out_pixmap->data = data;
  out_pixmap->format = 0;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

static void null_page_free(void* backend_page) {
  std::free(backend_page);
}

static void null_pixmap_free(pc_pixmap* pixmap) {
  if (pixmap && pixmap->data) {
    std::free(pixmap->data);
    pixmap->data = nullptr;
  }
}

static pc_status null_page_get_box(void* backend_doc, uint32_t index, pc_page_box* out) {
  (void)backend_doc;
  if (!out) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  if (index >= 5) {
    return {sizeof(pc_status), PC_ERR_RANGE, 0, "page index out of range"};
  }
  out->mediabox.x0 = 0;
  out->mediabox.y0 = 0;
  out->mediabox.x1 = 612;
  out->mediabox.y1 = 792;
  out->cropbox = out->mediabox;
  out->rotation = 0;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

static const char* null_get_last_error(void* backend_doc) {
  (void)backend_doc;
  return nullptr;
}

// R-M5: the null backend declares no capability; a capability query is answered 0 (unsupported)
// and find_tables reports PC_ERR_CAPABILITY, never an empty "no tables" list.
static int null_doc_has_capability(void* backend_doc, uint32_t capability) {
  (void)backend_doc;
  (void)capability;
  return 0;
}

static pc_status null_doc_find_tables(void* backend_doc, pc_rect* out_rects, size_t* out_count,
                                      size_t capacity) {
  (void)backend_doc;
  (void)out_rects;
  (void)capacity;
  if (!out_count) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  *out_count = 0;
  return {sizeof(pc_status), PC_ERR_CAPABILITY, 0, "capability not supported"};
}

pc_backend_api pc_null_backend_api = {
    .abi_major = 1,
    .abi_minor = 0,
    .struct_size = sizeof(pc_backend_api),
    .doc_open = null_doc_open,
    .doc_close = null_doc_close,
    .doc_page_count = null_doc_page_count,
    .page_get = null_page_get,
    .page_get_box = null_page_get_box,
    .page_render = null_page_render,
    .page_free = null_page_free,
    .pixmap_free = null_pixmap_free,
    .get_last_error = null_get_last_error,
    .doc_has_capability = null_doc_has_capability,
    .doc_find_tables = null_doc_find_tables,
};

const pc_backend_api* pc_null_backend_get_api(void) {
  return &pc_null_backend_api;
}