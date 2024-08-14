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

#ifndef MPACT_CHERIOT_CHERIOT_RVV_GETTERS_H_
#define MPACT_CHERIOT_CHERIOT_RVV_GETTERS_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "cheriot/cheriot_getter_helpers.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/cheriot_vector_true_operand.h"
#include "cheriot/riscv_cheriot_encoding_common.h"
#include "mpact/sim/generic/immediate_operand.h"
#include "mpact/sim/generic/literal_operand.h"
#include "mpact/sim/generic/operand_interface.h"
#include "riscv//riscv_register.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::cheriot::RiscVCheriotEncodingCommon;
using ::mpact::sim::generic::DestinationOperandInterface;
using ::mpact::sim::generic::ImmediateOperand;
using ::mpact::sim::generic::IntLiteralOperand;
using ::mpact::sim::generic::SourceOperandInterface;
using ::mpact::sim::riscv::RV32VectorTrueOperand;
using ::mpact::sim::riscv::RVVectorRegister;

using SourceOpGetterMap =
    absl::flat_hash_map<int, absl::AnyInvocable<SourceOperandInterface *()>>;
using DestOpGetterMap =
    absl::flat_hash_map<int,
                        absl::AnyInvocable<DestinationOperandInterface *(int)>>;

template <typename Enum, typename Extractors>
void AddCheriotRVVSourceGetters(SourceOpGetterMap &getter_map,
                                RiscVCheriotEncodingCommon *common) {
  Insert(getter_map, *Enum::kConst1, [common]() -> SourceOperandInterface * {
    return new IntLiteralOperand<1>();
  });
  Insert(getter_map, *Enum::kConst2, [common]() -> SourceOperandInterface * {
    return new IntLiteralOperand<2>();
  });
  Insert(getter_map, *Enum::kConst4, [common]() -> SourceOperandInterface * {
    return new IntLiteralOperand<4>();
  });
  Insert(getter_map, *Enum::kConst8, [common]() -> SourceOperandInterface * {
    return new IntLiteralOperand<8>();
  });
  Insert(getter_map, *Enum::kNf, [common]() -> SourceOperandInterface * {
    auto imm = Extractors::VMem::ExtractNf(common->inst_word());
    return new ImmediateOperand<uint32_t>(imm);
  });
  Insert(getter_map, *Enum::kSimm5, [common]() -> SourceOperandInterface * {
    auto imm = Extractors::VArith::ExtractSimm5(common->inst_word());
    return new ImmediateOperand<uint32_t>(imm);
  });
  Insert(getter_map, *Enum::kUimm5, [common]() -> SourceOperandInterface * {
    auto imm = Extractors::VArith::ExtractUimm5(common->inst_word());
    return new ImmediateOperand<int32_t>(imm);
  });
  Insert(getter_map, *Enum::kVd, [common]() -> SourceOperandInterface * {
    auto num = Extractors::VArith::ExtractVd(common->inst_word());
    return GetVectorRegisterSourceOp<RVVectorRegister>(common->state(), num);
  });
  Insert(getter_map, *Enum::kVm, [common]() -> SourceOperandInterface * {
    auto vm = Extractors::VArith::ExtractVm(common->inst_word());
    return new ImmediateOperand<uint32_t>(vm);
  });
  Insert(getter_map, *Enum::kVmask, [common]() -> SourceOperandInterface * {
    auto vm = Extractors::VArith::ExtractVm(common->inst_word());
    if (vm == 1) {
      // Unmasked, return the True mask.
      return new CheriotVectorTrueOperand(common->state());
    }
    // Masked. Return the mask register.
    return GetVectorMaskRegisterSourceOp<RVVectorRegister>(common->state(), 0);
  });
  Insert(getter_map, *Enum::kVmaskTrue, [common]() -> SourceOperandInterface * {
    return new CheriotVectorTrueOperand(common->state());
  });
  Insert(getter_map, *Enum::kVs1, [common]() -> SourceOperandInterface * {
    auto num = Extractors::VArith::ExtractVs1(common->inst_word());
    return GetVectorRegisterSourceOp<RVVectorRegister>(common->state(), num);
  });
  Insert(getter_map, *Enum::kVs2, [common]() -> SourceOperandInterface * {
    auto num = Extractors::VArith::ExtractVs2(common->inst_word());
    return GetVectorRegisterSourceOp<RVVectorRegister>(common->state(), num);
  });
  Insert(getter_map, *Enum::kVs3, [common]() -> SourceOperandInterface * {
    auto num = Extractors::VMem::ExtractVs3(common->inst_word());
    return GetVectorRegisterSourceOp<RVVectorRegister>(common->state(), num);
  });
  Insert(getter_map, *Enum::kZimm10, [common]() -> SourceOperandInterface * {
    auto imm = Extractors::VConfig::ExtractZimm10(common->inst_word());
    return new ImmediateOperand<uint32_t>(imm);
  });
  Insert(getter_map, *Enum::kZimm11, [common]() -> SourceOperandInterface * {
    auto imm = Extractors::VConfig::ExtractZimm11(common->inst_word());
    return new ImmediateOperand<uint32_t>(imm);
  });
}

template <typename Enum, typename Extractors>
void AddCheriotRVVDestGetters(DestOpGetterMap &getter_map,
                              RiscVCheriotEncodingCommon *common) {
  Insert(getter_map, Enum::kVd,
         [common](int latency) -> DestinationOperandInterface * {
           auto num = Extractors::VArith::ExtractVd(common->inst_word());
           return GetVectorRegisterDestinationOp<RVVectorRegister>(
               common->state(), latency, num);
         });
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT_CHERIOT_RVV_GETTERS_H_
