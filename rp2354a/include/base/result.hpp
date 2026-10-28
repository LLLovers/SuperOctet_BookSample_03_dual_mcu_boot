#pragma once

#include <cstdint>

namespace so {
enum class Result : uint8_t {
  Ok,
  Busy,
  InvalidLength,
  OutOfRange,
  Unsupported,
  InvalidState,
  CRCMismatch,
  Unavailable,
  ResourceLimit,
  Timeout
};
}
