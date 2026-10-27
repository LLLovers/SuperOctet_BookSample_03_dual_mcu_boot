#pragma once

#include "base/result.hpp"

#include <cstdint>

namespace so {

enum class BootAction : uint8_t {
  AssertReset,
  ConfigureRomUART,
  ReleaseReset,
  RomHandshake,
  UploadLoader,
  StartLoader,
  AwaitLoaderReady,
  TransferImage,
  AwaitLoaderResult,
  AwaitHeliosStart,
  AwaitBootReady,
  HoldReset
};

enum class BootState : uint8_t {
  Off,
  AssertReset,
  ConfigureRomUART,
  ReleaseReset,
  RomHandshake,
  UploadLoader,
  StartLoader,
  AwaitLoaderReady,
  TransferImage,
  AwaitLoaderResult,
  AwaitHeliosStart,
  AwaitBootReady,
  Ready,
  HoldReset,
  Fault
};

enum class BootFault : uint8_t {
  None,
  Reset,
  RomHandshake,
  LoaderUpload,
  LoaderReady,
  ImageTransfer,
  ImageCRC,
  HeliosStart,
  BootReady,
  Timeout,
};

struct BootIo {
  void *context = nullptr;
  Result (*perform)(void *, BootAction) = nullptr;
};

struct BootDiagnostics {
  uint32_t attempts = 0;
  uint32_t retries = 0;
  uint32_t bootFailures = 0;
  uint32_t stageTimeouts = 0;
  uint32_t discardedMIDI = 0;
  uint32_t readyUs = 0;
};

// Board-independent boot sequencing. ROM electrical framing and SPI DMA live in the adapter.
// Busy means that stage has not produced its response yet. A second failure is terminal.
class BootSequence {
 public:
  explicit BootSequence(BootIo io) : io(io) {}
  Result Start(uint32_t nowUs);
  Result Pump(uint32_t nowUs);
  bool AcceptMIDI();

  BootState State() const { return state; }
  BootFault Fault() const { return fault; }
  bool Ready() const { return state == BootState::Ready; }
  bool ResetHeld() const { return resetHeld; }
  const BootDiagnostics &Diagnostics() const { return diagnostics; }

 private:
  Result Run(BootAction action, BootState next, BootFault fault, uint32_t nowUs);
  Result Fail(BootFault fault, Result result, uint32_t nowUs);
  uint32_t TimeoutFor(BootState state) const;

  BootIo io{};
  BootDiagnostics diagnostics{};
  BootState state = BootState::Off;
  BootFault fault = BootFault::None;
  uint32_t bootSince = 0;
  uint32_t stageSince = 0;
  bool retried = false;
  bool resetHeld = false;
};

}  // namespace so
