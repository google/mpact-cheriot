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

#ifndef MPACT_CHERIOT_CHERIOT_F_GETTERS_H_
#define MPACT_CHERIOT_CHERIOT_F_GETTERS_H_

#include <cstdint>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/str_cat.h"
#include "cheriot/cheriot_getter_helpers.h"
#include "cheriot/riscv_cheriot_encoding_common.h"
#include "mpact/sim/generic/immediate_operand.h"
#include "mpact/sim/generic/literal_operand.h"
#include "mpact/sim/generic/operand_interface.h"
#include "riscv//riscv_register.h"
#include "riscv//riscv_state.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::cheriot::RiscVCheriotEncodingCommon;
using ::mpact::sim::generic::DestinationOperandInterface;
using ::mpact::sim::generic::ImmediateOperand;
using ::mpact::sim::generic::IntLiteralOperand;
using ::mpact::sim::generic::SourceOperandInterface;
using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RVFpRegister;

using SourceOpGetterMap =
    absl::flat_hash_map<int, absl::AnyInvocable<SourceOperandInterface *()>>;
using DestOpGetterMap =
    absl::flat_hash_map<int,
                        absl::AnyInvocable<DestinationOperandInterface *(int)>>;

template <typename Enum, typename Extractors>
void AddCheriotFSourceGetters(SourceOpGetterMap &getter_map,
                              RiscVCheriotEncodingCommon *common) {
  Insert(getter_map, *Enum::kFrs1, [common]() {
    int num = Extractors::RType::ExtractRs1(common->inst_word());
    return GetRegisterSourceOp<RVFpRegister>(
        common->state(), absl::StrCat(RiscVState::kFregPrefix, num));
  });
  Insert(getter_map, *Enum::kFrs2, [common]() {
    int num = Extractors::RType::ExtractRs2(common->inst_word());
    return GetRegisterSourceOp<RVFpRegister>(
        common->state(), absl::StrCat(RiscVState::kFregPrefix, num));
  });
  Insert(getter_map, *Enum::kFrs3, [common]() {
    int num = Extractors::R4Type::ExtractRs3(common->inst_word());
    return GetRegisterSourceOp<RVFpRegister>(
        common->state(), absl::StrCat(RiscVState::kFregPrefix, num));
  });
  Insert(getter_map, *Enum::kFs1, [common]() {
    int num = Extractors::RType::ExtractRs1(common->inst_word());
    return GetRegisterSourceOp<RVFpRegister>(
        common->state(), absl::StrCat(RiscVState::kFregPrefix, num));
  });
  Insert(getter_map, *Enum::kRm, [common]() -> SourceOperandInterface * {
    uint32_t rm = (common->inst_word() >> 12) & 0x7;
    switch (rm) {
      case 0:
        return new generic::IntLiteralOperand<0>();
      case 1:
        return new generic::IntLiteralOperand<1>();
      case 2:
        return new generic::IntLiteralOperand<2>();
      case 3:
        return new generic::IntLiteralOperand<3>();
      case 4:
        return new generic::IntLiteralOperand<4>();
      case 5:
        return new generic::IntLiteralOperand<5>();
      case 6:
        return new generic::IntLiteralOperand<6>();
      case 7:
        return new generic::IntLiteralOperand<7>();
      default:
        return nullptr;
    }
  });
}

template <typename Enum, typename Extractors>
void AddCheriotFDestGetters(DestOpGetterMap &getter_map,
                            RiscVCheriotEncodingCommon *common) {
  Insert(getter_map, *Enum::kFrd, [common](int latency) {
    int num = Extractors::RType::ExtractRd(common->inst_word());
    return GetRegisterDestinationOp<RVFpRegister>(
        common->state(), absl::StrCat(RiscVState::kFregPrefix, num), latency);
  });
  Insert(getter_map, *Enum::kFflags, [common](int latency) {
    return GetCSRSetBitsDestinationOp<uint32_t>(common->state(), "fflags",
                                                latency, "");
  });
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT_CHERIOT_F_GETTERS_H_
