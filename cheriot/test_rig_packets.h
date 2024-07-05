/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MPACT_CHERIOT__TEST_RIG_PACKETS_H_
#define MPACT_CHERIOT__TEST_RIG_PACKETS_H_

#include <cstdint>

namespace mpact::sim::cheriot {

// Keep these structs and enum in a separate namespace.
namespace test_rig {

enum TraceCommand : uint8_t {
  kEndOfTrace = 0,
  kInstruction = 1,
  kSetVersion = 0x76,
};

struct VersionPacket {
  char version_text[8];
  uint64_t version;
  VersionPacket() : version_text{'v', 'e', 'r', 's', 'i', 'o', 'n', '='} {}
};

struct InstructionPacket {
  // Instruction word. Sixteen bit instructions are stored in the lower half.
  uint32_t rvfi_insn;
  // Timestamp.
  uint16_t rvfi_time;
  // Trace command. Currently 0 = EndOfTrace, 1 = Instruction.
  TraceCommand rvfi_cmd;
  uint8_t padding;
};

struct ExecutionPacket {
  // Instruction number: minstret value after completion.
  uint64_t rvfi_order;
  // Pc for current instruction.
  uint64_t rvfi_pc_rdata;
  // Pc after instruction (either PC + 4 or jump/trap target).
  uint64_t rvfi_pc_wdata;
  // Instruction word.
  uint64_t rvfi_insn;
  // Read register values.
  uint64_t rvfi_rs1_data;
  uint64_t rvfi_rs2_data;
  // Write register value. Must be 0 if rvfi_rd_addr is 0.
  uint64_t rvfi_rd_wdata;
  // Memory address. Byte address (aligned if define is set). 0 if unused.
  uint64_t rvfi_mem_addr;
  // Read data (from memory).
  uint64_t rvfi_mem_rdata;
  // Write data (to memory).
  uint64_t rvfi_mem_wdata;
  // Read mask: indicates valid bytes read. 0 if unused.
  uint8_t rvfi_mem_rmask;
  // Write mask: indicates valid bytes written. 0 if unused.
  uint8_t rvfi_mem_wmask;
  // Rs1 source register id. Arbitrary when not used.
  uint8_t rvfi_rs1_addr;
  // Rs2 source register id. Arbitrary when not used.
  uint8_t rvfi_rs2_addr;
  // Destination register number - must be 0 if not used.
  uint8_t rvfi_rd_addr;
  // Marks an exception: invalid decode, misaligned access, or jump to
  // misaligned address.
  uint8_t rvfi_trap;
  // Marks the last instruction retired before halting execution.
  uint8_t rvfi_halt;
  // Trap handler indicator. Set for first instruction in a trap handler.
  uint8_t rvfi_intr;
};

// The test rig execution trace version 2 uses the following packets.

enum Mode : uint8_t {
  kUserMode = 0,
  kSupervisorMode = 1,
  kMachineMode = 3,
};

enum ModeXL : uint8_t {
  kXL32 = 1,
  kXL64 = 2,
};

struct ExecutionPacketMetaData {
  // Set to the instruction index. No indices can be used twice and there must
  // be no gaps. Instructions may be retired in a a reordered fashion.
  uint64_t rvfi_order;
  // Instruction word for the retired instruction. Upper bits are 0 for
  // instruction words shorter than 64 bits.
  uint64_t rvfi_insn;
  // Must be set for an instruction that cannot be decoded as a legal
  // instruction. Must also be set for a misaligned memory read or write, or
  // other memory access violations. Must also be set for a jump instruction
  // that jumps to a misaligned location.
  uint8_t rvfi_trap;
  // Set for the last instruction before halting execution.
  uint8_t rvfi_halt;
  // Set for the first instruction in a trap handler.
  uint8_t rvfi_intr;
  // Set to the current privilege level 0=U, 1=S, 2=reserved, 3=M.
  uint8_t rvfi_mode;
  // Set to the value of MXL/SXL/UXL in the current privilege level: 1=32, 2=64.
  uint8_t rvfi_ixl;
  // Should be set to 1.
  uint8_t rvfi_valid;
  // Padding to make the size a multiple of 8 bytes.
  uint8_t rvfi_padding[2];
};

struct ExecutionPacketPC {
  // Pc for current instruction.
  uint64_t rvfi_pc_rdata;
  // Pc after instruction (either PC + 4 or jump/trap target).
  uint64_t rvfi_pc_wdata;
};

struct ExecutionPacketExtInteger {
  // Magic bytes, must be "int-data".
  char magic[8];
  // The value of the x register addressed by rd after execution.
  uint64_t rvfi_rd_wdata;
  // The value of the x register addressed by rs1 before execution. Must be zero
  // when rs1 is zero.
  uint64_t rvfi_rs1_rdata;
  // The value of the x register addressed by rs2 before execution. Must be zero
  // when rs2 is zero.
  uint64_t rvfi_rs2_rdata;
  // The decoded rd register address for the instruction. Must be zero if the
  // instruction does not write to rd.
  uint8_t rvfi_rd_addr;
  // The decoded rs1 register address for the instruction. Must be zero if the
  // instruction does not read rs1.
  uint8_t rvfi_rs1_addr;
  // The decoded rs2 register address for the instruction. Must be zero if the
  // instruction does not read rs2.
  uint8_t rvfi_rs2_addr;
  // Padding to make the size a multiple of 8 bytes.
  uint8_t padding[5];
  ExecutionPacketExtInteger()
      : magic{'i', 'n', 't', '-', 'd', 'a', 't', 'a'}, padding{0, 0, 0, 0, 0} {}
};

struct ExecutionPacketExtMemAccess {
  // Magic bytes, must be "mem-data".
  char magic[8];
  // Data read from memory.
  uint64_t rvfi_mem_rdata[4];
  // Data written to memory.
  uint64_t rvfi_mem_wdata[4];
  // Bitmask for which bytes in rdata are valid.
  uint32_t rvfi_mem_rmask;
  // Bitmask for which bytes in wdata are valid.
  uint32_t rvfi_mem_wmask;
  // Address of the accessed memory location (when either rmask or wmask is
  // non-zero).
  uint64_t rvfi_mem_addr;
  ExecutionPacketExtMemAccess()
      : magic{'m', 'e', 'm', '-', 'd', 'a', 't', 'a'} {}
};

enum AvailableFieldsEnum : uint64_t {
  kIntegerData = 0x1,
  kMemoryAccess = 0x2,
};

struct ExecutionPacketV2 {
  // Magic bytes, must be "trace-v2".
  char magic[8];
  // Size of the trace packet + extensions.
  uint64_t trace_size;
  ExecutionPacketMetaData basic_data;
  ExecutionPacketPC pc_data;
  // Bit mask showing which extension fields will follow this packet.
  uint64_t available_fields;
  ExecutionPacketV2() : magic{'t', 'r', 'a', 'c', 'e', '-', 'v', '2'} {}
};

}  // namespace test_rig

}  // namespace mpact::sim::cheriot

#endif  // MPACT_CHERIOT__TEST_RIG_PACKETS_H_
