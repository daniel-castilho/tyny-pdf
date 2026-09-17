#ifndef PDFCORE_DOC_H
#define PDFCORE_DOC_H

#include <stddef.h>
#include <stdint.h>

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_doc pc_doc;

pc_status pc_doc_open(const char* path, const char* password, pc_doc** out_doc);

uint32_t pc_doc_page_count(const pc_doc* doc);

void pc_doc_close(pc_doc* doc);

#ifdef __cplusplus
}
#endif

#endif