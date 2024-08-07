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

#include "cheriot/riscv_cheriot_rvv_fp_encoding.h"

#include <cstdint>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "cheriot/cheriot_f_getters.h"
#include "cheriot/cheriot_getters.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_rvv_fp_getters.h"
#include "cheriot/cheriot_rvv_getters.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_encoding_common.h"
#include "cheriot/riscv_cheriot_rvv_fp_bin_decoder.h"
#include "cheriot/riscv_cheriot_rvv_fp_decoder.h"
#include "cheriot/riscv_cheriot_rvv_fp_enums.h"
#include "mpact/sim/generic/type_helpers.h"

namespace mpact {
namespace sim {
namespace cheriot {
namespace isa32_rvv_fp {

using Extractors = ::mpact::sim::cheriot::encoding_rvv_fp::Extractors;

RiscVCheriotRVVFPEncoding::RiscVCheriotRVVFPEncoding(CheriotState *state)
    : RiscVCheriotEncodingCommon(state) {
  source_op_getters_.emplace(*SourceOpEnum::kNone, []() { return nullptr; });
  dest_op_getters_.emplace(*DestOpEnum::kNone,
                           [](int latency) { return nullptr; });
  // Add Cheriot ISA source and destination operand getters.
  AddCheriotSourceGetters<SourceOpEnum, Extractors>(source_op_getters_, this);
  AddCheriotDestGetters<DestOpEnum, Extractors>(dest_op_getters_, this);
  // Add RVV source and destination operand getters.
  AddCheriotRVVSourceGetters<SourceOpEnum, Extractors>(source_op_getters_,
                                                       this);
  AddCheriotRVVDestGetters<DestOpEnum, Extractors>(dest_op_getters_, this);
  // Add RVV FP source and destination operand getters.
  AddCheriotRVVFPSourceGetters<SourceOpEnum, Extractors>(source_op_getters_,
                                                         this);
  AddCheriotRVVFPDestGetters<DestOpEnum, Extractors>(dest_op_getters_, this);
  // Add FP source and destination operand getters.
  AddCheriotFSourceGetters<SourceOpEnum, Extractors>(source_op_getters_, this);
  AddCheriotFDestGetters<DestOpEnum, Extractors>(dest_op_getters_, this);
  // Verify that all source and destination op enum values have a getter.
  for (int i = *SourceOpEnum::kNone; i < *SourceOpEnum::kPastMaxValue; ++i) {
    if (source_op_getters_.find(i) == source_op_getters_.end()) {
      LOG(ERROR) << "No getter for source op enum value " << i;
    }
  }
  for (int i = *DestOpEnum::kNone; i < *DestOpEnum::kPastMaxValue; ++i) {
    if (dest_op_getters_.find(i) == dest_op_getters_.end()) {
      LOG(ERROR) << "No getter for destination op enum value " << i;
    }
  }
}

// Parse the instruction word to determine the opcode.
void RiscVCheriotRVVFPEncoding::ParseInstruction(uint32_t inst_word) {
  inst_word_ = inst_word;
  if ((inst_word_ & 0x3) == 3) {
    auto [opcode, format] = mpact::sim::cheriot::encoding_rvv_fp::
        DecodeRiscVCheriotRVVFPInst32WithFormat(inst_word_);
    opcode_ = opcode;
    format_ = format;
    return;
  }

  auto [opcode, format] = mpact::sim::cheriot::encoding_rvv_fp::
      DecodeRiscVCheriotRVVFPInst16WithFormat(
          static_cast<uint16_t>(inst_word_ & 0xffff));
  opcode_ = opcode;
  format_ = format;
}

DestinationOperandInterface *RiscVCheriotRVVFPEncoding::GetDestination(
    SlotEnum, int, OpcodeEnum opcode, DestOpEnum dest_op, int dest_no,
    int latency) {
  int index = static_cast<int>(dest_op);
  auto iter = dest_op_getters_.find(index);
  if (iter == dest_op_getters_.end()) {
    LOG(ERROR) << absl::StrCat("No getter for destination op enum value ",
                               index, " for instruction ",
                               kOpcodeNames[static_cast<int>(opcode)]);
    return nullptr;
  }
  return (iter->second)(latency);
}

SourceOperandInterface *RiscVCheriotRVVFPEncoding::GetSource(
    SlotEnum, int, OpcodeEnum opcode, SourceOpEnum source_op, int source_no) {
  int index = static_cast<int>(source_op);
  auto iter = source_op_getters_.find(index);
  if (iter == source_op_getters_.end()) {
    LOG(ERROR) << absl::StrCat("No getter for source op enum value ", index,
                               " for instruction ",
                               kOpcodeNames[static_cast<int>(opcode)]);
    return nullptr;
  }
  return (iter->second)();
}

}  // namespace isa32_rvv_fp
}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
