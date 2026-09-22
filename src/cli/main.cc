#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "pdfcore.h"
#include "png_writer.h"

// Portable exit code extraction: POSIX WEXITSTATUS on Unix, raw status on Windows.
#if defined(_WIN32)
static inline int get_exit_code(int status) {
  return status;
}
#else
static inline int get_exit_code(int status) {
  return WEXITSTATUS(status);
}
#endif

static void print_usage(const char* prog) {
  fprintf(stderr, "Usage: %s render <in.pdf> --page N [--dpi D] [--out F] [--backend null|mupdf]\n",
          prog);
  fprintf(stderr, "       %s sidecar gc <sidecar.json> [--days N]\n", prog);
  fprintf(stderr, "       %s txn replay <log.json> --out <out.json>\n", prog);
}

int replay_command(int argc, char** argv);

int main(int argc, char** argv) {
  if (argc < 3) {
    print_usage(argv[0]);
    return 2;
  }

  const char* cmd = argv[1];
  const char* arg2 = argv[2];

  if (strcmp(cmd, "render") == 0) {
    // render command logic
    const char* in_path = arg2;

    int page = -1;
    int dpi = 72;
    const char* out_path = nullptr;
    const char* backend_name = "null";

    for (int i = 3; i < argc; ++i) {
      if (strcmp(argv[i], "--page") == 0 && i + 1 < argc) {
        char* endptr = nullptr;
        long val = strtol(argv[++i], &endptr, 10);
        if (*endptr != '\0' || val < 0) {
          fprintf(stderr, "Invalid --page value\n");
          return 2;
        }
        page = (int)val;
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
#ifdef PC_HAVE_MUPDF
      backend_api = pc_mupdf_backend_get_api();
#else
      fprintf(stderr, "mupdf backend not built in this configuration\n");
      return 3;
#endif
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
      return 2;
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
      if (!pc_png_write(out_path, &pixmap)) {
        fprintf(stderr, "Failed to write PNG: %s\n", out_path);
        backend_api->pixmap_free(&pixmap);
        backend_api->page_free(page_ptr);
        backend_api->doc_close(backend_doc);
        return 1;
      }
    } else {
      printf("Rendered page %d at %d DPI (%ux%u)\n", page, dpi, pixmap.width, pixmap.height);
    }

    backend_api->pixmap_free(&pixmap);
    backend_api->page_free(page_ptr);
    backend_api->doc_close(backend_doc);
    return 0;
  }

  if (strcmp(cmd, "sidecar") == 0) {
    if (strcmp(arg2, "gc") != 0) {
      print_usage(argv[0]);
      return 2;
    }
    if (argc < 4) {
      fprintf(stderr, "Usage: %s sidecar gc <sidecar.json> [--days N]\n", argv[0]);
      return 2;
    }
    const char* sidecar_path = argv[3];
    int days = 30;
    for (int i = 4; i < argc; ++i) {
      if (strcmp(argv[i], "--days") == 0 && i + 1 < argc) {
        days = atoi(argv[++i]);
        if (days < 0) {
          fprintf(stderr, "Invalid --days value\n");
          return 2;
        }
      } else {
        fprintf(stderr, "Unknown argument: %s\n", argv[i]);
        return 2;
      }
    }
    // Invoke sidecar-fmt.py gc
    char cmd[1024];
    int n = snprintf(cmd, sizeof(cmd), "python3 tools/sidecar-fmt.py gc \"%s\" --days %d",
                     sidecar_path, days);
    if (n < 0 || n >= (int)sizeof(cmd)) {
      fprintf(stderr, "Command too long\n");
      return 1;
    }
    int rc = system(cmd);
    if (rc != 0) {
      return get_exit_code(rc);
    }
    return 0;
  }

  if (strcmp(cmd, "txn") == 0) {
    return replay_command(argc, argv);
  }

  print_usage(argv[0]);
  return 2;
}