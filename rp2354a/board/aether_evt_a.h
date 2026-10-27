/* Aether EVT-A RP2354A board configuration. SPDX-License-Identifier: MIT */
#ifndef AETHER_EVT_A_H
#define AETHER_EVT_A_H
// pico_cmake_set PICO_PLATFORM = rp2350-arm-s
#define PICO_RP2350A 1
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1
#define PICO_FLASH_SPI_CLKDIV 2
// pico_cmake_set_default PICO_FLASH_SIZE_BYTES = (2 * 1024 * 1024)
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif
// pico_cmake_set_default PICO_RP2350_A2_SUPPORTED = 1
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif
#endif
