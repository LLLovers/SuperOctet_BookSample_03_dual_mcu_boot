/* Aether EVT-A RP2354A board configuration and pins. SPDX-License-Identifier: MIT */
#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H
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

#define PIN_ACTIVITY_CORE0 1
#define PIN_ACTIVITY_CORE1 0
#define PIN_TX 16
#define PIN_RX 17
#define MIDI_UART uart0
#define UART_MIDI_BAUD 3000000UL
#define K210_SPI spi0
#define PIN_K210_READY 18
#define PIN_K210_SPI_MOSI 19
#define PIN_K210_SPI_MISO 20
#define PIN_K210_SPI_CS 21
#define PIN_K210_SPI_CLK 22
#define PIN_K210_RESET 23
#endif
