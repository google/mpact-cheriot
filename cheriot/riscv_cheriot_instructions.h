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

#ifndef MPACT_CHERIOT__RISCV_CHERIOT_INSTRUCTIONS_H_
#define MPACT_CHERIOT__RISCV_CHERIOT_INSTRUCTIONS_H_

#include "mpact/sim/generic/instruction.h"

// This file declares the instruction semantic functions for the RiscV CHERIoT
// instructions. These instructions are defined in section 9 in the Microsoft
// Tech Report MSR-TR-2023-6 "CHERIoT: Rethinking security for low-cost embedded
// systems"."

namespace mpact {
namespace sim {
namespace cheriot {

using generic::Instruction;

// This takes 2 source operands and one destination operand. Source 0 is the
// source capability, source 1 is a pre-scaled immediate. Destination 0 is the
// target capability.
void CheriotAuicap(const Instruction *instruction);
// This takes 2 sources and 1 destination operand. Source 0 is the source
// capability, source 1 is an integer register value. Destination 0 is the
// target capability.
void CheriotCAndPerm(const Instruction *instruction);
// This takes 1 source and 1 destination operand. Source 0 is the source
// capability. Destination 0 is the target capability.
void CheriotCClearTag(const Instruction *instruction);
// This takes 1 source and 1 destination operand. Source 0 is the source
// capability. Destination 0 is an integer register.
void CheriotCGetAddr(const Instruction *instruction);
// This takes 1 source and 1 destination operand. Source 0 is the source
// capability. Destination 0 is an integer register.
void CheriotCGetBase(const Instruction *instruction);
// This takes 1 source and 1 destination operand. Source 0 is the source
// capability. Destination 0 is an integer register.
void CheriotCGetHigh(const Instruction *instruction);
// This takes 1 source and 1 destination operand. Source 0 is the source
// capability. Destination 0 is an integer register.
void CheriotCGetLen(const Instruction *instruction);
// This takes 1 source and 1 destination operand. Source 0 is the source
// capability. Destination 0 is an integer register.
void CheriotCGetPerm(const Instruction *instruction);
// This takes 1 source and 1 destination operand. Source 0 is the source
// capability. Destination 0 is an integer register.
void CheriotCGetTag(const Instruction *instruction);
// This takes 1 source and 1 destination operand. Source 0 is the source
// capability. Destination 0 is an integer register.
void CheriotCGetTop(const Instruction *instruction);
// This takes 1 source and 1 destination operand. Source 0 is the source
// capability. Destination 0 is an integer register.
void CheriotCGetType(const Instruction *instruction);
// This takes 2 source operands and 1 destination operand. Source 0 is the
// source capability and source 1 is the integer increment amount. Destination
// 0 is the target capability.
void CheriotCIncAddr(const Instruction *instruction);
// This instruction takes 1 source (an offset), and one destination (the link
// capability). The pcc is implied.
void CheriotCJal(const Instruction *instruction);
// This instruction takes 1 source (an offset). The pcc is implied.
void CheriotCJ(const Instruction *instruction);
// This instruction takes 2 sources and one destination operand. Source 0 is the
// source capability, source 1 is the offset. Destination 0 is the link
// capability. The pcc is implied.
void CheriotCJalr(const Instruction *instruction);
// This instruction takes 2 sources. Source 0 is the source capability, source 1
// is the offset. The pcc is implied.
void CheriotCJr(const Instruction *instruction);
// This instruction takes 2 sources and one destination operand. Source 0 is the
// source capability, source 1 is the offset. Destination 0 is the link
// capability. The pcc is implied.
void CheriotCJalrCra(const Instruction *instruction);
// This instruction takes 2 sources. Source 0 is the source capability, source 1
// is the offset. The pcc is implied.
void CheriotCJrCra(const Instruction *instruction);
// This instruction takes no sources, no destinations.
void CheriotCJalrZero(const Instruction *instruction);
// This takes 2 source operands and 1 destination operand. Source 0 is the
// address source capability, source 1 is the integer offset. Destination 0 is
// the target capability.
void CheriotCLc(const Instruction *instruction);
// Child instruction function for CheriotCLc.
void CheriotCLcChild(const Instruction *instruction);
// This takes 1 source and 1 destination operand. Source 0 is the source
// capability. Destination 0 is the target capability.
void CheriotCMove(const Instruction *instruction);
// This takes 1 source and 1 destination operand. Source 0 is an integer value
// (address of a capability). Destination 0 is an integer register.
void CheriotCRepresentableAlignmentMask(const Instruction *instruction);
// This takes 1 source and 1 destination operand. Source 0 is an integer value
// (address of a capability). Destination 0 is an integer register.
void CheriotCRoundRepresentableLength(const Instruction *instruction);
// This takes 3 source operands. Source 0 is the address source capability,
// source 1 is the integer offset, and source 2 is the source capability that
// is stored to memory.
void CheriotCSc(const Instruction *instruction);
// This takes 2 source operands and 1 destination operand. Source 0 is the
// source capability, source 1 is the sealing authority capability. Destination
// 0 is the target capability.
void CheriotCSeal(const Instruction *instruction);
// This takes 2 source operands and 1 destination operand. Source 0 is the
// source capability, source 1 is an integer value. Destination 0 is the
// target capability.
void CheriotCSetAddr(const Instruction *instruction);
// This takes 2 source operands and 1 destination operand. Source 0 is the
// source capability, source 1 is an integer value. Destination 0 is the
// target capability.
void CheriotCSetBounds(const Instruction *instruction);
// This takes 2 source operands and 1 destination operand. Source 0 is the
// source capability, source 1 is an integer value. Destination 0 is the
// target capability.
void CheriotCSetBoundsExact(const Instruction *instruction);
// This takes 2 source operands and 1 destination operand. Sources 0 and 1
// are capabilities. Destination 0 is an integer register.
void CheriotCSetEqualExact(const Instruction *instruction);
// This takes 2 source operands and 1 destination operand. Source 0 is the
// source capability, source 1 is an integer value. Destination 0 is the
// target capability.
void CheriotCSetHigh(const Instruction *instruction);
// This takes one source and one destination operand. Source 0 is the source
// special capability CSR. Destination 0 is the target capability.
void CheriotCSpecialR(const Instruction *instruction);
// This takes 2 source operands and 1 destination operand. Source 0 is the
// source capability, source 1 is the special capability CSR. Destination 0
// is the target capability.
void CheriotCSpecialRW(const Instruction *instruction);
// This takes 2 source operands and 1 destination operand. Sources 0 and 1
// are capabilities. Destination 0 is an integer register.
void CheriotCSub(const Instruction *instruction);
// This takes 2 source operands and 1 destination operand. Sources 0 and 1
// are capabilities. Destination 0 is an integer register.
void CheriotCTestSubset(const Instruction *instruction);
// This takes 2 source operands and 1 destination operand. Source 0 is the
// source capability, source 1 is the unsealing authority capability.
// Destination 0 is the target capability.
void CheriotCUnseal(const Instruction *instruction);

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__RISCV_CHERIOT_INSTRUCTIONS_H_
