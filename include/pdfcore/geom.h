#ifndef PDFCORE_GEOM_H
#define PDFCORE_GEOM_H

#include <stddef.h>
#include <stdint.h>

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_rect {
  double x0, y0, x1, y1;
} pc_rect;

typedef struct pc_page_box {
  pc_rect mediabox;
  pc_rect cropbox;
  int rotation;
} pc_page_box;

/// Convert a user-space rectangle to device space using MediaBox/CropBox/rotation (R8.1).
/// PC_ERR_NONE on success, PC_ERR_ARGUMENT for a null box or dev.
pc_status pc_rect_to_device(const pc_page_box* box, pc_rect us, pc_rect* dev);

#ifdef __cplusplus
}
#endif

#endif