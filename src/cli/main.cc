#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore.h"

static void print_usage(const char* prog) {
  fprintf(stderr, "Usage: %s render <in.pdf> --page N [--dpi D] [--out F] [--backend null|mupdf]\n",
          prog);
  fprintf(stderr, "       %s info <in.pdf>\n", prog);
}

int main(int argc, char** argv) {
  if (argc < 3) {
    print_usage(argv[0]);
    return 2;
  }

  const char* cmd = argv[1];
  const char* in_path = argv[2];

  if (strcmp(cmd, "render") != 0) {
    print_usage(argv[0]);
    return 2;
  }

  int page = -1;
  int dpi = 72;
  const char* out_path = nullptr;
  const char* backend_name = "null";

  for (int i = 3; i < argc; ++i) {
    if (strcmp(argv[i], "--page") == 0 && i + 1 < argc) {
      page = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--dpi") == 0 && i + 1 < argc) {
      dpi = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
      out_path = argv[++i];
    } else if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
      backend_name = argv[++i];
    } else {
      print_usage(argv[0]);
      return 2;
    }
  }

  if (page < 0) {
    fprintf(stderr, "Missing --page\n");
    return 2;
  }

  const pc_backend_api* backend_api = nullptr;
  if (strcmp(backend_name, "null") == 0) {
    extern pc_backend_api pc_null_backend_api;
    backend_api = &pc_null_backend_api;
  } else if (strcmp(backend_name, "mupdf") == 0) {
    extern pc_backend_api pc_mupdf_backend_api;
    backend_api = &pc_mupdf_backend_api;
  } else {
    fprintf(stderr, "Unknown backend: %s\n", backend_name);
    return 2;
  }

  void* backend_doc = nullptr;
  pc_status s = backend_api->doc_open(in_path, nullptr, &backend_doc);
  if (s.code != PC_ERR_NONE) {
    fprintf(stderr, "Failed to open document: %u\n", s.code);
    return 1;
  }

  uint32_t count = backend_api->doc_page_count(backend_doc);
  if (page >= (int)count) {
    backend_api->doc_close(backend_doc);
    fprintf(stderr, "Page %d out of range (0-%u)\n", page, count - 1);
    return 1;
  }

  void* page_ptr = nullptr;
  pc_status s2 = backend_api->page_get(backend_doc, page, &page_ptr);
  if (s2.code != PC_ERR_NONE || !page_ptr) {
    backend_api->doc_close(backend_doc);
    return 1;
  }

  pc_render_params params = {};
  params.dpi = dpi;
  params.clip = {0, 0, 100, 100};
  params.render_annots = 0;
  params.render_text = 1;

  pc_pixmap pixmap = {};
  pc_status s3 = backend_api->page_render(page_ptr, &params, &pixmap);
  if (s3.code != PC_ERR_NONE || !pixmap.data) {
    backend_api->page_free(page_ptr);
    backend_api->doc_close(backend_doc);
    return 1;
  }

  if (out_path) {
    FILE* f = fopen(out_path, "wb");
    if (!f) {
      fprintf(stderr, "Failed to open output file: %s\n", out_path);
      backend_api->pixmap_free(&pixmap);
      backend_api->page_free(page_ptr);
      backend_api->doc_close(backend_doc);
      return 1;
    }
    // Write simple PPM
    fprintf(f, "P6\n%u %u\n255\n", pixmap.width, pixmap.height);
    fwrite(pixmap.data, 1, pixmap.height * pixmap.stride, f);
    fclose(f);
  } else {
    printf("Rendered page %d at %d DPI (%ux%u)\n", page, dpi, pixmap.width, pixmap.height);
  }

  backend_api->pixmap_free(&pixmap);
  backend_api->page_free(page_ptr);
  backend_api->doc_close(backend_doc);
  return 0;
}