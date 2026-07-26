/*
 * Copyright (C) 2017-2020  The Project X-Ray Authors.
 *
 * Use of this source code is governed by a ISC-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/ISC
 *
 * SPDX-License-Identifier: ISC
 */
#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "fpga/xilinx/arch-types.h"
#include "fpga/xilinx/bitstream-reader.h"
#include "gtest/gtest.h"

namespace fpga {
namespace xilinx {
namespace {
constexpr fpga::xilinx::Architecture kArch = fpga::xilinx::Architecture::kXC7;
using ArchType = ArchitectureType<kArch>;
using ConfigurationPacket = ArchType::ConfigurationPacket;
using ConfigurationRegister = ArchType::ConfigurationRegister;

TEST(XC7BitstreamReaderTest, InitWithEmptyBytesReturnsNull) {
  absl::Span<uint8_t> const bitstream;
  auto reader = BitstreamReader<kArch>::InitWithBytes(bitstream);
  EXPECT_FALSE(reader);
}

TEST(XC7BitstreamReaderTest, InitWithOnlySyncReturnsObject) {
  std::vector<uint8_t> const bitstream{0xAA, 0x99, 0x55, 0x66};
  auto reader = BitstreamReader<kArch>::InitWithBytes(bitstream);
  EXPECT_TRUE(reader);
}

TEST(XC7BitstreamReaderTest,
     InitWithSyncAfterNonWordSizedPaddingReturnsObject) {
  std::vector<uint8_t> const bitstream{0xFF, 0xFE, 0xAA, 0x99, 0x55, 0x66};
  auto reader = BitstreamReader<kArch>::InitWithBytes(bitstream);
  EXPECT_TRUE(reader);
}

TEST(XC7BitstreamReaderTest, InitWithSyncAfterWordSizedPaddingReturnsObject) {
  std::vector<uint8_t> const bitstream{0xFF, 0xFE, 0xFD, 0xFC,
                                       0xAA, 0x99, 0x55, 0x66};
  auto reader = BitstreamReader<kArch>::InitWithBytes(bitstream);
  EXPECT_TRUE(reader);
}

TEST(XC7BitstreamReaderTest, ParsesType1Packet) {
  std::vector<uint8_t> const bitstream{
    0xAA, 0x99, 0x55, 0x66,  // sync
    0x20, 0x00, 0x00, 0x00,  // NOP
  };
  auto reader = BitstreamReader<kArch>::InitWithBytes(bitstream);
  ASSERT_TRUE(reader);
  // NOLINTBEGIN(bugprone-unchecked-optional-access)
  ASSERT_NE(reader->begin(), reader->end());

  auto first_packet = reader->begin();
  EXPECT_EQ(first_packet->opcode(), ConfigurationPacket::Opcode::kNOP);

  EXPECT_EQ(++first_packet, reader->end());
  // NOLINTEND(bugprone-unchecked-optional-access)
}

TEST(XC7BitstreamReaderTest, ParseType2PacketWithoutType1Fails) {
  std::vector<uint8_t> const bitstream{
    0xAA, 0x99, 0x55, 0x66,  // sync
    0x40, 0x00, 0x00, 0x00,  // Type 2 NOP
  };
  auto reader = BitstreamReader<kArch>::InitWithBytes(bitstream);
  ASSERT_TRUE(reader);
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  EXPECT_EQ(reader->begin(), reader->end());
}

TEST(XC7BitstreamReaderTest, ParsesType2AfterType1Packet) {
  std::vector<uint8_t> const bitstream{
    0xAA, 0x99, 0x55, 0x66,  // sync
    0x28, 0x00, 0x60, 0x00,  // Type 1 Read zero bytes from 6
    0x48, 0x00, 0x00, 0x04,  // Type 2 write of 4 words
    0x1,  0x2,  0x3,  0x4,  0x5, 0x6, 0x7, 0x8,
    0x9,  0xA,  0xB,  0xC,  0xD, 0xE, 0xF, 0x10,
  };
  std::vector<uint32_t> data_words{0x01020304, 0x05060708, 0x090A0B0C,
                                   0x0D0E0F10};

  auto reader = BitstreamReader<kArch>::InitWithBytes(bitstream);
  ASSERT_TRUE(reader);
  // NOLINTBEGIN(bugprone-unchecked-optional-access)
  ASSERT_NE(reader->begin(), reader->end());

  auto first_packet = reader->begin();
  EXPECT_EQ(first_packet->opcode(), ConfigurationPacket::Opcode::kRead);
  EXPECT_EQ(first_packet->address(), ConfigurationRegister::kFDRO);
  EXPECT_EQ(first_packet->data(), absl::Span<uint32_t>());

  auto second_packet = ++first_packet;
  ASSERT_NE(second_packet, reader->end());
  EXPECT_EQ(second_packet->opcode(), ConfigurationPacket::Opcode::kRead);
  EXPECT_EQ(second_packet->address(), ConfigurationRegister::kFDRO);
  EXPECT_EQ(first_packet->data(), absl::Span<uint32_t>(data_words));

  EXPECT_EQ(++first_packet, reader->end());
  // NOLINTEND(bugprone-unchecked-optional-access)
}
}  // namespace
}  // namespace xilinx
}  // namespace fpga
