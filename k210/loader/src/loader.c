#include "hardware.h"
#include "image.h"

__attribute__((noreturn)) static void Stop(enum LoaderResult result) {
  loader_report_result(result);
  for (;;) {
    __asm__ volatile("wfi");
  }
}

__attribute__((noreturn)) void loader_main(void) {
  loader_initialize();

  struct LoaderImageHeader header;
  const enum LoaderResult headerResult = loader_receive_header(&header);
  if (headerResult != LoaderSuccess)
    Stop(headerResult);

  loader_receive_image(header.bytes);
  const uint8_t *const image = (const uint8_t *)(uintptr_t)LoaderImageAddress;
  const enum LoaderResult result = loader_validate(image, header.bytes, header.crc);
  if (result != LoaderSuccess)
    Stop(result);

  loader_report_result(LoaderSuccess);
  loader_start_image();
}
