#include "pdfcore/backend.h"
#include "pdfcore/status.h"
#include "pdfcore/page.h"
#include <mupdf/fitz.h>
#include <cstdlib>
#include <cstring>
#include <cstdint>

struct MupdfDoc {
    fz_context *ctx;
    fz_document *doc;
    fz_device *dev;
    fz_pixmap *pixmap;
};

struct MupdfPage {
    fz_context *ctx;
    fz_page *page;
    fz_rect mediabox;
};

static pc_status mupdf_doc_open(const char *path, const char *password, void **out_backend_doc) __attribute__((used));
static pc_status mupdf_doc_open(const char *path, const char *password, void **out_backend_doc) {
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (!ctx) {
        return { sizeof(pc_status), PC_ERR_MEMORY, 0, "fz_new_context failed" };
    }

    fz_register_document_handlers(ctx);
    fz_try(ctx) {
        fz_document *doc = fz_open_document(ctx, path);
if (password && *password) {
                if (!fz_authenticate_password(ctx, doc, password)) {
                    fz_drop_document(ctx, doc);
                    fz_drop_context(ctx);
                    return { sizeof(pc_status), PC_ERR_PASSWORD, 0, "Invalid password" };
                }
            }

        MupdfDoc *doc_wrapper = (MupdfDoc*)std::malloc(sizeof(MupdfDoc));
        if (!doc_wrapper) {
            fz_drop_document(ctx, doc);
            fz_drop_context(ctx);
            return { sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM" };
        }
        doc_wrapper->ctx = ctx;
        doc_wrapper->doc = doc;
        doc_wrapper->dev = nullptr;
        doc_wrapper->pixmap = nullptr;
        *out_backend_doc = doc_wrapper;
        return { sizeof(pc_status), PC_ERR_NONE, 0, nullptr };
    }
    fz_catch(ctx) {
        fz_drop_context(ctx);
        return { sizeof(pc_status), PC_ERR_CORRUPT, 0, fz_caught_message(ctx) };
    }
    return { sizeof(pc_status), PC_ERR_CORRUPT, 0, "unreachable" };
}

static void mupdf_doc_close(void *backend_doc) __attribute__((used));
static void mupdf_doc_close(void *backend_doc) {
    if (!backend_doc) return;
    MupdfDoc *doc = static_cast<MupdfDoc*>(backend_doc);
    if (doc->pixmap) fz_drop_pixmap(doc->ctx, doc->pixmap);
    if (doc->dev) fz_drop_device(doc->ctx, doc->dev);
    if (doc->doc) fz_drop_document(doc->ctx, doc->doc);
    if (doc->ctx) fz_drop_context(doc->ctx);
    std::free(doc);
}

static uint32_t mupdf_doc_page_count(void *backend_doc) __attribute__((used));
static uint32_t mupdf_doc_page_count(void *backend_doc) {
    MupdfDoc *doc = static_cast<MupdfDoc*>(backend_doc);
    return fz_count_pages(doc->ctx, doc->doc);
}

static pc_status mupdf_page_get(void *backend_doc, uint32_t index, void **out_backend_page) __attribute__((used));
static pc_status mupdf_page_get(void *backend_doc, uint32_t index, void **out_backend_page) {
    MupdfDoc *doc = static_cast<MupdfDoc*>(backend_doc);
    uint32_t count = fz_count_pages(doc->ctx, doc->doc);
    if (index >= count) {
        return { sizeof(pc_status), PC_ERR_RANGE, 0, "page index out of range" };
    }
    fz_page *page = fz_load_page(doc->ctx, doc->doc, index);
    if (!page) {
        return { sizeof(pc_status), PC_ERR_CORRUPT, 0, "fz_load_page failed" };
    }
    MupdfPage *page_wrapper = (MupdfPage*)std::malloc(sizeof(MupdfPage));
    if (!page_wrapper) {
        fz_drop_page(doc->ctx, page);
        return { sizeof(pc_status), PC_ERR_MEMORY, 0, "OOM" };
    }
    page_wrapper->ctx = doc->ctx;
    page_wrapper->page = page;
    page_wrapper->mediabox = fz_bound_page(doc->ctx, page);
    *out_backend_page = page_wrapper;
    return { sizeof(pc_status), PC_ERR_NONE, 0, nullptr };
}

static pc_status mupdf_page_render(void *backend_page, const pc_render_params *params, pc_pixmap *out_pixmap) {
    (void)backend_page;
    (void)params;
    (void)out_pixmap;
    return { sizeof(pc_status), PC_ERR_UNSUPPORTED, 0, "mupdf_page_render not implemented" };
}

static void mupdf_page_free(void *backend_page) __attribute__((used));
static void mupdf_page_free(void *backend_page) {
    MupdfPage *page = static_cast<MupdfPage*>(backend_page);
    if (page->page && page->ctx) fz_drop_page(page->ctx, page->page);
    std::free(page);
}

static void mupdf_pixmap_free(pc_pixmap *pixmap) __attribute__((used));
static void mupdf_pixmap_free(pc_pixmap *pixmap) {
    if (pixmap && pixmap->data) {
        std::free(pixmap->data);
        pixmap->data = nullptr;
    }
}

static const char* mupdf_get_last_error(void *backend_doc) __attribute__((used));
static const char* mupdf_get_last_error(void *backend_doc) {
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
    .page_render = mupdf_page_render,
    .page_free = mupdf_page_free,
    .pixmap_free = mupdf_pixmap_free,
    .get_last_error = mupdf_get_last_error,
};

const pc_backend_api* pc_mupdf_backend_get_api(void) {
    return &pc_mupdf_backend_api;
}