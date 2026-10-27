#pragma once

#include "bytes.hpp"

namespace so {
// previous is the finalized CRC of preceding bytes; zero starts a new stream.
uint32_t CRC32(Bytes bytes, uint32_t previous = 0);
}
