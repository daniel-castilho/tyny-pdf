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

typedef struct pc_point {
  double x, y;
} pc_point;

/// Four-corner region in user-space points, corner order matching the engine's
/// quads (story 6.1, R32.1): ul/ur is the run's ascent line, ll/lr the descent.
/// Unlike pc_rect the edges may be non-axis-aligned (rotated or skewed text).
typedef struct pc_quad {
  double ul_x, ul_y;
  double ur_x, ur_y;
  double ll_x, ll_y;
  double lr_x, lr_y;
} pc_quad;

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