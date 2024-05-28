// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cheriot/cheriot_test_rig.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>

#include "absl/log/check.h"
#include "cheriot/test_rig_packets.h"
#include "googlemock/include/gmock/gmock.h"

namespace {

using ::mpact::sim::cheriot::CheriotTestRig;
using ::mpact::sim::cheriot::test_rig::ExecutionPacket;
using ::mpact::sim::cheriot::test_rig::ExecutionPacketExtInteger;
using ::mpact::sim::cheriot::test_rig::ExecutionPacketExtMemAccess;
using ::mpact::sim::cheriot::test_rig::ExecutionPacketV2;
using ::mpact::sim::cheriot::test_rig::InstructionPacket;
using ::mpact::sim::cheriot::test_rig::kInstruction;
using ::mpact::sim::cheriot::test_rig::kIntegerData;
using ::mpact::sim::cheriot::test_rig::kMachineMode;
using ::mpact::sim::cheriot::test_rig::kMemoryAccess;
using ::mpact::sim::cheriot::test_rig::kXL32;

// Test instructions encodings.
constexpr uint32_t kAddi = 0b000000000000'00000'000'00000'0010011;
constexpr uint32_t kBeq = 0b0000000'00000'00000'000'00000'1100011;
constexpr uint32_t kLui = 0b00000000000000000000'00000'0110111;
constexpr uint32_t kLbu = 0b000000000000'00000'100'00000'0000011;
constexpr uint32_t kLhu = 0b000000000000'00000'101'00000'0000011;
constexpr uint32_t kLw = 0b000000000000'00000'010'00000'0000011;
constexpr uint32_t kSb = 0b0000000'00000'00000'000'00000'0100011;
constexpr uint32_t kSh = 0b0000000'00000'00000'001'00000'0100011;
constexpr uint32_t kSw = 0b0000000'00000'00000'010'00000'0100011;
constexpr uint32_t kCSpecialRW = 0b0000001'00000'00000'000'00000'1011011;
constexpr uint32_t kCSetAddr = 0b0010000'00000'00000'000'00000'1011011;

constexpr uint32_t kMemAddr = 0x8000'2468;
constexpr uint32_t kMtdc = 29;

// Set register operands in 32 bit format instructions.
static uint32_t SetRd(uint32_t iword, uint32_t rdval) {
  return (iword | ((rdval & 0x1f) << 7));
}

static uint32_t SetRs1(uint32_t iword, uint32_t rsval) {
  return (iword | ((rsval & 0x1f) << 15));
}

static uint32_t SetRs2(uint32_t iword, uint32_t rsval) {
  return (iword | ((rsval & 0x1f) << 20));
}

// Set immediate operand for I type.
static uint32_t SetITypeImm(uint32_t iword, uint32_t val) {
  return (iword | ((val & 0xfff) << 20));
}

static uint32_t SetUTypeImm(uint32_t iword, uint32_t val) {
  // Use the upper 20 bits of val.
  return (iword | (val & 0xffff'f000));
}

static uint32_t SetSTypeImm(uint32_t iword, uint32_t val) {
  uint32_t low5 = val & 0x1f;
  uint32_t high7 = (val >> 5) & 0x7f;
  return (iword | (high7 << 25) | (low5 << 7));
}

static uint32_t SetBTypeImm(uint32_t iword, uint32_t val) {
  // signed b_imm[13] = imm7[6], imm5[0], imm7[5..0], imm5[4..1], 0b0;
  uint32_t imm5 = val & 0x1e;
  imm5 |= (val >> (1 + 4 + 6)) & 0x1;
  uint32_t imm7 = (val >> (4 + 1)) & 0x3f;
  imm7 |= (val >> (1 + 6 + 4 + 1)) & 0x1;
  return (iword | (imm7 << 25) | (imm5 << 7));
}

static uint32_t SetRTypeInstruction(uint32_t op, uint32_t rd, uint32_t rs1,
                                    uint32_t rs2) {
  return SetRd(SetRs1(SetRs2(op, rs2), rs1), rd);
}

static uint32_t SetSTypeInstruction(uint32_t op, uint32_t rs1, uint32_t rs2,
                                    uint32_t imm) {
  return SetRs1(SetRs2(SetSTypeImm(op, imm), rs2), rs1);
}

static uint32_t SetITypeInstruction(uint32_t op, uint32_t rd, uint32_t rs1,
                                    uint32_t imm) {
  return SetRd(SetRs1(SetITypeImm(op, imm), rs1), rd);
}

static uint32_t SetUTypeInstruction(uint32_t op, uint32_t rd, uint32_t imm) {
  return SetRd(SetUTypeImm(op, imm), rd);
}

static uint32_t SetBTypeInstruction(uint32_t op, uint32_t rs1, uint32_t rs2,
                                    uint32_t imm) {
  return SetRs1(SetRs2(SetBTypeImm(op, imm), rs2), rs1);
}

class CheriotTestRigTest : public ::testing::Test {
 protected:
  CheriotTestRigTest() {
    int pipe_fds[2];
    CHECK_EQ(pipe2(pipe_fds, O_CLOEXEC), 0);
    read_fd_ = pipe_fds[0];
    write_fd_ = pipe_fds[1];
  }

  ~CheriotTestRigTest() override {
    CHECK_EQ(close(read_fd_), 0);
    CHECK_EQ(close(write_fd_), 0);
  }

  CheriotTestRig test_rig_;
  int read_fd_;
  int write_fd_;
};

TEST_F(CheriotTestRigTest, LinearInstructionSequence) {  // NOLINT
  // Load immediate 0x80002468 into x12
  // Load immediate 0xdeadbeef into x11
  // Move data root capability to x10.
  // Set x10 address to x12
  // Store x11 as byte, half and word to memory.
  // Load x12 as byte, half and word from memory.
  CHECK_OK(test_rig_.SetVersion(1));
  InstructionPacket i_packet;
  ExecutionPacket e_packet;
  uint16_t time = 0;
  uint64_t inst_count = 0;
  // Initial pc value;
  uint64_t pc = 0x8000'0000;
  // lui x10, 0x80002
  uint32_t insn = SetUTypeInstruction(kLui, /*rd=*/12, /*imm=*/kMemAddr);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  int res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_intr, 0);
  EXPECT_EQ(e_packet.rvfi_halt, 0);
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 12);
  EXPECT_EQ(e_packet.rvfi_mem_wmask, 0);
  EXPECT_EQ(e_packet.rvfi_mem_rmask, 0);
  EXPECT_EQ(e_packet.rvfi_rd_wdata, kMemAddr & 0xffff'f000);
  EXPECT_EQ(e_packet.rvfi_insn, insn);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);

  // addi x12, x12, 0x468
  pc += sizeof(uint32_t);
  insn = SetITypeInstruction(kAddi, /*rd=*/12, /*rs1=*/12, /*imm=*/kMemAddr);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_intr, 0);
  EXPECT_EQ(e_packet.rvfi_halt, 0);
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 12);
  EXPECT_EQ(e_packet.rvfi_mem_wmask, 0);
  EXPECT_EQ(e_packet.rvfi_mem_rmask, 0);
  EXPECT_EQ(e_packet.rvfi_rd_wdata, kMemAddr);
  EXPECT_EQ(e_packet.rvfi_insn, insn);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);

  // lui x11, 0xdeadb
  pc += sizeof(uint32_t);
  insn = SetUTypeInstruction(kLui, /*rd=*/11, /*imm=*/0xdead'ceef);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 11);
  // Notice, since the following addi becomes a negative number when sign
  // extended, we load 0xdeadc in the upper 20 bits, so that then addi
  // 'subtracts' eef, we get the right result.
  EXPECT_EQ(e_packet.rvfi_rd_wdata, 0xdeadc000);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);

  // Addi x11, 0xeef
  pc += sizeof(uint32_t);
  insn = SetITypeInstruction(kAddi, /*rd=*/11, /*rs1=*/11, /*imm=*/0xdeadbeef);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 11);
  EXPECT_EQ(e_packet.rvfi_rd_wdata, 0xdeadbeef);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);

  // Move data root capability to x10.
  // CSpecialRW c10, mtdc, c0
  pc += sizeof(uint32_t);
  insn = SetRTypeInstruction(kCSpecialRW, /*rd=*/10, /*rs1=*/0, /*rs2=*/kMtdc);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 10);
  EXPECT_EQ(e_packet.rvfi_rd_wdata, 0);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);

  // Set the address of c10.
  // CSetAddr c10, c10, x12
  pc += sizeof(uint32_t);
  insn = SetRTypeInstruction(kCSetAddr, /*rd=*/10, /*rs1=*/10, /*rs2=*/12);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 10);
  EXPECT_EQ(e_packet.rvfi_rd_wdata, kMemAddr);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);

  // Store values to memory.
  // sb x11, 8(x10)  (stores to 0x8000'2470)
  pc += sizeof(uint32_t);
  insn = SetSTypeInstruction(kSb, /*rs1=*/10, /*rs2=*/11, /*imm=*/8);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 0);
  EXPECT_EQ(e_packet.rvfi_mem_rmask, 0);
  EXPECT_EQ(e_packet.rvfi_mem_wmask, 0x1);
  EXPECT_EQ(e_packet.rvfi_mem_wdata, 0xdeadbeef & 0xff);
  EXPECT_EQ(e_packet.rvfi_mem_addr, kMemAddr + 8);
  EXPECT_EQ(e_packet.rvfi_rd_wdata, 0);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);
  // sh x11, 12(x10) (stores to 0x8000'2474)
  pc += sizeof(uint32_t);
  insn = SetSTypeInstruction(kSh, /*rs1=*/10, /*rs2=*/11, /*imm=*/12);
  i_packet = {insn, time++, kInstruction, /*padding=*/0};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 0);
  EXPECT_EQ(e_packet.rvfi_mem_rmask, 0);
  EXPECT_EQ(e_packet.rvfi_mem_wmask, 0x3);
  EXPECT_EQ(e_packet.rvfi_mem_wdata, 0xdeadbeef & 0xffff);
  EXPECT_EQ(e_packet.rvfi_mem_addr, kMemAddr + 12);
  EXPECT_EQ(e_packet.rvfi_rd_wdata, 0);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);
  // sw x11, 0(x10) (stores to 0x8000'2468)
  pc += sizeof(uint32_t);
  insn = SetSTypeInstruction(kSw, /*rs1=*/10, /*rs2=*/11, /*imm=*/0);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 0);
  EXPECT_EQ(e_packet.rvfi_mem_rmask, 0);
  EXPECT_EQ(e_packet.rvfi_mem_wmask, 0xf);
  EXPECT_EQ(e_packet.rvfi_mem_wdata, 0xdeadbeef);
  EXPECT_EQ(e_packet.rvfi_mem_addr, kMemAddr);
  EXPECT_EQ(e_packet.rvfi_rd_wdata, 0);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);

  // Now load the values from memory and verify.
  // lw x13, 0(x10)
  pc += sizeof(uint32_t);
  insn = SetITypeInstruction(kLw, /*rd=*/13, /*rs1=*/10, /*imm=*/0);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 13);
  EXPECT_EQ(e_packet.rvfi_mem_rmask, 0xf);
  EXPECT_EQ(e_packet.rvfi_mem_wmask, 0);
  EXPECT_EQ(e_packet.rvfi_mem_rdata, 0xdeadbeef);
  EXPECT_EQ(e_packet.rvfi_mem_addr, kMemAddr);
  EXPECT_EQ(e_packet.rvfi_rd_wdata, 0xdeadbeef);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);
  // lh x14, 12(x10)
  pc += sizeof(uint32_t);
  insn = SetITypeInstruction(kLhu, /*rd=*/14, /*rs1=*/10, /*imm=*/12);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 14);
  EXPECT_EQ(e_packet.rvfi_mem_rmask, 0x3);
  EXPECT_EQ(e_packet.rvfi_mem_wmask, 0);
  EXPECT_EQ(e_packet.rvfi_mem_rdata, 0xdeadbeef & 0xffff);
  EXPECT_EQ(e_packet.rvfi_mem_addr, kMemAddr + 12);
  EXPECT_EQ(e_packet.rvfi_rd_wdata, 0xdeadbeef & 0xffff);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);
  // lb x15, 8(x10)
  pc += sizeof(uint32_t);
  insn = SetITypeInstruction(kLbu, /*rd=*/15, /*rs1=*/10, /*imm=*/8);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 15);
  EXPECT_EQ(e_packet.rvfi_mem_rmask, 0x1);
  EXPECT_EQ(e_packet.rvfi_mem_wmask, 0);
  EXPECT_EQ(e_packet.rvfi_mem_rdata, 0xdeadbeef & 0xff);
  EXPECT_EQ(e_packet.rvfi_mem_addr, kMemAddr + 8);
  EXPECT_EQ(e_packet.rvfi_rd_wdata, 0xdeadbeef & 0xff);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);
}

// Test that a branch instruction returns the correct pc_wdata value.
TEST_F(CheriotTestRigTest, Branch) {
  CHECK_OK(test_rig_.SetVersion(1));
  CheriotTestRig test_rig;
  InstructionPacket i_packet;
  ExecutionPacket e_packet;
  uint16_t time = 0;
  uint64_t inst_count = 0;
  // Initial pc value;
  uint64_t pc = 0x8000'0000;
  uint32_t insn =
      SetBTypeInstruction(kBeq, /*rs1=*/1, /*rs2=*/2, /*imm*/ 0x124);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  auto res = read(read_fd_, &e_packet, sizeof(e_packet));
  EXPECT_EQ(res, sizeof(e_packet));
  EXPECT_EQ(e_packet.rvfi_trap, 0);
  EXPECT_EQ(e_packet.rvfi_rd_addr, 0);
  EXPECT_EQ(e_packet.rvfi_pc_wdata, pc + 0x124);
  EXPECT_EQ(e_packet.rvfi_pc_rdata, pc);
  EXPECT_EQ(e_packet.rvfi_order, inst_count);
}

TEST_F(CheriotTestRigTest, LinearInstructionSequenceV2) {  // NOLINT
  // Load immediate 0x80002468 into x12
  // Load immediate 0xdeadbeef into x11
  // Move data root capability to x10.
  // Set x10 address to x12
  // Store x11 as byte, half and word to memory.
  // Load x12 as byte, half and word from memory.
  CHECK_OK(test_rig_.SetVersion(2));
  InstructionPacket i_packet;
  ExecutionPacketExtInteger ep_ext_integer;
  ExecutionPacketExtMemAccess ep_ext_mem_access;
  ExecutionPacketV2 ep_v2_packet;
  uint16_t time = 0;
  uint64_t inst_count = 0;
  // Initial pc value;
  uint64_t pc = 0x8000'0000;
  // lui x10, 0x80002
  uint32_t insn = SetUTypeInstruction(kLui, /*rd=*/12, /*imm=*/kMemAddr);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  int res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data, but not memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, kIntegerData);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, 0);
  res = read(read_fd_, &ep_ext_integer, sizeof(ep_ext_integer));
  EXPECT_EQ(res, sizeof(ep_ext_integer));
  EXPECT_EQ(ep_ext_integer.rvfi_rd_addr, 12);

  // addi x12, x12, 0x468
  pc += sizeof(uint32_t);
  insn = SetITypeInstruction(kAddi, /*rd=*/12, /*rs1=*/12, /*imm=*/kMemAddr);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data, but not memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, kIntegerData);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, 0);
  res = read(read_fd_, &ep_ext_integer, sizeof(ep_ext_integer));
  EXPECT_EQ(res, sizeof(ep_ext_integer));
  EXPECT_EQ(ep_ext_integer.rvfi_rd_addr, 12);

  // lui x11, 0xdeadb
  pc += sizeof(uint32_t);
  insn = SetUTypeInstruction(kLui, /*rd=*/11, /*imm=*/0xdead'ceef);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data, but not memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, kIntegerData);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, 0);
  res = read(read_fd_, &ep_ext_integer, sizeof(ep_ext_integer));
  EXPECT_EQ(res, sizeof(ep_ext_integer));
  EXPECT_EQ(ep_ext_integer.rvfi_rd_addr, 11);
  EXPECT_EQ(ep_ext_integer.rvfi_rd_wdata, 0xdeadc000);

  // Addi x11, 0xeef
  pc += sizeof(uint32_t);
  insn = SetITypeInstruction(kAddi, /*rd=*/11, /*rs1=*/11, /*imm=*/0xdeadbeef);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data, but not memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, kIntegerData);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, 0);
  res = read(read_fd_, &ep_ext_integer, sizeof(ep_ext_integer));
  EXPECT_EQ(res, sizeof(ep_ext_integer));
  EXPECT_EQ(ep_ext_integer.rvfi_rd_addr, 11);
  EXPECT_EQ(ep_ext_integer.rvfi_rd_wdata, 0xdeadbeef);

  // Move data root capability to x10.
  // CSpecialRW c10, mtdc, c0
  pc += sizeof(uint32_t);
  insn = SetRTypeInstruction(kCSpecialRW, /*rd=*/10, /*rs1=*/0, /*rs2=*/kMtdc);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data, but not memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, kIntegerData);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, 0);
  res = read(read_fd_, &ep_ext_integer, sizeof(ep_ext_integer));
  EXPECT_EQ(res, sizeof(ep_ext_integer));
  EXPECT_EQ(ep_ext_integer.rvfi_rd_addr, 10);

  // Set the address of c10.
  // CSetAddr c10, c10, x12
  pc += sizeof(uint32_t);
  insn = SetRTypeInstruction(kCSetAddr, /*rd=*/10, /*rs1=*/10, /*rs2=*/12);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data, but not memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, kIntegerData);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, 0);
  res = read(read_fd_, &ep_ext_integer, sizeof(ep_ext_integer));
  EXPECT_EQ(res, sizeof(ep_ext_integer));
  EXPECT_EQ(ep_ext_integer.rvfi_rd_addr, 10);
  EXPECT_EQ(ep_ext_integer.rvfi_rd_wdata, kMemAddr);

  // Store values to memory.
  // sb x11, 8(x10)  (stores to 0x8000'2470)
  pc += sizeof(uint32_t);
  insn = SetSTypeInstruction(kSb, /*rs1=*/10, /*rs2=*/11, /*imm=*/8);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data, but not memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, 0);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, kMemoryAccess);
  res = read(read_fd_, &ep_ext_mem_access, sizeof(ep_ext_mem_access));
  EXPECT_EQ(res, sizeof(ep_ext_mem_access));
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_rmask, 0);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_wmask, 0x1);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_wdata[0], 0xdeadbeef & 0xff);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_addr, kMemAddr + 8);

  // sh x11, 12(x10) (stores to 0x8000'2474)
  pc += sizeof(uint32_t);
  insn = SetSTypeInstruction(kSh, /*rs1=*/10, /*rs2=*/11, /*imm=*/12);
  i_packet = {insn, time++, kInstruction, /*padding=*/0};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data, but not memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, 0);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, kMemoryAccess);
  res = read(read_fd_, &ep_ext_mem_access, sizeof(ep_ext_mem_access));
  EXPECT_EQ(res, sizeof(ep_ext_mem_access));
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_rmask, 0);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_wmask, 0x3);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_wdata[0], 0xdeadbeef & 0xffff);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_addr, kMemAddr + 12);

  // sw x11, 0(x10) (stores to 0x8000'2468)
  pc += sizeof(uint32_t);
  insn = SetSTypeInstruction(kSw, /*rs1=*/10, /*rs2=*/11, /*imm=*/0);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data, but not memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, 0);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, kMemoryAccess);
  res = read(read_fd_, &ep_ext_mem_access, sizeof(ep_ext_mem_access));
  EXPECT_EQ(res, sizeof(ep_ext_mem_access));
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_rmask, 0);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_wmask, 0xf);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_wdata[0], 0xdeadbeef);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_addr, kMemAddr);

  // Now load the values from memory and verify.
  // lw x13, 0(x10)
  pc += sizeof(uint32_t);
  insn = SetITypeInstruction(kLw, /*rd=*/13, /*rs1=*/10, /*imm=*/0);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data and memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, kIntegerData);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, kMemoryAccess);
  res = read(read_fd_, &ep_ext_integer, sizeof(ep_ext_integer));
  EXPECT_EQ(res, sizeof(ep_ext_integer));
  EXPECT_EQ(ep_ext_integer.rvfi_rd_addr, 13);
  EXPECT_EQ(ep_ext_integer.rvfi_rd_wdata, 0xdeadbeef);
  res = read(read_fd_, &ep_ext_mem_access, sizeof(ep_ext_mem_access));
  EXPECT_EQ(res, sizeof(ep_ext_mem_access));
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_rmask, 0xf);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_wmask, 0);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_rdata[0], 0xdeadbeef);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_addr, kMemAddr);

  // lh x14, 12(x10)
  pc += sizeof(uint32_t);
  insn = SetITypeInstruction(kLhu, /*rd=*/14, /*rs1=*/10, /*imm=*/12);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data and memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, kIntegerData);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, kMemoryAccess);
  res = read(read_fd_, &ep_ext_integer, sizeof(ep_ext_integer));
  EXPECT_EQ(res, sizeof(ep_ext_integer));
  EXPECT_EQ(ep_ext_integer.rvfi_rd_addr, 14);
  EXPECT_EQ(ep_ext_integer.rvfi_rd_wdata, 0xdeadbeef & 0xffff);
  res = read(read_fd_, &ep_ext_mem_access, sizeof(ep_ext_mem_access));
  EXPECT_EQ(res, sizeof(ep_ext_mem_access));
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_rmask, 0x3);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_wmask, 0);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_rdata[0], 0xdeadbeef & 0xffff);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_addr, kMemAddr + 12);

  // lb x15, 8(x10)
  pc += sizeof(uint32_t);
  insn = SetITypeInstruction(kLbu, /*rd=*/15, /*rs1=*/10, /*imm=*/8);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + sizeof(uint32_t));
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data and memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, kIntegerData);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, kMemoryAccess);
  res = read(read_fd_, &ep_ext_integer, sizeof(ep_ext_integer));
  EXPECT_EQ(res, sizeof(ep_ext_integer));
  EXPECT_EQ(ep_ext_integer.rvfi_rd_addr, 15);
  EXPECT_EQ(ep_ext_integer.rvfi_rd_wdata, 0xdeadbeef & 0xff);
  res = read(read_fd_, &ep_ext_mem_access, sizeof(ep_ext_mem_access));
  EXPECT_EQ(res, sizeof(ep_ext_mem_access));
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_rmask, 0x1);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_wmask, 0);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_rdata[0], 0xdeadbeef & 0xff);
  EXPECT_EQ(ep_ext_mem_access.rvfi_mem_addr, kMemAddr + 8);
}

// Test that a branch instruction returns the correct pc_wdata value.
TEST_F(CheriotTestRigTest, BranchV2) {
  CHECK_OK(test_rig_.SetVersion(2));
  CheriotTestRig test_rig;
  InstructionPacket i_packet;
  ExecutionPacketV2 ep_v2_packet;
  uint16_t time = 0;
  uint64_t inst_count = 0;
  // Initial pc value;
  uint64_t pc = 0x8000'0000;
  uint32_t insn =
      SetBTypeInstruction(kBeq, /*rs1=*/1, /*rs2=*/2, /*imm*/ 0x124);
  i_packet = {insn, time++, kInstruction, /*padding=*/'\0'};
  CHECK_OK(test_rig_.Execute(i_packet, write_fd_));
  inst_count++;
  auto res = read(read_fd_, &ep_v2_packet, sizeof(ep_v2_packet));
  EXPECT_EQ(res, sizeof(ep_v2_packet));
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_intr, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_halt, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_trap, 0);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_insn, insn);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_mode, kMachineMode);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_ixl, kXL32);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_valid, 1);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_wdata, pc + 0x124);
  EXPECT_EQ(ep_v2_packet.pc_data.rvfi_pc_rdata, pc);
  EXPECT_EQ(ep_v2_packet.basic_data.rvfi_order, inst_count);
  // This packet should have integer data, but not memory access data.
  EXPECT_EQ(ep_v2_packet.available_fields & kIntegerData, 0);
  EXPECT_EQ(ep_v2_packet.available_fields & kMemoryAccess, 0);
}

}  // namespace
