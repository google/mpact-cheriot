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

#ifndef MPACT_CHERIOT__RISCV_CHERIOT_INSTRUCTION_HELPERS_H_
#define MPACT_CHERIOT__RISCV_CHERIOT_INSTRUCTION_HELPERS_H_

#include <any>
#include <cstdint>
#include <functional>
#include <limits>
#include <tuple>
#include <type_traits>

#include "absl/log/log.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "mpact/sim/generic/arch_state.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/operand_interface.h"
#include "mpact/sim/generic/register.h"
#include "mpact/sim/generic/type_helpers.h"
#include "riscv//riscv_state.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::Instruction;
using ::mpact::sim::generic::operator*;
using ::mpact::sim::generic::RegisterBase;
using CapReg = CheriotRegister;
using PB = ::mpact::sim::cheriot::CheriotRegister::PermissionBits;

// Get the destination capability register.
static inline CapReg *GetCapDest(const Instruction *instruction, int i) {
  return static_cast<CapReg *>(
      std::any_cast<RegisterBase *>(instruction->Destination(i)->GetObject()));
}

// Writing an integer result requires invalidating the capability and setting
// it to null.
template <typename Result>
static inline void WriteCapIntResult(const Instruction *instruction, int i,
                                     Result value) {
  auto *cap_reg = GetCapDest(instruction, i);
  cap_reg->data_buffer()->Set<Result>(0, value);
  cap_reg->Invalidate();
  cap_reg->set_is_null();
}

template <typename Register, typename Result, typename Argument>
inline void RiscVBinaryOp(const Instruction *instruction,
                          std::function<Result(Argument, Argument)> operation) {
  using RegValue = typename Register::ValueType;
  Argument lhs = generic::GetInstructionSource<Argument>(instruction, 0);
  Argument rhs = generic::GetInstructionSource<Argument>(instruction, 1);
  Result dest_value = operation(lhs, rhs);
  auto *reg = static_cast<generic::RegisterDestinationOperand<RegValue> *>(
                  instruction->Destination(0))
                  ->GetRegister();
  reg->data_buffer()->template Set<Result>(0, dest_value);
}

// Generic helper functions for binary instructions. Clears the tag bit for
// the capability register and sets it to null.
template <typename Register, typename Result, typename Argument1,
          typename Argument2>
inline void RVCheriotBinaryOp(
    const Instruction *instruction,
    std::function<Result(Argument1, Argument2)> operation) {
  Argument1 lhs = generic::GetInstructionSource<Argument1>(instruction, 0);
  Argument2 rhs = generic::GetInstructionSource<Argument2>(instruction, 1);
  Result dest_value = operation(lhs, rhs);
  WriteCapIntResult(instruction, 0, dest_value);
}

template <typename Register, typename Result, typename Argument>
inline void RVCheriotBinaryOp(
    const Instruction *instruction,
    std::function<Result(Argument, Argument)> operation) {
  Argument lhs = generic::GetInstructionSource<Argument>(instruction, 0);
  Argument rhs = generic::GetInstructionSource<Argument>(instruction, 1);
  Result dest_value = operation(lhs, rhs);
  WriteCapIntResult(instruction, 0, dest_value);
}

template <typename Register, typename Result>
inline void RVCheriotBinaryOp(const Instruction *instruction,
                              std::function<Result(Result, Result)> operation) {
  Result lhs = generic::GetInstructionSource<Result>(instruction, 0);
  Result rhs = generic::GetInstructionSource<Result>(instruction, 1);
  Result dest_value = operation(lhs, rhs);
  WriteCapIntResult(instruction, 0, dest_value);
}

// Generic helper function for unary instructions.
template <typename Register, typename Result, typename Argument>
inline void RVCheriotUnaryOp(const Instruction *instruction,
                             std::function<Result(Argument)> operation) {
  auto lhs = generic::GetInstructionSource<Argument>(instruction, 0);
  Result dest_value = operation(lhs);
  WriteCapIntResult(instruction, 0, dest_value);
}

// Helper function for conditional branches.
template <typename RegisterType, typename ValueType>
static inline void RVCheriotBranchConditional(
    const Instruction *instruction,
    std::function<bool(ValueType, ValueType)> cond) {
  using UIntType =
      typename std::make_unsigned<typename RegisterType::ValueType>::type;
  ValueType a = generic::GetInstructionSource<ValueType>(instruction, 0);
  ValueType b = generic::GetInstructionSource<ValueType>(instruction, 1);
  if (cond(a, b)) {
    UIntType offset = generic::GetInstructionSource<UIntType>(instruction, 2);
    UIntType target = offset + instruction->address();
    auto *state = static_cast<CheriotState *>(instruction->state());
    auto *pcc = state->pcc();
    if (!pcc->HasPermission(PB::kPermitExecute)) {
      state->HandleCheriRegException(
          instruction, pcc->address(),
          ExceptionCode::kCapExPermitExecuteViolation, pcc);
      return;
    }
    pcc->set_address(target);
    state->set_branch(true);
  }
}

// Generic helper function for load instructions.
template <typename Register, typename ValueType>
inline void RVCheriotLoad(const Instruction *instruction) {
  using RegVal = typename Register::ValueType;
  using URegVal = typename std::make_unsigned<RegVal>::type;
  // Bet the capability.
  auto *cap_reg = static_cast<CheriotRegister *>(
      static_cast<generic::RegisterSourceOperand<RegVal> *>(
          instruction->Source(0))
          ->GetRegister());
  URegVal base = cap_reg->address();
  RegVal offset = generic::GetInstructionSource<RegVal>(instruction, 1);
  URegVal address = base + offset;
  auto *state = static_cast<CheriotState *>(instruction->state());
  // Check for tag unset.
  if (!cap_reg->tag()) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   ExceptionCode::kCapExTagViolation, cap_reg);
    return;
  }
  // Check for sealed.
  if (cap_reg->IsSealed()) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   ExceptionCode::kCapExSealViolation, cap_reg);
    return;
  }
  // Check for permissions.
  if (!cap_reg->HasPermission(CheriotRegister::kPermitLoad)) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   ExceptionCode::kCapExPermitLoadViolation,
                                   cap_reg);
    return;
  }
  // Check for bounds.
  if (!cap_reg->IsInBounds(address, sizeof(ValueType))) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   ExceptionCode::kCapExBoundsViolation,
                                   cap_reg);
    return;
  }
  auto *value_db =
      instruction->state()->db_factory()->Allocate(sizeof(ValueType));
  value_db->set_latency(0);
  auto *context = new riscv::LoadContext(value_db);
  state->LoadMemory(instruction, address, value_db, instruction->child(),
                    context);
  context->DecRef();
}

// Generic helper function for load instructions' "child instruction".
template <typename Register, typename ValueType>
inline void RVCheriotLoadChild(const Instruction *instruction) {
  using RegVal = typename Register::ValueType;
  using URegVal = typename std::make_unsigned<RegVal>::type;
  using SRegVal = typename std::make_signed<URegVal>::type;
  riscv::LoadContext *context =
      static_cast<riscv::LoadContext *>(instruction->context());
  if (std::is_signed<ValueType>::value) {
    SRegVal value = static_cast<SRegVal>(context->value_db->Get<ValueType>(0));
    WriteCapIntResult(instruction, 0, value);
  } else {
    URegVal value = static_cast<URegVal>(context->value_db->Get<ValueType>(0));
    WriteCapIntResult(instruction, 0, value);
  }
}

// Generic helper function for store instructions.
template <typename RegisterType, typename ValueType>
inline void RVCheriotStore(const Instruction *instruction) {
  using URegVal =
      typename std::make_unsigned<typename RegisterType::ValueType>::type;
  using SRegVal = typename std::make_signed<URegVal>::type;
  ValueType value = generic::GetInstructionSource<ValueType>(instruction, 2);
  auto *cap_reg = static_cast<CheriotRegister *>(
      static_cast<
          generic::RegisterSourceOperand<typename RegisterType::ValueType> *>(
          instruction->Source(0))
          ->GetRegister());
  URegVal base = cap_reg->address();
  SRegVal offset = generic::GetInstructionSource<SRegVal>(instruction, 1);
  URegVal address = base + offset;
  auto *state = static_cast<CheriotState *>(instruction->state());
  // Check for tag unset.
  if (!cap_reg->tag()) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   ExceptionCode::kCapExTagViolation, cap_reg);
    return;
  }
  // Check for sealed.
  if (cap_reg->IsSealed()) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   ExceptionCode::kCapExSealViolation, cap_reg);
    return;
  }
  // Check for permissions.
  if (!cap_reg->HasPermission(CheriotRegister::kPermitStore)) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   ExceptionCode::kCapExPermitStoreViolation,
                                   cap_reg);
    return;
  }
  // Check for bounds.
  if (!cap_reg->IsInBounds(address, sizeof(ValueType))) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   ExceptionCode::kCapExBoundsViolation,
                                   cap_reg);
    return;
  }
  auto *db = state->db_factory()->Allocate(sizeof(ValueType));
  db->Set<ValueType>(0, value);
  state->StoreMemory(instruction, address, db);
  db->DecRef();
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__RISCV_CHERIOT_INSTRUCTION_HELPERS_H_
