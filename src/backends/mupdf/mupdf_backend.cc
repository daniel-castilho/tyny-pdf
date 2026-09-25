#include <mupdf/fitz.h>
#include <mupdf/pdf.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>

#include "exception_bridge.h"
#include "pdfcore/backend.h"
#include "pdfcore/forms.h"
#include "pdfcore/page.h"
#include "pdfcore/status.h"

// R-M2 (ADR-0011 R-M7): per-document contexts MUST NOT share one lock set - fz_locks_default is a
// single process-wide mutex array that serialises every context (mupdf-rs #260: 13.3x under 10
// threads). Each doc allocates its own FZ_LOCK_MAX recursive mutexes (mupdf-rs #263 pattern).
static void mupdf_ctx_lock(void* user, int lock);
static void mupdf_ctx_unlock(void* user, int lock);

struct MupdfLocks {
  std::recursive_mutex mutex[FZ_LOCK_MAX];
  fz_locks_context locks;

  MupdfLocks() {
    locks.user = this;
    locks.lock = &mupdf_ctx_lock;
    locks.unlock = &mupdf_ctx_unlock;
  }
};

static void mupdf_ctx_lock(void* user, int lock) {
  MupdfLocks* m = static_cast<MupdfLocks*>(user);
  if (lock >= 0 && lock < FZ_LOCK_MAX)
    m->mutex[lock].lock();
}

static void mupdf_ctx_unlock(void* user, int lock) {
  MupdfLocks* m = static_cast<MupdfLocks*>(user);
  if (lock >= 0 && lock < FZ_LOCK_MAX)
    m->mutex[lock].unlock();
}

struct MupdfDoc {
  fz_context* ctx;
  fz_document* doc;
  MupdfLocks* locks;
};

struct MupdfPage {
  fz_context* ctx;
  fz_page* page;
  fz_rect mediabox;
};

static pc_status mupdf_doc_open(const char* path, const char* password, void** out_backend_doc)
    __attribute__((used));
static pc_status mupdf_doc_open(const char* path, const char* password, void** out_backend_doc) {
  if (!path || !out_backend_doc) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }

  // Use MupdfLocks which has the mutex array and fz_locks_context
  MupdfLocks* locks = new (std::nothrow) MupdfLocks();
  if (!locks) {
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM allocating per-doc lock set"};
  }

  fz_context* ctx = fz_new_context(nullptr, &locks->locks, FZ_STORE_UNLIMITED);
  if (!ctx) {
    delete locks;
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "fz_new_context failed"};
  }

  fz_register_document_handlers(ctx);

  void* out_doc = nullptr;
  pc_status s = mupdf::run_guarded(
      ctx,
      [&]() -> pc_status {
        fz_document* doc = fz_open_document(ctx, path);
        if (password && *password && !fz_authenticate_password(ctx, doc, password)) {
          fz_drop_document(ctx, doc);
          return {sizeof(pc_status), PC_ERR_PASSWORD, 0, "Invalid password"};
        }
        MupdfDoc* wrapper = (MupdfDoc*)std::malloc(sizeof(MupdfDoc));
        if (!wrapper) {
          fz_drop_document(ctx, doc);
          return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM"};
        }
        wrapper->ctx = ctx;
        wrapper->doc = doc;
        wrapper->locks = locks;
        out_doc = wrapper;
        return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
      },
      "open failed");
  if (s.code != PC_ERR_NONE) {
    fz_drop_context(ctx);
    delete locks;
    return s;
  }
  *out_backend_doc = out_doc;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
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
  // Free the MupdfLocks struct (which contains the mutexes and fz_locks_context)
  if (doc->locks) {
    delete doc->locks;
  }
  std::free(doc);
}

static uint32_t mupdf_doc_page_count(void* backend_doc) __attribute__((used));
static uint32_t mupdf_doc_page_count(void* backend_doc) {
  MupdfDoc* doc = static_cast<MupdfDoc*>(backend_doc);
  uint32_t count = 0;
  pc_status s = mupdf::run_guarded(
      doc->ctx,
      [&]() -> pc_status {
        count = fz_count_pages(doc->ctx, doc->doc);
        return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
      },
      "count pages failed");
  if (s.code != PC_ERR_NONE) {
    return 0;
  }
  return count;
}

static pc_status mupdf_page_get_box(void* backend_doc, uint32_t index, pc_page_box* out)
    __attribute__((used));
static pc_status mupdf_page_get_box(void* backend_doc, uint32_t index, pc_page_box* out) {
  if (!out) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  MupdfDoc* doc = static_cast<MupdfDoc*>(backend_doc);
  uint32_t count = mupdf_doc_page_count(backend_doc);
  if (index >= count) {
    return {sizeof(pc_status), PC_ERR_RANGE, 0, "page index out of range"};
  }
  fz_rect mediabox{};
  pc_status s = mupdf::run_guarded(
      doc->ctx,
      [&]() -> pc_status {
        fz_page* page = fz_load_page(doc->ctx, doc->doc, index);
        if (!page) {
          return {sizeof(pc_status), PC_ERR_CORRUPT, 0, "page load failed"};
        }
        mediabox = fz_bound_page(doc->ctx, page);
        fz_drop_page(doc->ctx, page);  // the page is a local: drop it or the whole
                                       // loaded subtree leaks (found by the 7.4
                                       // flatten test's LeakSanitizer run)
        return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
      },
      "load page failed");
  if (s.code != PC_ERR_NONE) {
    return s;
  }

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
  if (!out_backend_page) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  MupdfDoc* doc = static_cast<MupdfDoc*>(backend_doc);
  uint32_t count = mupdf_doc_page_count(backend_doc);
  if (index >= count) {
    return {sizeof(pc_status), PC_ERR_RANGE, 0, "page index out of range"};
  }
  void* out_page = nullptr;
  pc_status s = mupdf::run_guarded(
      doc->ctx,
      [&]() -> pc_status {
        fz_page* page = fz_load_page(doc->ctx, doc->doc, index);
        MupdfPage* wrapper = (MupdfPage*)std::malloc(sizeof(MupdfPage));
        if (!wrapper) {
          fz_drop_page(doc->ctx, page);
          return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM"};
        }
        wrapper->ctx = doc->ctx;
        wrapper->page = page;
        wrapper->mediabox = fz_bound_page(doc->ctx, page);
        out_page = wrapper;
        return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
      },
      "load page failed");
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  *out_backend_page = out_page;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

// R13.1/R13.2 - the mupdf backend SHALL render a page with MuPDF and SHALL repeat bytes.
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
  pc_status r = mupdf::run_guarded(
      ctx,
      [&]() -> pc_status {
        pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bbox, nullptr, 1);
        fz_clear_pixmap_with_value(ctx, pix, 0xff);
        dev = fz_new_draw_device(ctx, ctm, pix);
        fz_run_page(ctx, page->page, dev, fz_identity, nullptr);
        if (params->render_annots) {
          fz_run_page_annots(ctx, page->page, dev, fz_identity, nullptr);
        }
        if (params->render_widgets) {
          // abi 1.4 (story 7.4): widgets draw through their own MuPDF entry
          // point - the pre-flatten half of the bake-equality claim.
          fz_run_page_widgets(ctx, page->page, dev, fz_identity, nullptr);
        }
        fz_close_device(ctx, dev);
        return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
      },
      "render failed");
  if (r.code != PC_ERR_NONE) {
    if (dev)
      fz_drop_device(ctx, dev);
    if (pix)
      fz_drop_pixmap(ctx, pix);
    return r;
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

// R-M5 (R16.1): the mupdf backend declares PC_CAP_FACE_COVERAGE (R13.3) and PC_CAP_TEXT_LAYOUT
// (R13.4, abi 1.2); find_tables reports PC_ERR_CAPABILITY, never an empty list.
static int mupdf_doc_has_capability(void* backend_doc, uint32_t capability) __attribute__((used));
static int mupdf_doc_has_capability(void* backend_doc, uint32_t capability) {
  if (!backend_doc) {
    return 0;
  }
  return capability == PC_CAP_FACE_COVERAGE || capability == PC_CAP_TEXT_LAYOUT ||
                 capability == PC_CAP_FORMS
             ? 1
             : 0;
}

static pc_status mupdf_doc_find_tables(void* backend_doc, pc_rect* out_rects, size_t* out_count,
                                       size_t capacity) __attribute__((used));
static pc_status mupdf_doc_find_tables(void* backend_doc, pc_rect* out_rects, size_t* out_count,
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

// R13.3 - font fallback faces. MuPDF has no face-enumeration API: the inbuilt font table (source/
// fitz/font-table.h) is static, so the probe list is OUR curated table of bundled faces, probed
// via fz_lookup_builtin_font + fz_encode_character. Order is policy: the first face that draws a
// codepoint wins. The report (text-fallback-faces.txt) pins these indexes, so the table changes
// only with that golden.
struct MupdfFace {
  const char* name;
};

static const MupdfFace kCuratedFaces[] = {
    {"Helvetica"},         {"Times"},      {"Courier"},    {"Symbol"},
    {"ZapfDingbats"},      {"Charis SIL"}, {"Noto Serif"}, {"Noto Sans Math"},
    {"Noto Sans Symbols"}, {"Noto Emoji"},
};

static constexpr uint32_t kNcuratedFaces = sizeof(kCuratedFaces) / sizeof(kCuratedFaces[0]);
static_assert(kNcuratedFaces > 0, "face table must not be empty");

static uint32_t mupdf_face_count(void* backend_doc) __attribute__((used));
static uint32_t mupdf_face_count(void* backend_doc) {
  (void)backend_doc;
  return kNcuratedFaces;
}

// Restored from 05ce35c (the 7.1 rewrite of this file broke it two ways: a nullptr
// `len` that search_by_family dereferences on the first base-14 match, and a
// "font exists" answer that made every codepoint covered, killing the R13.3
// missing-glyph contract that test_text_fallback pins).
static pc_status mupdf_face_coverage(void* backend_doc, uint32_t face, uint32_t codepoint,
                                     int* out_has) __attribute__((used));
static pc_status mupdf_face_coverage(void* backend_doc, uint32_t face, uint32_t codepoint,
                                     int* out_has) {
  if (!out_has) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  if (face >= kNcuratedFaces) {
    return {sizeof(pc_status), PC_ERR_RANGE, 0, "face index out of range"};
  }
  MupdfDoc* doc = static_cast<MupdfDoc*>(backend_doc);

  int has = 0;
  fz_var(has);
  pc_status s = mupdf::run_guarded(
      doc->ctx,
      [&]() -> pc_status {
        int len = 0;
        const unsigned char* data =
            fz_lookup_builtin_font(doc->ctx, kCuratedFaces[face].name, 0, 0, &len);
        if (!data || len <= 0) {
          return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};  // face absent -> has stays 0
        }
        fz_font* font =
            fz_new_font_from_memory(doc->ctx, kCuratedFaces[face].name, data, len, 0, 1);
        has = fz_encode_character(doc->ctx, font, (int)codepoint) != 0;
        fz_drop_font(doc->ctx, font);
        return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
      },
      "face probe failed");
  if (s.code != PC_ERR_NONE) {
    return s;
  }
  *out_has = has;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

// R13.4 (abi 1.2, story 6.1) - one page's text as UTF-8 plus per-cluster quads. The engine's
// fz_stext walk is translated exactly here (exception_bridge owns fz_try, R-M2), and every
// value crossing out is copied into our malloc'd buffers - the caller frees through
// page_text_layout_free, never fz memory directly (R-M6).
static pc_status mupdf_page_text_layout(void* backend_page, char** out_utf8,
                                        pc_text_box** out_boxes, uint32_t* out_count)
    __attribute__((used));
static pc_status mupdf_page_text_layout(void* backend_page, char** out_utf8,
                                        pc_text_box** out_boxes, uint32_t* out_count) {
  if (!backend_page || !out_utf8 || !out_boxes || !out_count) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  *out_utf8 = nullptr;
  *out_boxes = nullptr;
  *out_count = 0;

  MupdfPage* page = static_cast<MupdfPage*>(backend_page);
  fz_context* ctx = page->ctx;

  fz_stext_page* stext = nullptr;
  pc_status s = mupdf::run_guarded(
      ctx,
      [&]() -> pc_status {
        stext = fz_new_stext_page_from_page(ctx, page->page, nullptr);
        if (!stext) {
          return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM extracting text"};
        }
        return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
      },
      "text extraction failed");
  if (s.code != PC_ERR_NONE) {
    return s;
  }

  uint32_t cluster_count = 0;
  uint32_t utf8_len = 0;
  for (fz_stext_block* block = stext->first_block; block; block = block->next) {
    if (block->type != FZ_STEXT_BLOCK_TEXT)
      continue;
    for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
      for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
        cluster_count++;
        utf8_len += (uint32_t)fz_runetochar(nullptr, ch->c);
      }
      utf8_len += 1;
    }
  }

  char* utf8 = (char*)std::malloc(utf8_len + 1);
  pc_text_box* boxes =
      cluster_count ? (pc_text_box*)std::malloc(cluster_count * sizeof(pc_text_box)) : nullptr;
  if (!utf8 || (cluster_count && !boxes)) {
    std::free(utf8);
    std::free(boxes);
    fz_drop_stext_page(ctx, stext);
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM copying text out"};
  }

  uint32_t box_i = 0;
  uint32_t byte_i = 0;
  for (fz_stext_block* block = stext->first_block; block; block = block->next) {
    if (block->type != FZ_STEXT_BLOCK_TEXT)
      continue;
    for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
      for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
        pc_text_box* box = &boxes[box_i++];
        box->quad.ul_x = ch->quad.ul.x;
        box->quad.ul_y = ch->quad.ul.y;
        box->quad.ur_x = ch->quad.ur.x;
        box->quad.ur_y = ch->quad.ur.y;
        box->quad.ll_x = ch->quad.ll.x;
        box->quad.ll_y = ch->quad.ll.y;
        box->quad.lr_x = ch->quad.lr.x;
        box->quad.lr_y = ch->quad.lr.y;
        box->byte_offset = byte_i;
        char mini[8];
        int len = fz_runetochar(mini, ch->c);
        std::memcpy(utf8 + byte_i, mini, (size_t)len);
        box->byte_len = (uint32_t)len;
        byte_i += (uint32_t)len;
      }
      utf8[byte_i] = '\n';
      byte_i += 1;
    }
  }
  utf8[utf8_len] = '\0';
  fz_drop_stext_page(ctx, stext);

  *out_utf8 = utf8;
  *out_boxes = boxes;
  *out_count = cluster_count;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

static void mupdf_page_text_layout_free(char* utf8, pc_text_box* boxes) __attribute__((used));
static void mupdf_page_text_layout_free(char* utf8, pc_text_box* boxes) {
  std::free(utf8);
  std::free(boxes);
}

// ================ Forms bridge (R46.1 R46.2 R46.3 / story 7.1, R48.x / story 7.2) ===
//
// This MuPDF version does not expose the fz_widget API, so enumeration walks the AcroForm
// dictionary tree directly (Root/AcroForm/Fields) through the pdf_* object API - the same
// walk MuPDF's own source/pdf/pdf-form.c performs. Every value crossing out is copied
// into our malloc'd buffers (R-M4/R-M6). Flat fields only: nodes that carry /Kids without
// /T are container levels of the field hierarchy; joining parent.child names arrives with
// radio groups (SPEC marks radio/combo out of scope for 7.2).

// Growable buffer helper. Length is derived from the string itself - an explicit length
// argument here previously disagreed with the literal (the FDF header was 15 bytes, the
// call said 13) and emitted a truncated header that passed a prefix-only test.
static inline bool append_str(char** buf, size_t* capacity, size_t* len, const char* s) {
  size_t slen = std::strlen(s);
  if (*len + slen + 1 > *capacity) {
    *capacity = (*capacity == 0) ? 256 : *capacity * 2;
    while (*len + slen + 1 > *capacity) *capacity *= 2;
    char* new_buf = (char*)std::realloc(*buf, *capacity);
    if (!new_buf)
      return false;
    *buf = new_buf;
  }
  std::memcpy(*buf + *len, s, slen);
  *len += slen;
  (*buf)[*len] = '\0';
  return true;
}

static inline size_t cstr_len(const char* s) {
  return s ? std::strlen(s) : 0;
}

static char* dup_cstr(const char* s) {
  size_t len = cstr_len(s);
  char* out = (char*)std::malloc(len + 1);
  if (out) {
    if (len) {
      std::memcpy(out, s, len);
    }
    out[len] = '\0';
  }
  return out;
}

// R46.2 (abi 1.3) - enumerate AcroForm fields via the dict walk. A document without an
// AcroForm is a valid empty list, not an error (R-M5: capability, not emptiness, is the
// contract here because the backend declares PC_CAP_FORMS).
static pc_status mupdf_form_list_fields(const pc_backend_api* api, void* backend_doc,
                                        pc_form_list* out_list) {
  (void)api;
  if (!backend_doc || !out_list) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  MupdfDoc* doc = static_cast<MupdfDoc*>(backend_doc);
  fz_context* ctx = doc->ctx;

  *out_list = {};
  return mupdf::run_guarded(
      ctx,
      [&]() -> pc_status {
        pdf_document* pdoc = pdf_specifics(ctx, doc->doc);
        if (!pdoc) {
          return {sizeof(pc_status), PC_ERR_CAPABILITY, 0, "not a PDF document"};
        }
        pdf_obj* fields = pdf_dict_getp(ctx, pdf_trailer(ctx, pdoc), "Root/AcroForm/Fields");
        int n = fields ? pdf_array_len(ctx, fields) : 0;
        if (n < 0) {
          n = 0;
        }
        pc_form_field* items =
            n > 0 ? (pc_form_field*)std::calloc((size_t)n, sizeof(pc_form_field)) : nullptr;
        uint32_t count = 0;
        bool oom = false;
        for (int i = 0; i < n; ++i) {
          pdf_obj* item = pdf_array_get(ctx, fields, i);
          if (!item) {
            continue;
          }
          pdf_obj* t = pdf_dict_get(ctx, item, PDF_NAME(T));
          const char* name = t ? pdf_to_str_buf(ctx, t) : nullptr;
          if (!name || !name[0]) {
            continue;  // container node of the field hierarchy; flat names only in 7.2
          }
          pdf_obj* ft = pdf_dict_get_inheritable(ctx, item, PDF_NAME(FT));
          const char* ft_name = ft ? pdf_to_name(ctx, ft) : nullptr;
          pc_form_field_type type;
          if (ft_name && std::strcmp(ft_name, "Tx") == 0) {
            type = PC_FORM_FIELD_TEXT;
          } else if (ft_name && std::strcmp(ft_name, "Btn") == 0) {
            type = PC_FORM_FIELD_CHECKBOX;
          } else if (ft_name && std::strcmp(ft_name, "Ch") == 0) {
            type = PC_FORM_FIELD_COMBO;
          } else {
            continue;  // Sig and unknown types are not modelled until Epic 8
          }
          pc_form_field* f = &items[count];
          f->type = type;
          f->name = dup_cstr(name);
          if (!f->name) {
            oom = true;
            break;
          }
          // /V is a string for text fields and a name (Off/Yes) for checkboxes.
          pdf_obj* v = pdf_dict_get_inheritable(ctx, item, PDF_NAME(V));
          if (v && pdf_is_name(ctx, v)) {
            f->value = dup_cstr(pdf_to_name(ctx, v));
          } else if (v && pdf_is_string(ctx, v)) {
            f->value = dup_cstr(pdf_to_str_buf(ctx, v));
          } else {
            f->value = dup_cstr("");
          }
          if (!f->value) {
            oom = true;
            break;
          }
          f->flags = (uint32_t)pdf_dict_get_inheritable_int(ctx, item, PDF_NAME(Ff));
          f->max_len = (uint32_t)pdf_dict_get_inheritable_int(ctx, item, PDF_NAME(MaxLen));
          pdf_obj* rect = pdf_dict_get(ctx, item, PDF_NAME(Rect));
          if (rect) {
            fz_rect r = pdf_to_rect(ctx, rect);
            f->rect = {r.x0, r.y0, r.x1, r.y1};
          }
          // page_index stays 0 until the widget->page walk lands with tab order (7.3);
          // fill and FDF export do not read it.
          f->page_index = 0;
          count++;
        }
        if (oom) {
          for (uint32_t j = 0; j < count; ++j) {
            std::free(items[j].name);
            std::free(items[j].value);
          }
          std::free(items);
          return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM copying fields out"};
        }
        if (count != (uint32_t)(n > 0 ? n : 0)) {
          // Some entries were skipped; shrink so the caller never sees uninitialized tail.
          pc_form_field* shrunk = nullptr;
          if (count > 0) {
            shrunk = (pc_form_field*)std::realloc(items, (size_t)count * sizeof(pc_form_field));
            if (!shrunk) {
              for (uint32_t j = 0; j < count; ++j) {
                std::free(items[j].name);
                std::free(items[j].value);
              }
              std::free(items);
              return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM shrinking field list"};
            }
          } else {
            std::free(items);
          }
          items = shrunk;
        }
        out_list->items = items;
        out_list->count = count;
        return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
      },
      "form field enumeration failed");
}

static void mupdf_form_list_free(pc_form_list* list) {
  if (!list || !list->items)
    return;
  for (uint32_t i = 0; i < list->count; ++i) {
    std::free(list->items[i].name);
    std::free(list->items[i].value);
    std::free(list->items[i].default_value);
    for (uint32_t j = 0; j < list->items[i].options_count; ++j) {
      std::free(list->items[i].options[j]);
    }
    std::free(list->items[i].options);
    std::free(list->items[i].format);
  }
  std::free(list->items);
  list->items = nullptr;
  list->count = 0;
}

// FDF Export - R46.1 R46.2 R46.3 (story 7.1), emits the enumerated fields (story 7.2).
// Byte-identical to pc_form_fdf_export_ir for the same field state, so a CLI fill and a
// backend export of the same values produce the same artefact.
static pc_status mupdf_fdf_export(const pc_backend_api* api, void* backend_doc, pc_fdf* out_fdf) {
  (void)api;
  if (!backend_doc || !out_fdf) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }

  pc_form_list list = {};
  pc_status s = mupdf_form_list_fields(api, backend_doc, &list);
  if (s.code != PC_ERR_NONE) {
    return s;
  }

  char* buf = nullptr;
  size_t capacity = 0;
  size_t len = 0;
  bool ok = append_str(&buf, &capacity, &len, "%FDF-1.2\n%\xe2\xe3\xcf\xd3\n") &&
            append_str(&buf, &capacity, &len, "1 0 obj\n<<\n/FDF\n<<\n/Fields [\n");
  for (uint32_t i = 0; ok && i < list.count; ++i) {
    const char* name = list.items[i].name ? list.items[i].name : "";
    const char* value = list.items[i].value ? list.items[i].value : "";
    ok = append_str(&buf, &capacity, &len, "<<\n/T (") && append_str(&buf, &capacity, &len, name) &&
         append_str(&buf, &capacity, &len, ")\n/V (") && append_str(&buf, &capacity, &len, value) &&
         append_str(&buf, &capacity, &len, ")\n>>\n");
  }
  if (ok) {
    ok = append_str(&buf, &capacity, &len,
                    "]\n>>\n>>\nendobj\ntrailer\n<<\n/Root 1 0 R\n>>\n%%EOF\n");
  }
  mupdf_form_list_free(&list);
  if (!ok) {
    std::free(buf);
    return {sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM building FDF"};
  }
  out_fdf->data = buf;
  out_fdf->size = len;
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}

static pc_status mupdf_fdf_import(const pc_backend_api* api, void* backend_doc,
                                  const char* fdf_data, size_t fdf_size) {
  (void)api;
  (void)backend_doc;
  (void)fdf_data;
  (void)fdf_size;
  // Deferred, tracked in #73 (7.1 partial): writing /V back into the AcroForm tree
  // needs an incremental-save story first. Reports UNSUPPORTED, never a silent no-op.
  return {sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "FDF import not implemented"};
}

static void mupdf_form_fdf_free(pc_fdf* fdf) {
  if (fdf && fdf->data) {
    std::free(fdf->data);
    fdf->data = nullptr;
    fdf->size = 0;
  }
}

// R53.1 (abi 1.4, story 7.4) - bake widgets into static page content through
// pdf_bake_document (the same path mutool's own bake uses; its contract is the visual
// identity the render-hash test relies on), remove the interactive form objects, and
// save a NEW file. The source document is never modified in place.
static pc_status mupdf_form_flatten(const pc_backend_api* api, void* backend_doc,
                                    const char* out_path) {
  (void)api;
  if (!backend_doc || !out_path) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }
  MupdfDoc* doc = static_cast<MupdfDoc*>(backend_doc);
  return mupdf::run_guarded(
      doc->ctx,
      [&]() -> pc_status {
        pdf_document* pdoc = pdf_specifics(doc->ctx, doc->doc);
        if (!pdoc) {
          return {sizeof(pc_status), PC_ERR_CAPABILITY, 0, "not a PDF document"};
        }
        // bake_annots=0: annotations stay interactive; only form widgets bake.
        pdf_bake_document(doc->ctx, pdoc, 0, 1);
        pdf_write_options opts = {};
        opts.do_incremental = 0;
        opts.do_garbage = 0;
        opts.do_clean = 0;
        pdf_save_document(doc->ctx, pdoc, out_path, &opts);
        return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
      },
      "flatten failed");
}

pc_backend_api pc_mupdf_backend_api = {
    .abi_major = PC_BACKEND_API_VERSION_MAJOR,
    .abi_minor = PC_BACKEND_API_VERSION_MINOR,
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
    .doc_has_capability = mupdf_doc_has_capability,
    .doc_find_tables = mupdf_doc_find_tables,
    .face_count = mupdf_face_count,
    .face_coverage = mupdf_face_coverage,
    .page_text_layout = mupdf_page_text_layout,
    .page_text_layout_free = mupdf_page_text_layout_free,
    .form_list_fields = mupdf_form_list_fields,
    .form_list_free = mupdf_form_list_free,
    .form_fdf_export = mupdf_fdf_export,
    .form_fdf_import = mupdf_fdf_import,
    .form_fdf_free = mupdf_form_fdf_free,
    .form_flatten = mupdf_form_flatten,
};

const pc_backend_api* pc_mupdf_backend_get_api(void) {
  return &pc_mupdf_backend_api;
}