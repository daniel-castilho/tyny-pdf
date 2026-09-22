#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pdfcore.h"
#include "pdfcore/transaction.h"

int replay_command(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s txn replay <log.json> --out <out.json>\n", argv[0]);
    return 2;
  }

  if (strcmp(argv[2], "replay") != 0) {
    fprintf(stderr, "Unknown txn subcommand: %s\n", argv[2]);
    return 2;
  }

  if (argc < 5) {
    fprintf(stderr, "Usage: %s txn replay <log.json> --out <out.json>\n", argv[0]);
    return 2;
  }

  const char* log_path = argv[3];
  const char* out_path = nullptr;

  for (int i = 4; i < argc; ++i) {
    if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
      out_path = argv[++i];
    } else {
      fprintf(stderr, "Unknown argument: %s\n", argv[i]);
      return 2;
    }
  }

  if (!out_path) {
    fprintf(stderr, "Missing --out\n");
    return 2;
  }

  FILE* f = fopen(log_path, "rb");
  if (!f) {
    fprintf(stderr, "Failed to open log file\n");
    return 1;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return 1;
  }
  long size = ftell(f);
  if (size < 0 || size > 10 * 1024 * 1024) {
    fclose(f);
    fprintf(stderr, "Log file size invalid\n");
    return 1;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return 1;
  }
  char* json = static_cast<char*>(std::malloc(static_cast<size_t>(size) + 1));
  if (!json) {
    fclose(f);
    return 1;
  }
  size_t read = fread(json, 1, static_cast<size_t>(size), f);
  fclose(f);
  if (read != static_cast<size_t>(size)) {
    std::free(json);
    return 1;
  }
  json[size] = '\0';

  pc_doc* doc = nullptr;
  pc_status s = pc_doc_open("replay.pdf", nullptr, &doc);
  if (s.code != PC_ERR_NONE) {
    std::free(json);
    return 1;
  }

  pc_txn* txn = nullptr;
  s = pc_txn_from_json(json, doc, &txn);
  std::free(json);
  if (s.code != PC_ERR_NONE) {
    pc_doc_close(doc);
    return 1;
  }

  char* out_json = nullptr;
  s = pc_txn_to_json(txn, &out_json);
  pc_txn_free(txn);
  pc_doc_close(doc);
  if (s.code != PC_ERR_NONE) {
    return 1;
  }

  FILE* out = fopen(out_path, "wb");
  if (!out) {
    std::free(out_json);
    fprintf(stderr, "Failed to open output\n");
    return 1;
  }
  size_t len = std::strlen(out_json);
  size_t written = fwrite(out_json, 1, len, out);
  fclose(out);
  std::free(out_json);
  if (written != len) {
    return 1;
  }
  return 0;
}
