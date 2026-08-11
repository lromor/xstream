/*
 * Copyright (C) 2017-2020  The Project X-Ray Authors.
 *
 * Use of this source code is governed by a ISC-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/ISC
 *
 * SPDX-License-Identifier: ISC
 */
#ifndef FPGA_XILINX_ARCH_XC7_CONFIGURATION_PACKET_H
#define FPGA_XILINX_ARCH_XC7_CONFIGURATION_PACKET_H

#include <cstdint>
#include <memory>
#include <ostream>
#include <vector>

#include "absl/types/span.h"
#include "fpga/xilinx/arch-xc7-frame.h"
#include "fpga/xilinx/bit-ops.h"
#include "fpga/xilinx/configuration-packet.h"

namespace fpga {
namespace xilinx {
namespace xc7 {
enum class Command : uint32_t {
  kNOP = 0x0,
  kWCFG = 0x1,
  kMFW = 0x2,
  kLFRM = 0x3,
  kRCFG = 0x4,
  kSTART = 0x5,
  kRCAP = 0x6,
  kRCRC = 0x7,
  kAGHIGH = 0x8,
  kSWITCH = 0x9,
  kGRESTORE = 0xA,
  kSHUTDOWN = 0xB,
  kGCAPTURE = 0xC,
  kDESYNC = 0xD,
  kIPROG = 0xF,
  kCRCC = 0x10,
  kLTIMER = 0x11,
  kBSPI_READ = 0x12,
  kFALL_EDGE = 0x13,
};

// Series-7 configuration register addresses
// according to UG470, pg. 109
enum class ConfigurationRegister : unsigned int {
  kCRC = 0x00,
  kFAR = 0x01,
  kFDRI = 0x02,
  kFDRO = 0x03,
  kCMD = 0x04,
  kCTL0 = 0x05,
  kMASK = 0x06,
  kSTAT = 0x07,
  kLOUT = 0x08,
  kCOR0 = 0x09,
  kMFWR = 0x0a,
  kCBC = 0x0b,
  kIDCODE = 0x0c,
  kAXSS = 0x0d,
  kCOR1 = 0x0e,
  kWBSTAR = 0x10,
  kTIMER = 0x11,
  kUNKNOWN = 0x13,
  kBOOTSTS = 0x16,
  kCTL1 = 0x18,
  kBSPI = 0x1F,
};

std::ostream &operator<<(std::ostream &o, const ConfigurationRegister &value);

class ConfigurationPacket
    : public ConfigurationPacketBase<ConfigurationRegister,
                                     ConfigurationPacket> {
 private:
  using BaseType =
    ConfigurationPacketBase<ConfigurationRegister, ConfigurationPacket>;

 public:
  using BaseType::BaseType;
  using ConfRegType = ConfigurationRegister;

 private:
  friend BaseType;
  static ParseResult InitWithWordsImpl(
    absl::Span<FrameWord> words,
    const ConfigurationPacket *previous_packet = nullptr);
};

template <typename ConfigRegType>
using ConfigurationPackage = std::vector<std::unique_ptr<ConfigurationPacket>>;

class ConfigurationOptions0Value {
 public:
  ConfigurationOptions0Value() = default;

  enum class StartupClockSource : uint32_t {
    CCLK = 0x0,
    User = 0x1,
    JTAG = 0x2,
  };

  enum class SignalReleaseCycle : uint32_t {
    Phase1 = 0x0,
    Phase2 = 0x1,
    Phase3 = 0x2,
    Phase4 = 0x3,
    Phase5 = 0x4,
    Phase6 = 0x5,
    TrackDone = 0x6,
    Keep = 0x7,
  };

  enum class StallCycle : uint32_t {
    Phase0 = 0x0,
    Phase1 = 0x1,
    Phase2 = 0x2,
    Phase3 = 0x3,
    Phase4 = 0x4,
    Phase5 = 0x5,
    Phase6 = 0x6,
    NoWait = 0x7,
  };

  explicit operator uint32_t() const { return value_; }

  ConfigurationOptions0Value &SetUseDonePinAsPowerdownStatus(bool enabled) {
    value_ = bit_field_set(value_, 27, 27, enabled ? 1 : 0);
    return *this;
  }

  ConfigurationOptions0Value &SetAddPipelineStageForDoneIn(bool enabled) {
    value_ = bit_field_set(value_, 25, 25, enabled ? 1 : 0);
    return *this;
  }

  ConfigurationOptions0Value &SetDriveDoneHigh(bool enabled) {
    value_ = bit_field_set(value_, 24, 24, enabled);
    return *this;
  }

  ConfigurationOptions0Value &SetReadbackIsSingleShot(bool enabled) {
    value_ = bit_field_set(value_, 23, 23, enabled);
    return *this;
  }

  ConfigurationOptions0Value &SetCclkFrequency(uint32_t mhz) {
    value_ = bit_field_set(value_, 22, 17, mhz);
    return *this;
  }

  ConfigurationOptions0Value &SetStartupClockSource(StartupClockSource source) {
    value_ = bit_field_set(value_, 16, 15, static_cast<uint32_t>(source));
    return *this;
  }

  ConfigurationOptions0Value &SetReleaseDonePinAtStartupCycle(
    SignalReleaseCycle cycle) {
    value_ = bit_field_set(value_, 14, 12, static_cast<uint32_t>(cycle));
    return *this;
  }

  ConfigurationOptions0Value &SetStallAtStartupCycleUntilDciMatch(
    StallCycle cycle) {
    value_ = bit_field_set(value_, 11, 9, static_cast<uint32_t>(cycle));
    return *this;
  };

  ConfigurationOptions0Value &SetStallAtStartupCycleUntilMmcmLock(
    StallCycle cycle) {
    value_ = bit_field_set(value_, 8, 6, static_cast<uint32_t>(cycle));
    return *this;
  };

  ConfigurationOptions0Value &SetReleaseGtsSignalAtStartupCycle(
    SignalReleaseCycle cycle) {
    value_ = bit_field_set(value_, 5, 3, static_cast<uint32_t>(cycle));
    return *this;
  }

  ConfigurationOptions0Value &SetReleaseGweSignalAtStartupCycle(
    SignalReleaseCycle cycle) {
    value_ = bit_field_set(value_, 2, 0, static_cast<uint32_t>(cycle));
    return *this;
  }

 private:
  uint32_t value_ = 0;
};
}  // namespace xc7
}  // namespace xilinx
}  // namespace fpga
#endif  // FPGA_XILINX_ARCH_XC7_CONFIGURATION_PACKET_H
