#pragma once

#include "boot/boot.hpp"
#include "base/bytes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace so {

struct K210BootPort {
  void *context = nullptr;
  bool (*reset)(void *, bool asserted) = nullptr;
  bool (*configureUART)(void *, uint32_t baud) = nullptr;
  // Return only after the last stop bit has left the UART.
  bool (*uartWrite)(void *, Bytes) = nullptr;
  size_t (*uartRead)(void *, Buffer) = nullptr;
  bool (*spiReady)(void *) = nullptr;
  bool (*spiBegin)(void *) = nullptr;
  bool (*spiWrite)(void *, Bytes) = nullptr;
  void (*spiEnd)(void *) = nullptr;
  Result (*bootReady)(void *) = nullptr;
  uint32_t (*nowUs)(void *) = nullptr;
};

// K210 ROM/loader transport with bounded response waits. ROM packets use little-endian ISP framing
// inside SLIP; the loader image is written in 1 KiB records. The Helios image is one continuous
// SPI CS assertion containing size, CRC32, then the raw image.
class K210BootTransport {
 public:
  K210BootTransport(K210BootPort port, Bytes loader, Bytes image, uint32_t imageCRC)
      : port(port), loader(loader), image(image), imageCRC(imageCRC) {}
  Result Perform(BootAction action);
  BootIo Interface() { return {this, PerformThunk, NowThunk, LoaderResultTimeoutUs(image.size)}; }
  uint32_t LoaderOffset() const { return loaderOffset; }
  // ROM-clock CRC probe: 0x5fc000 bytes in 281.2 ms at 390 MHz (<46 us/KiB).
  // Budget 100 us/KiB (>2x measured CRC time), plus 100 ms for DMA drain/UART.
  // Clamp the budget to the application region; the loader rejects larger images.
  static uint32_t LoaderResultTimeoutUs(size_t imageBytes) {
    const size_t bytes = imageBytes < 0x5fc000 ? imageBytes : 0x5fc000;
    return 100000 + static_cast<uint32_t>((bytes + 1023) / 1024) * 100;
  }

 private:
  static Result PerformThunk(void *context, BootAction action) {
    return static_cast<K210BootTransport *>(context)->Perform(action);
  }
  static uint32_t NowThunk(void *context) {
    auto &transport = *static_cast<K210BootTransport *>(context);
    return transport.port.nowUs == nullptr ? 0 : transport.port.nowUs(transport.port.context);
  }
  Result Send(uint16_t operation, Bytes body);
  Result Response(uint8_t operation);
  Result RomHandshake();
  Result UploadLoader();
  Result StartLoader();
  Result TransferImage();
  Result AwaitLoaderResult();
  Result AwaitHeliosStart();
  void EndImageTransfer();
  void ClearAttempt();

  static constexpr size_t TransferChunkBytes = 1024;
  static constexpr size_t RomHeaderBytes = 8;
  static constexpr size_t MemoryWriteHeaderBytes = 8;
  static constexpr size_t PacketBytes =
      RomHeaderBytes + MemoryWriteHeaderBytes + TransferChunkBytes;

  K210BootPort port{};
  Bytes loader{}, image{};
  uint32_t imageCRC = 0, loaderOffset = 0;
  size_t imageOffset = 0;
  std::array<uint8_t, PacketBytes> packet{};
  std::array<uint8_t, PacketBytes * 2 + 2> slip{};
  std::array<uint8_t, 64> response{};
  size_t responseSize = 0;
  uint32_t lastNopUs = 0, responseSince = 0;
  uint16_t loaderChunkBytes = 0;
  bool commandPending = false;
  bool escapePending = false, insideFrame = false;
  bool imageTransferred = false, imageSelected = false;
};

}  // namespace so
