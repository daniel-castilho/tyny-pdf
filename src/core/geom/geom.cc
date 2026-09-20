#include "pdfcore/geom.h"
#include "pdfcore/page.h"

#include <algorithm>
#include <cmath>

static void rect_normalize(pc_rect* r) {
  if (r->x0 > r->x1) std::swap(r->x0, r->x1);
  if (r->y0 > r->y1) std::swap(r->y0, r->y1);
}

pc_status pc_rect_to_device(const pc_page_box* box, pc_rect us, pc_rect* dev) {
  if (!box || !dev) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "null argument"};
  }

  pc_rect m = box->mediabox;
  pc_rect c = box->cropbox;
  rect_normalize(&m);
  rect_normalize(&c);

  double mw = m.x1 - m.x0;
  double mh = m.y1 - m.y0;
  double cw = c.x1 - c.x0;
  double ch = c.y1 - c.y0;

  if (mw <= 0 || mh <= 0 || cw <= 0 || ch <= 0) {
    return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "invalid box dimensions"};
  }

  switch (box->rotation) {
    case 0:
      // Device space = CropBox-relative user space at 72 DPI
      dev->x0 = us.x0 - c.x0;
      dev->y0 = us.y0 - c.y0;
      dev->x1 = us.x1 - c.x0;
      dev->y1 = us.y1 - c.y0;
      break;
    case 90:
      // Rotate 90 CW: MediaBox -> rotated CropBox at CropBox origin
      // dev.x = c.x0 + us.y * (ch / mh)
      // dev.y = c.y0 + cw - us.x * (cw / mw)
      dev->x0 = c.x0 + us.y0 * ch / mh;
      dev->y0 = c.y0 + cw - us.x1 * cw / mw;
      dev->x1 = c.x0 + us.y1 * ch / mh;
      dev->y1 = c.y0 + cw - us.x0 * cw / mw;
      break;
    case 180:
      // Rotate 180: MediaBox -> CropBox at CropBox origin (flipped)
      // dev.x = c.x0 + cw - us.x * (cw / mw)
      // dev.y = c.y0 + ch - us.y * (ch / mh)
      dev->x0 = c.x0 + cw - us.x1 * cw / mw;
      dev->y0 = c.y0 + ch - us.y1 * ch / mh;
      dev->x1 = c.x0 + cw - us.x0 * cw / mw;
      dev->y1 = c.y0 + ch - us.y0 * ch / mh;
      break;
    case 270:
      // Rotate 270 CW (90 CCW): MediaBox -> rotated CropBox at CropBox origin
      // dev.x = c.x0 + ch - us.y * (ch / mh)
      // dev.y = c.y0 + us.x * (cw / mw)
      dev->x0 = c.x0 + ch - us.y1 * ch / mh;
      dev->y0 = c.y0 + us.x0 * cw / mw;
      dev->x1 = c.x0 + ch - us.y0 * ch / mh;
      dev->y1 = c.y0 + us.x1 * cw / mw;
      break;
    default:
      return {sizeof(pc_status), PC_ERR_ARGUMENT, 0, "invalid rotation"};
  }

  rect_normalize(dev);
  return {sizeof(pc_status), PC_ERR_NONE, 0, nullptr};
}