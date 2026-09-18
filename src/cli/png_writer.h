#ifndef TYNYPDF_CLI_PNG_WRITER_H
#define TYNYPDF_CLI_PNG_WRITER_H

#include <stdbool.h>
#include <stdint.h>

#include "pdfcore/page.h"

#ifdef __cplusplus
extern "C" {
#endif

bool pc_png_write(const char* path, const pc_pixmap* pixmap);

#ifdef __cplusplus
}
#endif

#endif