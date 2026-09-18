#include "png_writer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void init_crc32_table(void) {
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t c = i;
    for (int j = 0; j < 8; ++j) {
      c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    }
    crc32_table[i] = c;
  }
  crc32_table_init = true;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
  if (!crc32_table_init)
    init_crc32_table();
  crc = ~crc;
  while (len--) {
    crc = crc32_table[(crc ^ *data++) & 0xFF] ^ (crc >> 8);
  }
  return ~crc;
}

static uint32_t adler32_update(uint32_t adler, const uint8_t* data, size_t len) {
  uint32_t s1 = adler & 0xFFFF;
  uint32_t s2 = (adler >> 16) & 0xFFFF;
  while (len--) {
    s1 = (s1 + *data++) % 65521;
    s2 = (s2 + s1) % 65521;
  }
  return (s2 << 16) | s1;
}

static bool write_chunk(FILE* f, const char* type, const uint8_t* data, size_t len) {
  uint8_t len_be[4];
  len_be[0] = (len >> 24) & 0xFF;
  len_be[1] = (len >> 16) & 0xFF;
  len_be[2] = (len >> 8) & 0xFF;
  len_be[3] = len & 0xFF;
  if (fwrite(len_be, 1, 4, f) != 4)
    return false;
  if (fwrite(type, 1, 4, f) != 4)
    return false;
  if (len && fwrite(data, 1, len, f) != len)
    return false;
  uint32_t crc = crc32_update(0, reinterpret_cast<const uint8_t*>(type), 4);
  crc = crc32_update(crc, data, len);
  uint8_t crc_be[4];
  crc_be[0] = (crc >> 24) & 0xFF;
  crc_be[1] = (crc >> 16) & 0xFF;
  crc_be[2] = (crc >> 8) & 0xFF;
  crc_be[3] = crc & 0xFF;
  if (fwrite(crc_be, 1, 4, f) != 4)
    return false;
  return true;
}

static bool write_ihdr(FILE* f, uint32_t width, uint32_t height) {
  uint8_t data[13];
  data[0] = (width >> 24) & 0xFF;
  data[1] = (width >> 16) & 0xFF;
  data[2] = (width >> 8) & 0xFF;
  data[3] = width & 0xFF;
  data[4] = (height >> 24) & 0xFF;
  data[5] = (height >> 16) & 0xFF;
  data[6] = (height >> 8) & 0xFF;
  data[7] = height & 0xFF;
  data[8] = 8;
  data[9] = 6;
  data[10] = 0;
  data[11] = 0;
  data[12] = 0;
  return write_chunk(f, "IHDR", data, 13);
}

static bool write_idat(FILE* f, const uint8_t* rows, uint32_t width, uint32_t height,
                       uint32_t stride) {
  const size_t row_bytes = width * 4;
  const size_t row_size = row_bytes + 1;
  const size_t max_block = 65535;
  const size_t total_uncompressed = row_size * height;

  uint32_t adler = 1;

  uint8_t zlib_header[2] = {0x78, 0x01};
  if (fwrite(zlib_header, 1, 2, f) != 2)
    return false;
  adler = adler32_update(adler, zlib_header, 2);

  size_t remaining = total_uncompressed;
  size_t pos_in_row = 0;
  size_t row = 0;

  while (remaining > 0) {
    size_t block_len = std::min(remaining, max_block);
    bool is_last = (block_len == remaining);
    (void)is_last;

    uint8_t block_header[5];
    block_header[0] = (block_len == remaining) ? 0x01 : 0x00;
    block_header[1] = block_len & 0xFF;
    block_header[2] = (block_len >> 8) & 0xFF;
    block_header[3] = ~block_header[1] & 0xFF;
    block_header[4] = ~block_header[2] & 0xFF;

    if (fwrite(block_header, 1, 5, f) != 5)
      return false;
    adler = adler32_update(adler, block_header, 5);

    size_t block_pos = 0;
    while (block_pos < block_len) {
      uint8_t byte;
      if (pos_in_row == 0) {
        byte = 0;
      } else {
        size_t pixel_index = pos_in_row - 1;
        if (pixel_index >= row_bytes) {
          return false;
        }
        byte = rows[row * stride + pixel_index];
      }

      if (fwrite(&byte, 1, 1, f) != 1)
        return false;
      adler = adler32_update(adler, &byte, 1);

      pos_in_row++;
      block_pos++;
      remaining--;

      if (pos_in_row >= row_size) {
        pos_in_row = 0;
        row++;
        if (row >= height && remaining > 0)
          return false;
      }
    }
  }

  uint8_t adler_be[4];
  adler_be[0] = (adler >> 24) & 0xFF;
  adler_be[1] = (adler >> 16) & 0xFF;
  adler_be[2] = (adler >> 8) & 0xFF;
  adler_be[3] = adler & 0xFF;
  if (fwrite(adler_be, 1, 4, f) != 4)
    return false;

  return true;
}

static bool write_iend(FILE* f) {
  uint8_t len_be[4] = {0, 0, 0, 0};
  if (fwrite(len_be, 1, 4, f) != 4)
    return false;
  if (fwrite("IEND", 1, 4, f) != 4)
    return false;
  uint32_t crc = crc32_update(0, reinterpret_cast<const uint8_t*>("IEND"), 4);
  uint8_t crc_be[4];
  crc_be[0] = (crc >> 24) & 0xFF;
  crc_be[1] = (crc >> 16) & 0xFF;
  crc_be[2] = (crc >> 8) & 0xFF;
  crc_be[3] = crc & 0xFF;
  if (fwrite(crc_be, 1, 4, f) != 4)
    return false;
  return true;
}

bool pc_png_write(const char* path, const pc_pixmap* pixmap) {
  if (!path || !pixmap || !pixmap->data || pixmap->width == 0 || pixmap->height == 0) {
    return false;
  }

  FILE* f = fopen(path, "wb");
  if (!f)
    return false;

  static const uint8_t png_sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  if (fwrite(png_sig, 1, 8, f) != 8) {
    fclose(f);
    return false;
  }

  if (!write_ihdr(f, pixmap->width, pixmap->height)) {
    fclose(f);
    return false;
  }

  if (!write_idat(f, pixmap->data, pixmap->width, pixmap->height, pixmap->stride)) {
    fclose(f);
    return false;
  }

  if (!write_iend(f)) {
    fclose(f);
    return false;
  }

  fclose(f);
  return true;
}