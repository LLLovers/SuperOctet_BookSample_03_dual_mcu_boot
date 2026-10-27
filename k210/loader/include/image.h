#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum LoaderResult { LoaderSuccess = 0, LoaderSizeError = 1, LoaderCRCError = 2 };

enum { LoaderImageAddress = 0x80000000u, LoaderMaximumImageBytes = 0x5fc000u };

uint32_t loader_crc32(const uint8_t *data, uint32_t size);
enum LoaderResult loader_validate(const uint8_t *image, uint32_t size, uint32_t expectedCRC);

#ifdef __cplusplus
}
#endif
