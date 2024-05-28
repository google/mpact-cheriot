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

#include "cheriot/riscv_cheriot_i_instructions.h"

#include <cstdint>
#include <functional>
#include <ios>
#include <type_traits>

#include "absl/log/log.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_instruction_helpers.h"
#include "mpact/sim/generic/type_helpers.h"
#include "riscv//riscv_state.h"

namespace mpact {
namespace sim {
namespace cheriot {

using EC = ::mpact::sim::riscv::ExceptionCode;

void RiscVIllegalInstruction(const Instruction *inst) {
  auto *state = static_cast<CheriotState *>(inst->state());
  // Get instruction word, as it needs to be used as trap value.
  uint64_t address = inst->address();
  auto db = state->db_factory()->Allocate<uint32_t>(1);
  state->DbgLoadMemory(address, db);
  uint32_t inst_word = db->Get<uint32_t>(0);
  db->DecRef();
  // See if the instruction is interpreted as 32 or 16 bit instruction.
  if ((inst_word & 0b11) != 0b11) inst_word &= 0xffff;
  LOG(INFO) << "RiscVIllegalInstruction: " << std::hex << inst_word;
  state->Trap(/*is_interrupt=*/false, /*trap_value=*/inst_word,
              *EC::kIllegalInstruction,
              /*epc=*/inst->address(), inst);
}

// The following instruction semantic functions implement basic alu operations.
// They are used for both register-register and register-immediate versions of
// the corresponding instructions.

using RegisterType = CheriotRegister;

// Register width integer types. These are preferred to uint32_t, etc, for
// those instructions that operate on the entire instruction width.
using UIntReg =
    typename std::make_unsigned<typename RegisterType::ValueType>::type;
using IntReg = typename std::make_signed<UIntReg>::type;

void RiscVIAdd(const Instruction *instruction) {
  RVCheriotBinaryOp<RegisterType, UIntReg, UIntReg>(
      instruction, [](UIntReg a, UIntReg b) { return a + b; });
}

void RiscVISub(const Instruction *instruction) {
  RVCheriotBinaryOp<RegisterType, UIntReg, UIntReg>(
      instruction, [](UIntReg a, UIntReg b) { return a - b; });
}

void RiscVISlt(const Instruction *instruction) {
  RVCheriotBinaryOp<RegisterType, IntReg, IntReg>(
      instruction, [](IntReg a, IntReg b) { return a < b; });
}

void RiscVISltu(const Instruction *instruction) {
  RVCheriotBinaryOp<RegisterType, UIntReg, UIntReg>(
      instruction, [](UIntReg a, UIntReg b) { return a < b; });
}

void RiscVIAnd(const Instruction *instruction) {
  RVCheriotBinaryOp<RegisterType, UIntReg, UIntReg>(
      instruction, [](UIntReg a, UIntReg b) { return a & b; });
}

void RiscVIOr(const Instruction *instruction) {
  RVCheriotBinaryOp<RegisterType, UIntReg, UIntReg>(
      instruction, [](UIntReg a, UIntReg b) { return a | b; });
}

void RiscVIXor(const Instruction *instruction) {
  RVCheriotBinaryOp<RegisterType, UIntReg, UIntReg>(
      instruction, [](UIntReg a, UIntReg b) { return a ^ b; });
}

void RiscVISll(const Instruction *instruction) {
  RVCheriotBinaryOp<RegisterType, UIntReg, UIntReg>(
      instruction, [](UIntReg a, UIntReg b) { return a << (b & 0x1f); });
}

void RiscVISrl(const Instruction *instruction) {
  RVCheriotBinaryOp<RegisterType, UIntReg, UIntReg>(
      instruction, [](UIntReg a, UIntReg b) { return a >> (b & 0x1f); });
}

void RiscVISra(const Instruction *instruction) {
  RVCheriotBinaryOp<RegisterType, IntReg, IntReg>(
      instruction, [](IntReg a, IntReg b) { return a >> (b & 0x1f); });
}

// Load upper immediate. It is assumed that the decoder already shifted the
// immediate. Operates on 32 bit quantities, not XLEN bits.
void RiscVILui(const Instruction *instruction) {
  RVCheriotUnaryOp<RegisterType, uint32_t, uint32_t>(
      instruction, [](uint32_t lhs) { return lhs & ~0xfff; });
}

// RiscVIJal and RiscVIJalr are superseded by the capability versions CJal and
// CJalr.

void RiscVINop(const Instruction *instruction) {}

// Register width integer types.
using UIntReg =
    typename std::make_unsigned<typename RegisterType::ValueType>::type;
using IntReg = typename std::make_signed<UIntReg>::type;

void RiscVIBeq(const Instruction *instruction) {
  RVCheriotBranchConditional<RegisterType, UIntReg>(
      instruction, [](UIntReg a, UIntReg b) { return a == b; });
}

void RiscVIBne(const Instruction *instruction) {
  RVCheriotBranchConditional<RegisterType, UIntReg>(
      instruction, [](UIntReg a, UIntReg b) { return a != b; });
}

void RiscVIBlt(const Instruction *instruction) {
  RVCheriotBranchConditional<RegisterType, IntReg>(
      instruction, [](IntReg a, IntReg b) { return a < b; });
}

void RiscVIBltu(const Instruction *instruction) {
  RVCheriotBranchConditional<RegisterType, UIntReg>(
      instruction, [](UIntReg a, UIntReg b) { return a < b; });
}

void RiscVIBge(const Instruction *instruction) {
  RVCheriotBranchConditional<RegisterType, IntReg>(
      instruction, [](IntReg a, IntReg b) { return a >= b; });
}

void RiscVIBgeu(const Instruction *instruction) {
  RVCheriotBranchConditional<RegisterType, UIntReg>(
      instruction, [](UIntReg a, UIntReg b) { return a >= b; });
}

void RiscVILd(const Instruction *instruction) {
  RVCheriotLoad<RegisterType, uint64_t>(instruction);
}

void RiscVILw(const Instruction *instruction) {
  RVCheriotLoad<RegisterType, int32_t>(instruction);
}

void RiscVILwChild(const Instruction *instruction) {
  RVCheriotLoadChild<RegisterType, int32_t>(instruction);
}

void RiscVILh(const Instruction *instruction) {
  RVCheriotLoad<RegisterType, int16_t>(instruction);
}

void RiscVILhChild(const Instruction *instruction) {
  RVCheriotLoadChild<RegisterType, int16_t>(instruction);
}

void RiscVILhu(const Instruction *instruction) {
  RVCheriotLoad<RegisterType, uint16_t>(instruction);
}

void RiscVILhuChild(const Instruction *instruction) {
  RVCheriotLoadChild<RegisterType, uint16_t>(instruction);
}

void RiscVILb(const Instruction *instruction) {
  RVCheriotLoad<RegisterType, int8_t>(instruction);
}

void RiscVILbChild(const Instruction *instruction) {
  RVCheriotLoadChild<RegisterType, int8_t>(instruction);
}

void RiscVILbu(const Instruction *instruction) {
  RVCheriotLoad<RegisterType, uint8_t>(instruction);
}

void RiscVILbuChild(const Instruction *instruction) {
  RVCheriotLoadChild<RegisterType, uint8_t>(instruction);
}

void RiscVISd(const Instruction *instruction) {
  RVCheriotStore<RegisterType, uint64_t>(instruction);
}

void RiscVISw(const Instruction *instruction) {
  RVCheriotStore<RegisterType, uint32_t>(instruction);
}

void RiscVISh(const Instruction *instruction) {
  RVCheriotStore<RegisterType, uint16_t>(instruction);
}

void RiscVISb(const Instruction *instruction) {
  RVCheriotStore<RegisterType, uint8_t>(instruction);
}

void RiscVIFence(const Instruction *instruction) {
  uint32_t bits = instruction->Source(0)->AsUint32(0);
  int fm = (bits >> 8) & 0xf;
  int predecessor = (bits >> 4) & 0xf;
  int successor = bits & 0xf;
  auto *state = static_cast<CheriotState *>(instruction->state());
  state->Fence(instruction, fm, predecessor, successor);
}

void RiscVIEcall(const Instruction *instruction) {
  auto *state = static_cast<CheriotState *>(instruction->state());
  state->ECall(instruction);
}

void RiscVIEbreak(const Instruction *instruction) {
  auto *state = static_cast<CheriotState *>(instruction->state());
  state->EBreak(instruction);
}

void RiscVWFI(const Instruction *instruction) {
  auto *state = static_cast<CheriotState *>(instruction->state());
  state->WFI(instruction);
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
