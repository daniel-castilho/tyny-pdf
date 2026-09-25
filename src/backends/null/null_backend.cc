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
  if (capability == PC_CAP_FORMS)
    return 0;
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

// R-M5: the null backend declares PC_CAP_FACE_COVERAGE off (has_capability -> 0), so
// face_count reports 0 faces and face_coverage is capability not supported, never a "nothing
// can be covered" lie that would render tofu in a strict sequence.
static uint32_t null_face_count(void* backend_doc) {
  (void)backend_doc;
  return 0;
}

static pc_status null_face_coverage(void* backend_doc, uint32_t face, uint32_t codepoint,
                                    int* out_has) {
  (void)backend_doc;
  (void)face;
  (void)codepoint;
  (void)out_has;
  return {sizeof(pc_status), PC_ERR_CAPABILITY, 0, "capability not supported"};
}

// R-M5: the null backend declares PC_CAP_TEXT_LAYOUT off, so page_text_layout answers
// PC_ERR_CAPABILITY - never an empty "no text" layout that a caller could mistake for a
// textless page.
static pc_status null_page_text_layout(void* backend_page, char** out_utf8, pc_text_box** out_boxes,
                                       uint32_t* out_count) {
  (void)backend_page;
  if (!out_utf8 || !out_boxes || !out_count) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  *out_utf8 = nullptr;
  *out_boxes = nullptr;
  *out_count = 0;
  return {sizeof(pc_status), PC_ERR_CAPABILITY, 0, "capability not supported"};
}

static void null_page_text_layout_free(char* utf8, pc_text_box* boxes) {
  (void)utf8;
  (void)boxes;
}

static pc_status null_form_list_fields(const pc_backend_api* api, void* backend_doc,
                                       pc_form_list* out_list) {
  (void)api;
  (void)backend_doc;
  (void)out_list;
  return {sizeof(pc_status), PC_ERR_CAPABILITY, 0, "forms not supported"};
}

static void null_form_list_free(pc_form_list* list) {
  (void)list;
}

static pc_status null_form_fdf_export(const pc_backend_api* api, void* backend_doc,
                                      pc_fdf* out_fdf) {
  (void)api;
  (void)backend_doc;
  (void)out_fdf;
  return {sizeof(pc_status), PC_ERR_CAPABILITY, 0, "forms not supported"};
}

static pc_status null_form_fdf_import(const pc_backend_api* api, void* backend_doc,
                                      const char* fdf_data, size_t fdf_size) {
  (void)api;
  (void)backend_doc;
  (void)fdf_data;
  (void)fdf_size;
  return {sizeof(pc_status), PC_ERR_CAPABILITY, 0, "forms not supported"};
}

static void null_form_fdf_free(pc_fdf* fdf) {
  (void)fdf;
}

// abi 1.4 (story 7.4): the null backend declares no PC_CAP_FORMS, so flatten answers
// PC_ERR_CAPABILITY - never a silent no-op that could be mistaken for a baked file.
static pc_status null_form_flatten(const pc_backend_api* api, void* backend_doc,
                                   const char* out_path) {
  (void)api;
  (void)backend_doc;
  (void)out_path;
  return {sizeof(pc_status), PC_ERR_CAPABILITY, 0, "forms not supported"};
}

pc_backend_api pc_null_backend_api = {
    .abi_major = PC_BACKEND_API_VERSION_MAJOR,
    .abi_minor = PC_BACKEND_API_VERSION_MINOR,
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
    .face_count = null_face_count,
    .face_coverage = null_face_coverage,
    .page_text_layout = null_page_text_layout,
    .page_text_layout_free = null_page_text_layout_free,
    .form_list_fields = null_form_list_fields,
    .form_list_free = null_form_list_free,
    .form_fdf_export = null_form_fdf_export,
    .form_fdf_import = null_form_fdf_import,
    .form_fdf_free = null_form_fdf_free,
    .form_flatten = null_form_flatten,
};

const pc_backend_api* pc_null_backend_get_api(void) {
  return &pc_null_backend_api;
}

extern "C" pc_status pc_backend_get_api(const pc_backend_api** out_api, uint32_t abi_major,
                                        uint32_t /*abi_minor*/) {
  if (!out_api) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null out_api"};
  }
  if (abi_major != PC_BACKEND_API_VERSION_MAJOR) {
    return {sizeof(pc_status), PC_ERR_VERSION, 0, "backend ABI major mismatch"};
  }
  *out_api = &pc_null_backend_api;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}