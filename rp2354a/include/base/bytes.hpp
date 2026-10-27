#pragma once

#include <cstddef>
#include <cstdint>

namespace so {
struct Bytes {
  const uint8_t *data = nullptr;
  size_t size = 0;
};

struct Buffer {
  uint8_t *data = nullptr;
  size_t size = 0;
};

}
