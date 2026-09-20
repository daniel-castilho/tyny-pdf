#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "pdfcore/geom.h"
#include "pdfcore/page.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(a, b)                                                                            \
  do {                                                                                             \
    if ((a) != (b)) {                                                                              \
      fprintf(stderr, "FAIL: %s:%d: %s == %s (%d != %d)\n", __FILE__, __LINE__, #a, #b, (a), (b)); \
      tests_failed++;                                                                              \
    } else {                                                                                       \
      tests_passed++;                                                                              \
    }                                                                                              \
  } while (0)

#define ASSERT_NEAR(a, b, eps)                                                                     \
  do {                                                                                             \
    if (fabs((a) - (b)) > (eps)) {                                                                 \
      fprintf(stderr, "FAIL: %s:%d: %s == %s (|%f - %f| > %f)\n", __FILE__, __LINE__, #a, #b, (a), \
              (b), (eps));                                                                         \
      tests_failed++;                                                                              \
    } else {                                                                                       \
      tests_passed++;                                                                              \
    }                                                                                              \
  } while (0)

// R8.1: pc_rect_to_device converts user-space to device-space for rotation 0
static void test_rect_to_device_rotation0(void) {
  pc_page_box box = {};
  box.mediabox = {0, 0, 612, 792};
  box.cropbox = {100, 100, 500, 700};
  box.rotation = 0;

  // us in MediaBox coordinates, dev in CropBox-relative coordinates
  pc_rect us = {100, 100, 200, 200};  // At CropBox origin, 100x100 size
  pc_rect dev = {};
  pc_status s = pc_rect_to_device(&box, us, &dev);
  ASSERT_EQ(s.code, PC_ERR_NONE);

  // us (100,100) is CropBox origin -> dev (0,0)
  // us (200,200) is 100 points into CropBox -> dev (100,100) at 72 DPI
  ASSERT_NEAR(dev.x0, 0.0, 1e-9);
  ASSERT_NEAR(dev.y0, 0.0, 1e-9);
  ASSERT_NEAR(dev.x1, 100.0, 1e-9);
  ASSERT_NEAR(dev.y1, 100.0, 1e-9);
}

// R8.1: rotation 90
static void test_rect_to_device_rotation90(void) {
  pc_page_box box = {};
  box.mediabox = {0, 0, 612, 792};
  box.cropbox = {100, 100, 500, 700};
  box.rotation = 90;

  // Full MediaBox maps to rotated CropBox bounding box
  pc_rect us = {0, 0, 612, 792};
  pc_rect dev = {};
  pc_status s = pc_rect_to_device(&box, us, &dev);
  ASSERT_EQ(s.code, PC_ERR_NONE);

  // Rotated CropBox: width=600 (ch), height=400 (cw)
  // Origin at CropBox bottom-left (100,100)
  ASSERT_NEAR(dev.x0, 100.0, 1e-9);
  ASSERT_NEAR(dev.y0, 100.0, 1e-9);
  ASSERT_NEAR(dev.x1, 700.0, 1e-9);
  ASSERT_NEAR(dev.y1, 500.0, 1e-9);
}

// R8.1: rotation 180
static void test_rect_to_device_rotation180(void) {
  pc_page_box box = {};
  box.mediabox = {0, 0, 612, 792};
  box.cropbox = {100, 100, 500, 700};
  box.rotation = 180;

  pc_rect us = {0, 0, 612, 792};
  pc_rect dev = {};
  pc_status s = pc_rect_to_device(&box, us, &dev);
  ASSERT_EQ(s.code, PC_ERR_NONE);

  // Full MediaBox maps to CropBox (flipped)
  ASSERT_NEAR(dev.x0, 100.0, 1e-9);
  ASSERT_NEAR(dev.y0, 100.0, 1e-9);
  ASSERT_NEAR(dev.x1, 500.0, 1e-9);
  ASSERT_NEAR(dev.y1, 700.0, 1e-9);
}

// R8.1: rotation 270
static void test_rect_to_device_rotation270(void) {
  pc_page_box box = {};
  box.mediabox = {0, 0, 612, 792};
  box.cropbox = {100, 100, 500, 700};
  box.rotation = 270;

  pc_rect us = {0, 0, 612, 792};
  pc_rect dev = {};
  pc_status s = pc_rect_to_device(&box, us, &dev);
  ASSERT_EQ(s.code, PC_ERR_NONE);

  // Rotated CropBox: width=600 (ch), height=400 (cw)
  ASSERT_NEAR(dev.x0, 100.0, 1e-9);
  ASSERT_NEAR(dev.y0, 100.0, 1e-9);
  ASSERT_NEAR(dev.x1, 700.0, 1e-9);
  ASSERT_NEAR(dev.y1, 500.0, 1e-9);
}

// R8.2: CropBox != MediaBox at rotation 0
static void test_cropbox_not_equal_mediabox(void) {
  pc_page_box box = {};
  box.mediabox = {0, 0, 612, 792};
  box.cropbox = {50, 50, 550, 750};
  box.rotation = 0;

  pc_rect us = {0, 0, 612, 792};
  pc_rect dev = {};
  pc_status s = pc_rect_to_device(&box, us, &dev);
  ASSERT_EQ(s.code, PC_ERR_NONE);

  // Full MediaBox in CropBox-relative coordinates
  // dev = us - CropBox_origin = {-50, -50, 562, 742}
  ASSERT_NEAR(dev.x0, -50.0, 1e-9);
  ASSERT_NEAR(dev.y0, -50.0, 1e-9);
  ASSERT_NEAR(dev.x1, 562.0, 1e-9);
  ASSERT_NEAR(dev.y1, 742.0, 1e-9);
}

// R8.3: invalid rotation
static void test_invalid_rotation(void) {
  pc_page_box box = {};
  box.mediabox = {0, 0, 612, 792};
  box.cropbox = {0, 0, 612, 792};
  box.rotation = 45;

  pc_rect us = {0, 0, 100, 100};
  pc_rect dev = {};
  pc_status s = pc_rect_to_device(&box, us, &dev);
  ASSERT_EQ(s.code, PC_ERR_ARGUMENT);
}

// R8.3: null argument validation
static void test_null_arguments(void) {
  pc_page_box box = {};
  box.mediabox = {0, 0, 612, 792};
  box.cropbox = {0, 0, 612, 792};
  box.rotation = 0;

  pc_rect us = {0, 0, 100, 100};
  pc_rect dev = {};
  pc_status s = pc_rect_to_device(nullptr, us, &dev);
  ASSERT_EQ(s.code, PC_ERR_ARGUMENT);

  s = pc_rect_to_device(&box, us, nullptr);
  ASSERT_EQ(s.code, PC_ERR_ARGUMENT);
}

int main(void) {
  test_rect_to_device_rotation0();
  test_rect_to_device_rotation90();
  test_rect_to_device_rotation180();
  test_rect_to_device_rotation270();
  test_cropbox_not_equal_mediabox();
  test_invalid_rotation();
  test_null_arguments();

  printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}