#pragma once

#include "image.h"

// SPI header words are size and file CRC32; DMA receives the following image words.
struct LoaderImageHeader {
  uint32_t bytes;
  uint32_t crc;
};

void loader_initialize(void);
enum LoaderResult loader_receive_header(struct LoaderImageHeader *header);
// Requires a validated size; returns after DMA completion and CS deassertion.
void loader_receive_image(uint32_t bytes);
// Disable SPI, send one UART result byte, and wait for it to leave the UART.
void loader_report_result(enum LoaderResult result);
__attribute__((noreturn)) void loader_start_image(void);
