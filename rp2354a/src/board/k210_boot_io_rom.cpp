#include "board/k210_boot.hpp"

#include "boot/boot.hpp"
#include "board/k210_boot_io.hpp"
#include "hardware_config.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/time.h"

extern "C" {
extern const uint8_t so_loader_image[];
extern const uint32_t so_loader_size;
extern const uint8_t so_helios_image[];
extern const uint32_t so_helios_size;
extern const uint32_t so_helios_crc32;
}

namespace {

constexpr uint32_t RESET_PULSE_US = 10'000;
constexpr uint32_t BOOT_SPI_HZ = 25'000'000;
constexpr uint32_t BOOT_PUMP_INTERVAL_US = 50;

uint32_t resetAssertedAt = 0;
int spiDMA = -1;
bool spiSelected = false;
bool runtimeBaudSet = false;
bool uartInitialized = false;

bool ResetK210(void *, bool asserted) {
  if (asserted) {
    resetAssertedAt = time_us_32();
    runtimeBaudSet = false;
  } else {
    const uint32_t elapsed = time_us_32() - resetAssertedAt;
    if (elapsed < RESET_PULSE_US)
      sleep_us(RESET_PULSE_US - elapsed);
  }
  gpio_put(PIN_K210_RESET, asserted);
  return true;
}

bool ConfigureUART(void *, uint32_t baud) {
  uint32_t actualBaud = 0;
  if (!uartInitialized) {
    actualBaud = uart_init(MIDI_UART, baud);
    uart_set_fifo_enabled(MIDI_UART, false);
    gpio_set_function(PIN_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_RX, GPIO_FUNC_UART);
    uartInitialized = true;
  } else {
    uart_tx_wait_blocking(MIDI_UART);
    actualBaud = uart_set_baudrate(MIDI_UART, baud);
    while (uart_is_readable(MIDI_UART))
      (void)uart_getc(MIDI_UART);
    uart_get_hw(MIDI_UART)->rsr = 0;
  }
  return actualBaud >= baud - baud / 100 && actualBaud <= baud + baud / 100;
}

bool UARTWrite(void *, so::Bytes bytes) {
  if (bytes.data == nullptr || bytes.size == 0)
    return false;
  uart_write_blocking(MIDI_UART, bytes.data, bytes.size);
  // uart_write_blocking only fills the FIFO. Start response deadlines after wire completion.
  uart_tx_wait_blocking(MIDI_UART);
  return true;
}

size_t UARTRead(void *, so::Buffer buffer) {
  size_t count = 0;
  while (count < buffer.size && uart_is_readable(MIDI_UART)) {
    buffer.data[count++] = uart_getc(MIDI_UART);
  }
  return count;
}

bool SPIReady(void *) {
  return gpio_get(PIN_K210_READY);
}
bool SPIBegin(void *) {
  if (spiSelected)
    return false;
  gpio_put(PIN_K210_SPI_CS, false);
  spiSelected = true;
  return true;
}

bool SPIWrite(void *, so::Bytes bytes) {
  if (!spiSelected || bytes.data == nullptr || bytes.size == 0 || spiDMA < 0 ||
      bytes.size > UINT32_MAX)
    return false;
  const uint channel = static_cast<uint>(spiDMA);
  dma_channel_config config = dma_channel_get_default_config(channel);
  channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
  channel_config_set_read_increment(&config, true);
  channel_config_set_write_increment(&config, false);
  channel_config_set_dreq(&config, spi_get_dreq(K210_SPI, true));
  dma_channel_configure(channel, &config, &spi_get_hw(K210_SPI)->dr, bytes.data,
                        static_cast<uint32_t>(bytes.size), true);
  dma_channel_wait_for_finish_blocking(channel);
  while (spi_is_busy(K210_SPI))
    tight_loop_contents();
  while (spi_is_readable(K210_SPI))
    (void)spi_get_hw(K210_SPI)->dr;
  spi_get_hw(K210_SPI)->icr = SPI_SSPICR_RORIC_BITS;
  return true;
}

void SPIEnd(void *) {
  gpio_put(PIN_K210_SPI_CS, true);
  spiSelected = false;
}

so::Result BootReady(void *) {
  if (!runtimeBaudSet) {
    if (!ConfigureUART(nullptr, UART_MIDI_BAUD))
      return so::Result::Unavailable;
    runtimeBaudSet = true;
  }
  return so::Result::Ok;
}

uint32_t NowUs(void *) {
  return time_us_32();
}

bool InitializeBootHardware() {
  gpio_init(PIN_K210_RESET);
  gpio_put(PIN_K210_RESET, true);
  gpio_set_dir(PIN_K210_RESET, GPIO_OUT);
  resetAssertedAt = time_us_32();

  gpio_init(PIN_K210_SPI_CS);
  gpio_put(PIN_K210_SPI_CS, true);
  gpio_set_dir(PIN_K210_SPI_CS, GPIO_OUT);
  gpio_init(PIN_K210_READY);
  gpio_set_dir(PIN_K210_READY, GPIO_IN);
  gpio_disable_pulls(PIN_K210_READY);
  gpio_set_function(PIN_K210_SPI_CLK, GPIO_FUNC_SPI);
  gpio_set_function(PIN_K210_SPI_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(PIN_K210_SPI_MISO, GPIO_FUNC_SPI);
  spi_init(K210_SPI, BOOT_SPI_HZ);
  spi_set_format(K210_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  spiDMA = dma_claim_unused_channel(false);
  return spiDMA >= 0;
}

}  // namespace

bool UniBootK210() {
  if (!InitializeBootHardware())
    return false;

  const so::K210BootPort port{nullptr,  ResetK210, ConfigureUART, UARTWrite, UARTRead, SPIReady,
                              SPIBegin, SPIWrite,  SPIEnd,        BootReady, NowUs};
  so::K210BootTransport transport(port, {so_loader_image, so_loader_size},
                                  {so_helios_image, so_helios_size}, so_helios_crc32);
  so::BootSequence sequence(transport.Interface());
  if (sequence.Start(time_us_32()) != so::Result::Ok)
    return false;
  while (!sequence.Ready() && sequence.State() != so::BootState::Fault) {
    (void)sequence.Pump(time_us_32());
    sleep_us(BOOT_PUMP_INTERVAL_US);
  }
  return sequence.Ready();
}
