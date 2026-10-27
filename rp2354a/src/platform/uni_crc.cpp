#include "base/crc.hpp"

namespace so {
namespace {
uint32_t CRCByte(uint32_t crc, uint8_t byte) {
  crc ^= byte;
  for (unsigned bit = 0; bit < 8; ++bit) {
    crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
  }
  return crc;
}
}

uint32_t CRC32(Bytes bytes, uint32_t previous) {
  uint32_t crc = previous ^ 0xFFFFFFFFU;
  for (size_t i = 0; i < bytes.size; ++i) {
    crc = CRCByte(crc, bytes.data[i]);
  }
  return crc ^ 0xFFFFFFFFU;
}

}
