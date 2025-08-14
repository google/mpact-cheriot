// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MPACT_CHERIOT_CHERIOT_GETTER_HELPERS_H_
#define MPACT_CHERIOT_CHERIOT_GETTER_HELPERS_H_

#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "mpact/sim/generic/operand_interface.h"
#include "mpact/sim/generic/register.h"
#include "riscv//riscv_register.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::DestinationOperandInterface;
using ::mpact::sim::generic::SourceOperandInterface;
using ::mpact::sim::riscv::RV32VectorDestinationOperand;
using ::mpact::sim::riscv::RV32VectorSourceOperand;

constexpr int kNumRegTable[8] = {8, 1, 2, 1, 4, 1, 2, 1};

template <typename M, typename E, typename G>
inline void Insert(M& map, E entry, G getter) {
  if (!map.contains(static_cast<int>(entry))) {
    map.insert(std::make_pair(static_cast<int>(entry), getter));
  } else {
    map.at(static_cast<int>(entry)) = getter;
  }
}

// Generic helper functions to create register operands.
template <typename RegType>
inline DestinationOperandInterface* GetRegisterDestinationOp(
    CheriotState* state, std::string name, int latency) {
  auto* reg = state->GetRegister<RegType>(name).first;
  return reg->CreateDestinationOperand(latency);
}

template <typename RegType>
inline DestinationOperandInterface* GetRegisterDestinationOp(
    CheriotState* state, std::string name, int latency, std::string op_name) {
  auto* reg = state->GetRegister<RegType>(name).first;
  return reg->CreateDestinationOperand(latency, op_name);
}

template <typename T>
inline DestinationOperandInterface* GetCSRSetBitsDestinationOp(
    CheriotState* state, std::string name, int latency, std::string op_name) {
  auto result = state->csr_set()->GetCsr(name);
  if (!result.ok()) {
    LOG(ERROR) << "No such CSR '" << name << "'";
    return nullptr;
  }
  auto* csr = result.value();
  auto* op = csr->CreateSetDestinationOperand(latency, op_name);
  return op;
}

template <typename RegType>
inline SourceOperandInterface* GetRegisterSourceOp(CheriotState* state,
                                                   std::string name) {
  auto* reg = state->GetRegister<RegType>(name).first;
  auto* op = reg->CreateSourceOperand();
  return op;
}

template <typename RegType>
inline SourceOperandInterface* GetRegisterSourceOp(CheriotState* state,
                                                   std::string name,
                                                   std::string op_name) {
  auto* reg = state->GetRegister<RegType>(name).first;
  auto* op = reg->CreateSourceOperand(op_name);
  return op;
}

template <typename RegType>
inline void GetVRegGroup(CheriotState* state, int reg_num,
                         std::vector<generic::RegisterBase*>* vreg_group) {
  // The number of registers in a vector register group depends on the register
  // index: 0, 8, 16, 24 each have 8 registers, 4, 12, 20, 28 each have 4,
  // 2, 6, 10, 14, 18, 22, 26, 30 each have two, and all odd numbered register
  // groups have only 1.
  int num_regs = kNumRegTable[reg_num % 8];
  for (int i = 0; i < num_regs; i++) {
    auto vreg_name = absl::StrCat(CheriotState::kVregPrefix, reg_num + i);
    vreg_group->push_back(state->GetRegister<RegType>(vreg_name).first);
  }
}
template <typename RegType>
inline SourceOperandInterface* GetVectorRegisterSourceOp(CheriotState* state,
                                                         int reg_num) {
  std::vector<generic::RegisterBase*> vreg_group;
  GetVRegGroup<RegType>(state, reg_num, &vreg_group);
  auto* v_src_op = new RV32VectorSourceOperand(
      absl::Span<generic::RegisterBase*>(vreg_group),
      absl::StrCat(CheriotState::kVregPrefix, reg_num));
  return v_src_op;
}

template <typename RegType>
inline DestinationOperandInterface* GetVectorRegisterDestinationOp(
    CheriotState* state, int latency, int reg_num) {
  std::vector<generic::RegisterBase*> vreg_group;
  GetVRegGroup<RegType>(state, reg_num, &vreg_group);
  auto* v_dst_op = new RV32VectorDestinationOperand(
      absl::Span<generic::RegisterBase*>(vreg_group), latency,
      absl::StrCat(CheriotState::kVregPrefix, reg_num));
  return v_dst_op;
}

template <typename RegType>
inline SourceOperandInterface* GetVectorMaskRegisterSourceOp(
    CheriotState* state, int reg_num) {
  // Mask register groups only have a single register.
  std::vector<generic::RegisterBase*> vreg_group;
  vreg_group.push_back(state
                           ->GetRegister<RegType>(
                               absl::StrCat(CheriotState::kVregPrefix, reg_num))
                           .first);
  auto* v_src_op = new RV32VectorSourceOperand(
      absl::Span<generic::RegisterBase*>(vreg_group),
      absl::StrCat(CheriotState::kVregPrefix, reg_num));
  return v_src_op;
}

template <typename RegType>
inline DestinationOperandInterface* GetVectorMaskRegisterDestinationOp(
    CheriotState* state, int latency, int reg_num) {
  // Mask register groups only have a single register.
  std::vector<generic::RegisterBase*> vreg_group;
  vreg_group.push_back(state
                           ->GetRegister<RegType>(
                               absl::StrCat(CheriotState::kVregPrefix, reg_num))
                           .first);
  auto* v_dst_op = new RV32VectorDestinationOperand(
      absl::Span<generic::RegisterBase*>(vreg_group), latency,
      absl::StrCat(CheriotState::kVregPrefix, reg_num));
  return v_dst_op;
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT_CHERIOT_GETTER_HELPERS_H_
