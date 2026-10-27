#include "image.h"

uint32_t loader_crc32(const uint8_t *data, uint32_t size) {
  // CRC-32/ISO-HDLC, processed four bits at a time with a 64-byte table.
  static const uint32_t table[16] = {
      0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu, 0x76dc4190u, 0x6b6b51f4u,
      0x4db26158u, 0x5005713cu, 0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
      0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu,
  };
  uint32_t crc = 0xffffffffu;
  for (uint32_t i = 0; i < size; ++i) {
    crc ^= data[i];
    crc = (crc >> 4) ^ table[crc & 15u];
    crc = (crc >> 4) ^ table[crc & 15u];
  }
  return ~crc;
}

enum LoaderResult loader_validate(const uint8_t *image, uint32_t size, uint32_t expectedCRC) {
  if (size == 0 || size > LoaderMaximumImageBytes) {
    return LoaderSizeError;
  }
  return loader_crc32(image, size) == expectedCRC ? LoaderSuccess : LoaderCRCError;
}
