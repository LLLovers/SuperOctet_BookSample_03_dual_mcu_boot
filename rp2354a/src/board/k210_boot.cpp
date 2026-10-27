#include "boot/boot.hpp"

namespace so {
namespace {
constexpr uint32_t STAGE_TIMEOUT_US = 100000;
constexpr uint32_t IMAGE_TRANSFER_TIMEOUT_US = 500000;
// Helios waits for DAC clock detection and analog power-up before it can start responding.
constexpr uint32_t HELIOS_INIT_TIMEOUT_US = 2000000;
constexpr uint32_t BOOT_READY_TIMEOUT_US = 500000;
}

Result BootSequence::Start(uint32_t nowUs) {
  if (state != BootState::Off)
    return Result::InvalidState;
  if (io.perform == nullptr)
    return Result::InvalidState;
  diagnostics = {};
  diagnostics.attempts = 1;
  bootSince = stageSince = nowUs;
  fault = BootFault::None;
  retried = resetHeld = false;
  state = BootState::AssertReset;
  return Result::Ok;
}

uint32_t BootSequence::TimeoutFor(BootState state) const {
  switch (state) {
    case BootState::TransferImage:
      return IMAGE_TRANSFER_TIMEOUT_US;
    case BootState::AwaitHeliosStart:
      return HELIOS_INIT_TIMEOUT_US;
    case BootState::AwaitBootReady:
      return BOOT_READY_TIMEOUT_US;
    default:
      return STAGE_TIMEOUT_US;
  }
}

Result BootSequence::Fail(BootFault fault, Result result, uint32_t nowUs) {
  ++diagnostics.bootFailures;
  this->fault = fault;
  if (!retried) {
    retried = true;
    ++diagnostics.retries;
    ++diagnostics.attempts;
    state = BootState::AssertReset;
    stageSince = nowUs;
    return Result::Busy;
  }
  state = BootState::HoldReset;
  stageSince = nowUs;
  return result;
}

Result BootSequence::Run(BootAction action, BootState next, BootFault fault, uint32_t nowUs) {
  const auto result = io.perform(io.context, action);
  if (result == Result::Ok) {
    state = next;
    stageSince = nowUs;
    return Result::Ok;
  }
  if (result == Result::Busy) {
    if (static_cast<uint32_t>(nowUs - stageSince) < TimeoutFor(state))
      return Result::Busy;
    ++diagnostics.stageTimeouts;
    return Fail(BootFault::Timeout, Result::Unavailable, nowUs);
  }
  if (result == Result::CRCMismatch)
    fault = BootFault::ImageCRC;
  return Fail(fault, result, nowUs);
}

Result BootSequence::Pump(uint32_t nowUs) {
  switch (state) {
    case BootState::Off:
      return Result::InvalidState;
    case BootState::AssertReset:
      return Run(BootAction::AssertReset, BootState::ConfigureRomUART, BootFault::Reset, nowUs);
    case BootState::ConfigureRomUART:
      return Run(BootAction::ConfigureRomUART, BootState::ReleaseReset, BootFault::Reset, nowUs);
    case BootState::ReleaseReset:
      return Run(BootAction::ReleaseReset, BootState::RomHandshake, BootFault::Reset, nowUs);
    case BootState::RomHandshake:
      return Run(BootAction::RomHandshake, BootState::UploadLoader, BootFault::RomHandshake, nowUs);
    case BootState::UploadLoader:
      return Run(BootAction::UploadLoader, BootState::StartLoader, BootFault::LoaderUpload, nowUs);
    case BootState::StartLoader:
      return Run(BootAction::StartLoader, BootState::AwaitLoaderReady, BootFault::LoaderUpload,
                 nowUs);
    case BootState::AwaitLoaderReady:
      return Run(BootAction::AwaitLoaderReady, BootState::TransferImage, BootFault::LoaderReady,
                 nowUs);
    case BootState::TransferImage:
      return Run(BootAction::TransferImage, BootState::AwaitLoaderResult, BootFault::ImageTransfer,
                 nowUs);
    case BootState::AwaitLoaderResult:
      return Run(BootAction::AwaitLoaderResult, BootState::AwaitHeliosStart,
                 BootFault::ImageTransfer, nowUs);
    case BootState::AwaitHeliosStart:
      return Run(BootAction::AwaitHeliosStart, BootState::AwaitBootReady, BootFault::HeliosStart,
                 nowUs);
    case BootState::AwaitBootReady: {
      const auto result =
          Run(BootAction::AwaitBootReady, BootState::Ready, BootFault::BootReady, nowUs);
      if (state == BootState::Ready) {
        fault = BootFault::None;
        diagnostics.readyUs = static_cast<uint32_t>(nowUs - bootSince);
      }
      return result;
    }
    case BootState::Ready:
      return Result::Ok;
    case BootState::HoldReset: {
      const auto result = io.perform(io.context, BootAction::HoldReset);
      if (result == Result::Ok) {
        resetHeld = true;
        state = BootState::Fault;
      }
      return result;
    }
    case BootState::Fault:
      return Result::Unavailable;
  }
  return Result::InvalidState;
}

bool BootSequence::AcceptMIDI() {
  if (state == BootState::Ready)
    return true;
  if (diagnostics.discardedMIDI != UINT32_MAX)
    ++diagnostics.discardedMIDI;
  return false;
}

}  // namespace so
