#include "board/k210_boot_io.hpp"

#include "base/crc.hpp"

#include <algorithm>

namespace so {
namespace {
constexpr uint32_t K210_ROM_UART_BAUD = 115200;
constexpr uint8_t UART_APPLICATION_STARTED = 0xc3;
constexpr uint32_t LOADER_ADDRESS = 0x805fc000;
constexpr uint8_t NOP = 0xc2, MEMORY_WRITE = 0xc3, MEMORY_BOOT = 0xc5;
constexpr uint8_t RETURN_OK = 0xe0;
constexpr uint32_t NOP_RETRY_US = 10'000;
constexpr uint32_t ROM_RESPONSE_TIMEOUT_US = 100'000;
constexpr uint8_t SLIP_END = 0xc0, SLIP_ESCAPE = 0xdb;
constexpr uint8_t SLIP_ESCAPED_END = 0xdc, SLIP_ESCAPED_ESCAPE = 0xdd;
constexpr uint8_t ROM_DEBUG_FRAME = 0xd1;

void Put32LE(uint8_t *output, uint32_t value) {
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8);
  output[2] = static_cast<uint8_t>(value >> 16);
  output[3] = static_cast<uint8_t>(value >> 24);
}
void Put32BE(uint8_t *output, uint32_t value) {
  output[0] = static_cast<uint8_t>(value >> 24);
  output[1] = static_cast<uint8_t>(value >> 16);
  output[2] = static_cast<uint8_t>(value >> 8);
  output[3] = static_cast<uint8_t>(value);
}
}

Result K210BootTransport::Perform(BootAction action) {
  switch (action) {
    case BootAction::AssertReset:
      ClearAttempt();
      return port.reset != nullptr && port.reset(port.context, true) ? Result::Ok
                                                                     : Result::Unavailable;
    case BootAction::ConfigureRomUART:
      return port.configureUART != nullptr && port.configureUART(port.context, K210_ROM_UART_BAUD)
                 ? Result::Ok
                 : Result::Unavailable;
    case BootAction::ReleaseReset:
      return port.reset != nullptr && port.reset(port.context, false) ? Result::Ok
                                                                      : Result::Unavailable;
    case BootAction::RomHandshake:
      return RomHandshake();
    case BootAction::UploadLoader:
      return UploadLoader();
    case BootAction::StartLoader:
      return StartLoader();
    case BootAction::AwaitLoaderReady:
      return port.spiReady != nullptr && port.spiReady(port.context) ? Result::Ok : Result::Busy;
    case BootAction::TransferImage:
      return TransferImage();
    case BootAction::AwaitLoaderResult:
      return AwaitLoaderResult();
    case BootAction::AwaitHeliosStart:
      return AwaitHeliosStart();
    case BootAction::AwaitBootReady:
      return port.bootReady == nullptr ? Result::InvalidState : port.bootReady(port.context);
    case BootAction::HoldReset:
      EndImageTransfer();
      return port.reset != nullptr && port.reset(port.context, true) ? Result::Ok
                                                                     : Result::Unavailable;
  }
  return Result::InvalidState;
}

void K210BootTransport::EndImageTransfer() {
  if (imageSelected && port.spiEnd != nullptr)
    port.spiEnd(port.context);
  imageSelected = false;
}

void K210BootTransport::ClearAttempt() {
  EndImageTransfer();
  loaderOffset = 0;
  imageOffset = 0;
  responseSize = 0;
  loaderChunkBytes = 0;
  lastNopUs = 0;
  responseSince = 0;
  commandPending = escapePending = insideFrame = imageTransferred = false;
}

Result K210BootTransport::Send(uint16_t operation, Bytes body) {
  if (port.uartWrite == nullptr || body.size > packet.size() - RomHeaderBytes)
    return Result::InvalidState;
  packet[0] = static_cast<uint8_t>(operation);
  packet[1] = static_cast<uint8_t>(operation >> 8);
  packet[2] = packet[3] = 0;
  Put32LE(packet.data() + 4, operation == NOP ? 0 : CRC32(body));
  for (size_t i = 0; i < body.size; ++i)
    packet[RomHeaderBytes + i] = body.data[i];
  size_t used = 0;
  slip[used++] = SLIP_END;
  for (size_t i = 0; i < body.size + RomHeaderBytes; ++i) {
    const uint8_t byte = packet[i];
    if (byte == SLIP_END || byte == SLIP_ESCAPE) {
      slip[used++] = SLIP_ESCAPE;
      slip[used++] = byte == SLIP_END ? SLIP_ESCAPED_END : SLIP_ESCAPED_ESCAPE;
    } else {
      slip[used++] = byte;
    }
  }
  slip[used++] = SLIP_END;
  return port.uartWrite(port.context, {slip.data(), used}) ? Result::Ok : Result::Unavailable;
}

Result K210BootTransport::Response(uint8_t operation) {
  if (port.uartRead == nullptr)
    return Result::InvalidState;
  std::array<uint8_t, 16> incoming{};
  const size_t count = port.uartRead(port.context, {incoming.data(), incoming.size()});
  for (size_t i = 0; i < count; ++i) {
    const uint8_t byte = incoming[i];
    if (!insideFrame) {
      if (byte == SLIP_END) {
        insideFrame = true;
        escapePending = false;
        responseSize = 0;
      }
      continue;
    }
    if (escapePending) {
      escapePending = false;
      if (byte != SLIP_ESCAPED_END && byte != SLIP_ESCAPED_ESCAPE)
        return Result::Unavailable;
      if (responseSize == response.size())
        return Result::ResourceLimit;
      response[responseSize++] = byte == SLIP_ESCAPED_END ? SLIP_END : SLIP_ESCAPE;
    } else if (byte == SLIP_ESCAPE) {
      escapePending = true;
    } else if (byte == SLIP_END) {
      insideFrame = false;
      if (responseSize < 2)
        return Result::Unavailable;
      if (response[0] == ROM_DEBUG_FRAME)
        continue;  // ROM debug frame; wait for command response.
      if (response[0] != operation || response[1] != RETURN_OK)
        return Result::Unavailable;
      responseSize = 0;
      return Result::Ok;
    } else {
      if (responseSize == response.size())
        return Result::ResourceLimit;
      response[responseSize++] = byte;
    }
  }
  return Result::Busy;
}

Result K210BootTransport::RomHandshake() {
  if (port.nowUs == nullptr)
    return Result::InvalidState;
  if (!commandPending) {
    // Match the ROM greeting used by Kendryte kflash.py: 13 bytes inside SLIP.
    std::array<uint8_t, 5> zeros{};
    const auto result = Send(NOP, {zeros.data(), zeros.size()});
    if (result != Result::Ok)
      return result;
    commandPending = true;
    responseSince = lastNopUs = port.nowUs(port.context);
  }
  if (const auto result = Response(NOP); result != Result::Ok) {
    if (result == Result::Busy) {
      const uint32_t now = port.nowUs(port.context);
      if (static_cast<uint32_t>(now - responseSince) >= ROM_RESPONSE_TIMEOUT_US)
        return Result::Timeout;
      if (now - lastNopUs >= NOP_RETRY_US) {
        std::array<uint8_t, 5> zeros{};
        if (const auto send = Send(NOP, {zeros.data(), zeros.size()}); send != Result::Ok)
          return send;
        lastNopUs = port.nowUs(port.context);
        responseSize = 0;
        escapePending = insideFrame = false;
      }
    }
    return result;
  }
  commandPending = false;
  return Result::Ok;
}

Result K210BootTransport::UploadLoader() {
  if (port.nowUs == nullptr)
    return Result::InvalidState;
  if (loader.data == nullptr || loader.size == 0 || loader.size > 2048)
    return Result::InvalidLength;
  if (commandPending) {
    const auto result = Response(MEMORY_WRITE);
    if (result == Result::Busy &&
        static_cast<uint32_t>(port.nowUs(port.context) - responseSince) >=
            ROM_RESPONSE_TIMEOUT_US)
      return Result::Timeout;
    if (result != Result::Ok)
      return result;
    loaderOffset += loaderChunkBytes;
    commandPending = false;
  }
  if (loaderOffset == loader.size)
    return Result::Ok;
  loaderChunkBytes =
      static_cast<uint16_t>(std::min<size_t>(loader.size - loaderOffset, TransferChunkBytes));
  Put32LE(packet.data() + RomHeaderBytes, LOADER_ADDRESS + loaderOffset);
  Put32LE(packet.data() + RomHeaderBytes + 4, loaderChunkBytes);
  for (uint16_t i = 0; i < loaderChunkBytes; ++i)
    packet[RomHeaderBytes + MemoryWriteHeaderBytes + i] = loader.data[loaderOffset + i];
  if (const auto result =
          Send(MEMORY_WRITE, {packet.data() + RomHeaderBytes,
                             static_cast<size_t>(loaderChunkBytes) + MemoryWriteHeaderBytes});
      result != Result::Ok)
    return result;
  commandPending = true;
  responseSince = port.nowUs(port.context);
  return Result::Busy;
}

Result K210BootTransport::StartLoader() {
  std::array<uint8_t, 8> body{};
  Put32LE(body.data(), LOADER_ADDRESS);
  return Send(MEMORY_BOOT, {body.data(), body.size()});
}

Result K210BootTransport::TransferImage() {
  if (imageTransferred)
    return Result::Ok;
  if (image.data == nullptr || image.size == 0 || image.size > UINT32_MAX ||
      port.spiBegin == nullptr || port.spiWrite == nullptr || port.spiEnd == nullptr)
    return Result::InvalidLength;
  if (!imageSelected) {
    std::array<uint8_t, 8> prefix{};
    Put32BE(prefix.data(), static_cast<uint32_t>(image.size));
    Put32BE(prefix.data() + 4, imageCRC);
    if (!port.spiBegin(port.context))
      return Result::Unavailable;
    imageSelected = true;
    if (!port.spiWrite(port.context, {prefix.data(), prefix.size()})) {
      EndImageTransfer();
      return Result::Unavailable;
    }
  }
  // The K210 receives 32-bit MSB-first frames. Reverse each wire word so
  // its DMA writes the original little-endian image bytes into SRAM.
  const size_t padded = (image.size + 3u) & ~size_t{3};
  const size_t chunkBytes = std::min(padded - imageOffset, TransferChunkBytes);
  for (size_t i = 0; i < chunkBytes; i += 4) {
    for (size_t byte = 0; byte < 4; ++byte) {
      const size_t source = imageOffset + i + 3u - byte;
      packet[i + byte] = source < image.size ? image.data[source] : 0;
    }
  }
  if (!port.spiWrite(port.context, {packet.data(), chunkBytes})) {
    EndImageTransfer();
    return Result::Unavailable;
  }
  imageOffset += chunkBytes;
  if (imageOffset != padded)
    return Result::Busy;
  EndImageTransfer();
  imageTransferred = true;
  return Result::Ok;
}

Result K210BootTransport::AwaitLoaderResult() {
  uint8_t result = 0xff;
  if (port.uartRead == nullptr || port.uartRead(port.context, {&result, 1}) == 0)
    return Result::Busy;
  switch (result) {
    case 0:
      return Result::Ok;
    case 1:
      return Result::InvalidLength;
    case 2:
      return Result::CRCMismatch;
    default:
      return Result::Unavailable;
  }
}

Result K210BootTransport::AwaitHeliosStart() {
  if (port.uartRead == nullptr)
    return Result::InvalidState;
  uint8_t started = 0;
  if (port.uartRead(port.context, {&started, 1}) == 0)
    return Result::Busy;
  return started == UART_APPLICATION_STARTED ? Result::Ok : Result::Unavailable;
}

}  // namespace so
