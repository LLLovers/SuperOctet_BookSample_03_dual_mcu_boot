#include "hardware.h"
#include "board/hardware_config.h"

#include "dmac.h"
#include "encoding.h"
#include "fpioa.h"
#include "gpiohs.h"
#include "platform.h"
#include "spi.h"
#include "sysctl.h"
#include "uarths.h"

enum {
  SPIClockIO = PIN_K210_SPI_CLK,
  SPISelectIO = PIN_K210_SPI_CS,
  SPISelectSenseIO = PIN_K210_SPI_CS_SENSE,
  SPIMOSIIO = PIN_K210_SPI_MOSI,
  SPIReadyIO = PIN_K210_READY,
  SelectSenseGPIO = SPI_SELECT_GPIOHS,
  ReadyGPIO = SPI_READY_GPIOHS,
  SelectMask = 1u << SelectSenseGPIO,
  ReadyMask = 1u << ReadyGPIO,
  DMAChannelEnable = 1u << SPI_DMAC,
  DMAChannelWriteEnable = 1u << (8 + SPI_DMAC),
  DMAMaximumBlockWords = 1u << 22,
  UARTTxFull = 1u << 31,
};

// The compact loader uses DMA0; reject a board reassignment instead of silently
// disagreeing with the firmware configuration.
_Static_assert(SPI_DMAC == DMAC_CHANNEL0, "loader requires SPI DMA0");

// A 32-bit DMA block covers the entire allowed image, including its last padded word.
_Static_assert((LoaderMaximumImageBytes + 3u) / 4u <= DMAMaximumBlockWords,
               "loader image must fit in one DMA block");

static volatile spi_t *const spi2 = (volatile spi_t *)SPI_SLAVE_BASE_ADDR;
static volatile dmac_t *const dma = (volatile dmac_t *)DMAC_BASE_ADDR;
static volatile sysctl_t *const systemControl = (volatile sysctl_t *)SYSCTL_BASE_ADDR;
static volatile fpioa_t *const io = (volatile fpioa_t *)FPIOA_BASE_ADDR;
static volatile gpiohs_raw_t *const gpio = (volatile gpiohs_raw_t *)GPIOHS_BASE_ADDR;
// Keep UART accesses at 32 bits: bitfield/aggregate accesses may split a FIFO write.
static volatile uint32_t *const uartTX =
    (volatile uint32_t *)(UARTHS_BASE_ADDR + UARTHS_REG_TXFIFO);
static volatile uint32_t *const uartTXControl =
    (volatile uint32_t *)(UARTHS_BASE_ADDR + UARTHS_REG_TXCTRL);
static volatile uint32_t *const uartDivider =
    (volatile uint32_t *)(UARTHS_BASE_ADDR + UARTHS_REG_DIV);

// Kept in a loaded section so hart 1 can never observe stale SRAM from a prior boot.
volatile uint32_t loader_go __attribute__((section(".loader_state"))) = 0;

static fpioa_io_config_t InputFunction(uint8_t function) {
  return (fpioa_io_config_t){.ch_sel = function, .ie_en = 1, .st = 1};
}

static fpioa_io_config_t GPIOFunction(uint8_t function) {
  return (fpioa_io_config_t){.ch_sel = function, .ds = 15, .oe_en = 1, .ie_en = 1, .st = 1};
}

void loader_initialize(void) {
  systemControl->clk_en_cent.apb0_clk_en = 1;
  systemControl->clk_en_peri.dma_clk_en = 1;
  systemControl->clk_en_peri.gpio_clk_en = 1;
  systemControl->clk_en_peri.spi2_clk_en = 1;
  systemControl->clk_en_peri.fpioa_clk_en = 1;
  systemControl->clk_th1.spi2_clk_threshold = 0;
  systemControl->peri_reset.spi2_reset = 1;
  for (volatile unsigned wait = 0; wait < 10000; ++wait) {
  }
  systemControl->peri_reset.spi2_reset = 0;

  io->io[SPIClockIO] = InputFunction(FUNC_SPI_SLAVE_SCLK);
  io->io[SPISelectIO] = InputFunction(FUNC_SPI_SLAVE_SS);
  // Both physical CS pins are required: IO36 feeds SPI2 and IO37 observes
  // deassertion without stealing the hardware slave-select function.
  io->io[SPISelectSenseIO] = GPIOFunction(FUNC_GPIOHS0 + SelectSenseGPIO);
  io->io[SPIMOSIIO] = (fpioa_io_config_t){.ch_sel = FUNC_SPI_SLAVE_D0,
                                          .ds = 15,
                                          .oe_en = 1,
                                          .oe_inv = 1,
                                          .ie_en = 1,
                                          .ie_inv = 1,
                                          .st = 1};
  io->io[SPIReadyIO] = GPIOFunction(FUNC_GPIOHS0 + ReadyGPIO);

  gpio->input_en |= SelectMask;
  gpio->output_en &= ~SelectMask;
  gpio->pullup_en |= SelectMask;
  gpio->input_en &= ~ReadyMask;
  gpio->output_val &= ~ReadyMask;
  gpio->output_en |= ReadyMask;

  spi2->ssienr = 0;
  // The SSI data register requires 32-bit DMA reads. Pack four wire bytes per frame.
  spi2->ctrlr0 = (1u << 10) | (31u << 16);  // mode 0, RX-only slave, 32-bit frame
  spi2->dmatdlr = 4;
  spi2->dmardlr = 3;
  spi2->dmacr = 0;
  spi2->txftlr = 0;
  spi2->rxftlr = 7;
  spi2->imr = 0;
  spi2->ssienr = 1;

  dma->reset = 1;
  while ((dma->reset & 1u) != 0) {
  }
  dma->cfg = 1;  // global DMA enable; interrupts remain disabled
  systemControl->dma_sel0.dma_sel0 = SYSCTL_DMA_SELECT_SSI2_RX_REQ;
  gpio->output_val |= ReadyMask;
}

enum LoaderResult loader_receive_header(struct LoaderImageHeader *header) {
  while ((gpio->input_val & SelectMask) != 0) {
  }
  while (spi2->rxflr < 2) {
    if ((gpio->input_val & SelectMask) != 0)
      return LoaderSizeError;
  }
  header->bytes = spi2->dr[0];
  header->crc = spi2->dr[0];
  gpio->output_val &= ~ReadyMask;
  if (header->bytes == 0 || header->bytes > LoaderMaximumImageBytes)
    return LoaderSizeError;
  return LoaderSuccess;
}

void loader_receive_image(uint32_t bytes) {
  // The header has been validated. Keep CS asserted through this single DMA block.
  volatile dmac_channel_t *const channel = &dma->channel[SPI_DMAC];
  dma->chen = DMAChannelWriteEnable;
  while ((dma->chen & DMAChannelEnable) != 0) {
  }
  channel->intclear = ~(uint64_t)0;
  channel->cfg = ((uint64_t)DMAC_PRF2MEM_DMA << 32) | ((uint64_t)1 << 36);
  channel->sar = (uintptr_t)&spi2->dr[0];
  channel->dar = LoaderImageAddress;
  channel->block_ts = (bytes + 3u) / 4u - 1u;
  channel->ctl = ((uint64_t)DMAC_MASTER2 << 2) | ((uint64_t)DMAC_ADDR_NOCHANGE << 4) |
                 ((uint64_t)DMAC_TRANS_WIDTH_32 << 8) | ((uint64_t)DMAC_TRANS_WIDTH_32 << 11) |
                 ((uint64_t)DMAC_MSIZE_4 << 14) | ((uint64_t)DMAC_MSIZE_4 << 18);
  spi2->dmacr = 1;
  dma->chen = DMAChannelWriteEnable | DMAChannelEnable;
  while ((dma->chen & DMAChannelEnable) != 0) {
  }
  __asm__ volatile("fence rw,rw" ::: "memory");
  while ((gpio->input_val & SelectMask) == 0) {
  }
}

void loader_report_result(enum LoaderResult result) {
  gpio->output_val &= ~ReadyMask;
  spi2->dmacr = 0;
  spi2->ssienr = 0;

  // ROM ISP already owns UARTHS at 115200 8N1. Preserve its divisor and pin
  // routing and return exactly one byte.
  *uartTXControl |= UARTHS_TXEN;
  while ((*uartTX & UARTTxFull) != 0) {
  }
  *uartTX = (uint32_t)result;
  // UARTHS exposes no shift-register-empty bit. Its divisor is relative to
  // mcycle, so twelve complete bit periods cover the 8N1 character and margin.
  const uint64_t drainCycles = ((uint64_t)(*uartDivider & 0xffffu) + 1u) * 12u;
  const uint64_t begin = read_cycle();
  while (read_cycle() - begin < drainCycles) {
  }
}

__attribute__((noreturn)) void loader_start_image(void) {
  __asm__ volatile(
      "csrci mstatus, 8\n"
      "fence rw,rw\n"
      "fence.i" ::
          : "memory");
  loader_go = 1;
  __asm__ volatile(
      "fence rw,rw\n"
      "fence.i" ::
          : "memory");
  ((void (*)(void))(uintptr_t)LoaderImageAddress)();
  __builtin_unreachable();
}
