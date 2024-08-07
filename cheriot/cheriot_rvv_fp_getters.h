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

#ifndef MPACT_CHERIOT_CHERIOT_RVV_FP_GETTERS_H_
#define MPACT_CHERIOT_CHERIOT_RVV_FP_GETTERS_H_

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/str_cat.h"
#include "cheriot/cheriot_getter_helpers.h"
#include "cheriot/riscv_cheriot_encoding_common.h"
#include "mpact/sim/generic/operand_interface.h"
#include "riscv//riscv_register.h"
#include "riscv//riscv_state.h"

namespace mpact {
namespace sim {
namespace cheriot {
using ::mpact::sim::cheriot::RiscVCheriotEncodingCommon;
using ::mpact::sim::generic::DestinationOperandInterface;
using ::mpact::sim::generic::SourceOperandInterface;
using ::mpact::sim::riscv::RiscVState;
using ::mpact::sim::riscv::RVFpRegister;

using SourceOpGetterMap =
    absl::flat_hash_map<int, absl::AnyInvocable<SourceOperandInterface *()>>;
using DestOpGetterMap =
    absl::flat_hash_map<int,
                        absl::AnyInvocable<DestinationOperandInterface *(int)>>;

template <typename Enum, typename Extractors>
void AddCheriotRVVFPSourceGetters(SourceOpGetterMap &getter_map,
                                  RiscVCheriotEncodingCommon *common) {
  Insert(getter_map, *Enum::kFs1, [common]() {
    int num = Extractors::VArith::ExtractRs1(common->inst_word());
    return GetRegisterSourceOp<RVFpRegister>(
        common->state(), absl::StrCat(RiscVState::kFregPrefix, num));
  });
}

template <typename Enum, typename Extractors>
void AddCheriotRVVFPDestGetters(DestOpGetterMap &getter_map,
                                RiscVCheriotEncodingCommon *common) {
  Insert(getter_map, *Enum::kFd, [common](int latency) {
    int num = Extractors::VArith::ExtractRd(common->inst_word());
    return GetRegisterDestinationOp<RVFpRegister>(
        common->state(), absl::StrCat(RiscVState::kFregPrefix, num), latency);
  });
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT_CHERIOT_RVV_FP_GETTERS_H_
