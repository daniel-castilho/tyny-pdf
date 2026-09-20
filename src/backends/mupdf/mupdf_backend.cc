#include <mupdf/fitz.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "pdfcore/backend.h"
#include "pdfcore/page.h"
#include "pdfcore/status.h"

struct MupdfDoc {
  fz_context* ctx;
  fz_document* doc;
};

struct MupdfPage {
  fz_context* ctx;
  fz_page* page;
  fz_rect mediabox;
};

static pc_status mupdf_doc_open(const char* path, const char* password, void** out_backend_doc)
    __attribute__((used));
static pc_status mupdf_doc_open(const char* path, const char* password, void** out_backend_doc) {
  fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
  if (!ctx) {
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "fz_new_context failed"};
  }

  fz_register_document_handlers(ctx);
  fz_try(ctx) {
    fz_document* doc = fz_open_document(ctx, path);
    if (password && *password) {
      if (!fz_authenticate_password(ctx, doc, password)) {
        fz_drop_document(ctx, doc);
        fz_drop_context(ctx);
        return {sizeof(pc_status), PC_ERR_PASSWORD, 0, "Invalid password"};
      }
    }

    MupdfDoc* doc_wrapper = (MupdfDoc*)std::malloc(sizeof(MupdfDoc));
    if (!doc_wrapper) {
      fz_drop_document(ctx, doc);
      fz_drop_context(ctx);
      return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM"};
    }
    doc_wrapper->ctx = ctx;
    doc_wrapper->doc = doc;
    *out_backend_doc = doc_wrapper;
    return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
  }
  fz_catch(ctx) {
    fz_drop_context(ctx);
    return {sizeof(pc_status), PC_ERR_CORRUPT, 0, fz_caught_message(ctx)};
  }
  return {sizeof(pc_status), PC_ERR_CORRUPT, 0, "unreachable"};
}

static void mupdf_doc_close(void* backend_doc) __attribute__((used));
static void mupdf_doc_close(void* backend_doc) {
  if (!backend_doc)
    return;
  MupdfDoc* doc = static_cast<MupdfDoc*>(backend_doc);
  if (doc->doc)
    fz_drop_document(doc->ctx, doc->doc);
  if (doc->ctx)
    fz_drop_context(doc->ctx);
  std::free(doc);
}

static uint32_t mupdf_doc_page_count(void* backend_doc) __attribute__((used));
static uint32_t mupdf_doc_page_count(void* backend_doc) {
  MupdfDoc* doc = static_cast<MupdfDoc*>(backend_doc);
  return fz_count_pages(doc->ctx, doc->doc);
}

static pc_status mupdf_page_get_box(void* backend_doc, uint32_t index, pc_page_box* out)
    __attribute__((used));
static pc_status mupdf_page_get_box(void* backend_doc, uint32_t index, pc_page_box* out) {
  if (!out) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  MupdfDoc* doc = static_cast<MupdfDoc*>(backend_doc);
  uint32_t count = fz_count_pages(doc->ctx, doc->doc);
  if (index >= count) {
    return {sizeof(pc_status), PC_ERR_RANGE, 0, "page index out of range"};
  }
  fz_page* page = fz_load_page(doc->ctx, doc->doc, index);
  if (!page) {
    return {sizeof(pc_status), PC_ERR_CORRUPT, 0, "fz_load_page failed"};
  }
  fz_rect mediabox = fz_bound_page(doc->ctx, page);
  fz_drop_page(doc->ctx, page);

  out->mediabox.x0 = mediabox.x0;
  out->mediabox.y0 = mediabox.y0;
  out->mediabox.x1 = mediabox.x1;
  out->mediabox.y1 = mediabox.y1;
  out->cropbox = out->mediabox;
  out->rotation = 0;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

static pc_status mupdf_page_get(void* backend_doc, uint32_t index, void** out_backend_page)
    __attribute__((used));
static pc_status mupdf_page_get(void* backend_doc, uint32_t index, void** out_backend_page) {
  MupdfDoc* doc = static_cast<MupdfDoc*>(backend_doc);
  uint32_t count = fz_count_pages(doc->ctx, doc->doc);
  if (index >= count) {
    return {sizeof(pc_status), PC_ERR_RANGE, 0, "page index out of range"};
  }
  fz_page* page = fz_load_page(doc->ctx, doc->doc, index);
  if (!page) {
    return {sizeof(pc_status), PC_ERR_CORRUPT, 0, "fz_load_page failed"};
  }
  MupdfPage* page_wrapper = (MupdfPage*)std::malloc(sizeof(MupdfPage));
  if (!page_wrapper) {
    fz_drop_page(doc->ctx, page);
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM"};
  }
  page_wrapper->ctx = doc->ctx;
  page_wrapper->page = page;
  page_wrapper->mediabox = fz_bound_page(doc->ctx, page);
  *out_backend_page = page_wrapper;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

// R13.1 - the mupdf backend SHALL render a PDF page using the MuPDF engine.
// R13.2 - the mupdf backend SHALL return deterministic pixels for the same page index.
static pc_status mupdf_page_render(void* backend_page, const pc_render_params* params,
                                   pc_pixmap* out_pixmap) __attribute__((used));
static pc_status mupdf_page_render(void* backend_page, const pc_render_params* params,
                                   pc_pixmap* out_pixmap) {
  if (!backend_page || !params || !out_pixmap) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  MupdfPage* page = static_cast<MupdfPage*>(backend_page);
  fz_context* ctx = page->ctx;

  double scale = params->dpi ? (double)params->dpi / 72.0 : 1.0;
  fz_rect area = page->mediabox;
  if (params->clip.x1 > params->clip.x0 && params->clip.y1 > params->clip.y0) {
    area = fz_make_rect((float)params->clip.x0, (float)params->clip.y0, (float)params->clip.x1,
                        (float)params->clip.y1);
  }

  fz_matrix ctm = fz_scale((float)scale, (float)scale);
  fz_irect bbox = fz_round_rect(fz_transform_rect(area, ctm));
  if (bbox.x1 <= bbox.x0 || bbox.y1 <= bbox.y0) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "empty render area"};
  }

  fz_pixmap* pix = nullptr;
  fz_device* dev = nullptr;
  fz_var(pix);
  fz_var(dev);

  fz_try(ctx) {
    pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bbox, nullptr, 1);
    fz_clear_pixmap_with_value(ctx, pix, 0xff);
    dev = fz_new_draw_device(ctx, ctm, pix);
    fz_run_page(ctx, page->page, dev, fz_identity, nullptr);
    if (params->render_annots) {
      fz_run_page_annots(ctx, page->page, dev, fz_identity, nullptr);
    }
    fz_close_device(ctx, dev);
  }
  fz_catch(ctx) {
    if (dev)
      fz_drop_device(ctx, dev);
    if (pix)
      fz_drop_pixmap(ctx, pix);
    return {sizeof(pc_status), PC_ERR_CORRUPT, 0, fz_caught_message(ctx)};
  }
  fz_drop_device(ctx, dev);

  uint32_t width = (uint32_t)(bbox.x1 - bbox.x0);
  uint32_t height = (uint32_t)(bbox.y1 - bbox.y0);
  uint32_t stride = width * 4;
  int src_stride = fz_pixmap_stride(ctx, pix);
  uint8_t* data = (uint8_t*)std::malloc((size_t)height * stride);
  if (!data) {
    fz_drop_pixmap(ctx, pix);
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM"};
  }
  const uint8_t* samples = fz_pixmap_samples(ctx, pix);
  for (uint32_t y = 0; y < height; ++y) {
    std::memcpy(data + (size_t)y * stride, samples + (size_t)y * src_stride, stride);
  }
  fz_drop_pixmap(ctx, pix);

  out_pixmap->width = width;
  out_pixmap->height = height;
  out_pixmap->stride = stride;
  out_pixmap->data = data;
  out_pixmap->format = 0;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

static void mupdf_page_free(void* backend_page) __attribute__((used));
static void mupdf_page_free(void* backend_page) {
  MupdfPage* page = static_cast<MupdfPage*>(backend_page);
  if (page->page && page->ctx)
    fz_drop_page(page->ctx, page->page);
  std::free(page);
}

static void mupdf_pixmap_free(pc_pixmap* pixmap) __attribute__((used));
static void mupdf_pixmap_free(pc_pixmap* pixmap) {
  if (pixmap && pixmap->data) {
    std::free(pixmap->data);
    pixmap->data = nullptr;
  }
}

static const char* mupdf_get_last_error(void* backend_doc) __attribute__((used));
static const char* mupdf_get_last_error(void* backend_doc) {
  (void)backend_doc;
  return "MuPDF error";
}

pc_backend_api pc_mupdf_backend_api = {
    .abi_major = 1,
    .abi_minor = 0,
    .struct_size = sizeof(pc_backend_api),
    .doc_open = mupdf_doc_open,
    .doc_close = mupdf_doc_close,
    .doc_page_count = mupdf_doc_page_count,
    .page_get = mupdf_page_get,
    .page_get_box = mupdf_page_get_box,
    .page_render = mupdf_page_render,
    .page_free = mupdf_page_free,
    .pixmap_free = mupdf_pixmap_free,
    .get_last_error = mupdf_get_last_error,
};

const pc_backend_api* pc_mupdf_backend_get_api(void) {
  return &pc_mupdf_backend_api;
}