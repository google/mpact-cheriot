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

#ifndef MPACT_CHERIOT_CHERIOT_GETTERS_H_
#define MPACT_CHERIOT_CHERIOT_GETTERS_H_

#include <cstdint>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/str_cat.h"
#include "cheriot/cheriot_getter_helpers.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_encoding_common.h"
#include "cheriot/riscv_cheriot_register_aliases.h"
#include "mpact/sim/generic/immediate_operand.h"
#include "mpact/sim/generic/literal_operand.h"
#include "mpact/sim/generic/operand_interface.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::cheriot::RiscVCheriotEncodingCommon;
using ::mpact::sim::generic::DestinationOperandInterface;
using ::mpact::sim::generic::ImmediateOperand;
using ::mpact::sim::generic::IntLiteralOperand;
using ::mpact::sim::generic::SourceOperandInterface;

using SourceOpGetterMap =
    absl::flat_hash_map<int, absl::AnyInvocable<SourceOperandInterface*()>>;
using DestOpGetterMap =
    absl::flat_hash_map<int,
                        absl::AnyInvocable<DestinationOperandInterface*(int)>>;

template <typename Enum, typename Extractors>
void AddCheriotSourceGetters(SourceOpGetterMap& getter_map,
                             RiscVCheriotEncodingCommon* common) {
  // Source operand getters.
  Insert(getter_map, *Enum::kAAq, [common]() -> SourceOperandInterface* {
    if (Extractors::Inst32Format::ExtractAq(common->inst_word())) {
      return new IntLiteralOperand<1>();
    }
    return new IntLiteralOperand<0>();
  });
  Insert(getter_map, *Enum::kARl, [common]() -> SourceOperandInterface* {
    if (Extractors::Inst32Format::ExtractRl(common->inst_word())) {
      return new generic::IntLiteralOperand<1>();
    }
    return new generic::IntLiteralOperand<0>();
  });
  Insert(getter_map, *Enum::kBImm12, [common]() {
    return new ImmediateOperand<int32_t>(
        Extractors::Inst32Format::ExtractBImm(common->inst_word()));
  });
  Insert(getter_map, *Enum::kC2, [common]() {
    return GetRegisterSourceOp<CheriotRegister>(common->state(), "c2", "csp");
  });
  Insert(getter_map, *Enum::kC3cs1, [common]() {
    auto num = Extractors::CS::ExtractRs1(common->inst_word());
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kCregPrefix, num),
        kCRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kC3cs2, [common]() {
    auto num = Extractors::CS::ExtractRs2(common->inst_word());
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kCregPrefix, num),
        kCRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kC3rs1, [common]() {
    auto num = Extractors::CS::ExtractRs1(common->inst_word());
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kXregPrefix, num),
        kXRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kC3rs2, [common]() {
    auto num = Extractors::CS::ExtractRs2(common->inst_word());
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kXregPrefix, num),
        kXRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kCcs2, [common]() {
    auto num = Extractors::CSS::ExtractRs2(common->inst_word());
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kCregPrefix, num),
        kCRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kCgp, [common]() {
    return GetRegisterSourceOp<CheriotRegister>(common->state(), "c3", "c3");
  });
  Insert(getter_map, *Enum::kCSRUimm5, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::Inst32Format::ExtractIUimm5(common->inst_word()));
  });
  Insert(getter_map, *Enum::kCrs1, [common]() {
    auto num = Extractors::CR::ExtractRs1(common->inst_word());
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kXregPrefix, num),
        kXRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kCrs2, [common]() {
    auto num = Extractors::CR::ExtractRs2(common->inst_word());
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kXregPrefix, num),
        kXRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kCs1, [common]() {
    auto num = Extractors::RType::ExtractRs1(common->inst_word());
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kCregPrefix, num),
        kCRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kCs2, [common]() {
    auto num = Extractors::RType::ExtractRs2(common->inst_word());
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kCregPrefix, num),
        kCRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kCsr, [common]() {
    auto csr_indx = Extractors::IType::ExtractUImm12(common->inst_word());
    auto res = common->state()->csr_set()->GetCsr(csr_indx);
    if (!res.ok()) {
      return new ImmediateOperand<uint32_t>(csr_indx);
    }
    auto* csr = res.value();
    return new ImmediateOperand<uint32_t>(csr_indx, csr->name());
  });
  Insert(getter_map, *Enum::kICbImm8, [common]() {
    return new ImmediateOperand<int32_t>(
        Extractors::Inst16Format::ExtractBimm(common->inst_word()));
  });
  Insert(getter_map, *Enum::kICiImm6, [common]() {
    return new ImmediateOperand<int32_t>(
        Extractors::CI::ExtractImm6(common->inst_word()));
  });
  Insert(getter_map, *Enum::kICiImm612, [common]() {
    return new ImmediateOperand<int32_t>(
        Extractors::Inst16Format::ExtractImm18(common->inst_word()));
  });
  Insert(getter_map, *Enum::kICiUimm6, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::Inst16Format::ExtractUimm6(common->inst_word()));
  });
  Insert(getter_map, *Enum::kICiUimm6x4, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::Inst16Format::ExtractCiImmW(common->inst_word()));
  });
  Insert(getter_map, *Enum::kICiImm6x16, [common]() {
    return new ImmediateOperand<int32_t>(
        Extractors::Inst16Format::ExtractCiImm10(common->inst_word()));
  });
  Insert(getter_map, *Enum::kICiUimm6x8, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::Inst16Format::ExtractCiImmD(common->inst_word()));
  });
  Insert(getter_map, *Enum::kICiwUimm8x4, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::Inst16Format::ExtractCiwImm10(common->inst_word()));
  });
  Insert(getter_map, *Enum::kICjImm11, [common]() {
    return new ImmediateOperand<int32_t>(
        Extractors::Inst16Format::ExtractJimm(common->inst_word()));
  });
  Insert(getter_map, *Enum::kIClUimm5x4, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::Inst16Format::ExtractClImmW(common->inst_word()));
  });
  Insert(getter_map, *Enum::kIClUimm5x8, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::Inst16Format::ExtractClImmD(common->inst_word()));
  });
  Insert(getter_map, *Enum::kICshUimm6, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::CSH::ExtractUimm6(common->inst_word()));
  });
  Insert(getter_map, *Enum::kICshImm6, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::CSH::ExtractImm6(common->inst_word()));
  });
  Insert(getter_map, *Enum::kICssUimm6x4, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::Inst16Format::ExtractCssImmW(common->inst_word()));
  });
  Insert(getter_map, *Enum::kICssUimm6x8, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::Inst16Format::ExtractCssImmD(common->inst_word()));
  });
  Insert(getter_map, *Enum::kIImm12, [common]() {
    return new ImmediateOperand<int32_t>(
        Extractors::Inst32Format::ExtractImm12(common->inst_word()));
  });
  Insert(getter_map, *Enum::kIUimm5, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::I5Type::ExtractRUimm5(common->inst_word()));
  });
  Insert(getter_map, *Enum::kIUimm12, [common]() {
    return new ImmediateOperand<uint32_t>(
        Extractors::Inst32Format::ExtractUImm12(common->inst_word()));
  });
  Insert(getter_map, *Enum::kJImm12, [common]() {
    return new ImmediateOperand<int32_t>(
        Extractors::Inst32Format::ExtractImm12(common->inst_word()));
  });
  Insert(getter_map, *Enum::kJImm20, [common]() {
    return new ImmediateOperand<int32_t>(
        Extractors::Inst32Format::ExtractJImm(common->inst_word()));
  });
  Insert(getter_map, *Enum::kPcc, [common]() {
    return GetRegisterSourceOp<CheriotRegister>(common->state(), "pcc", "pcc");
  });
  Insert(getter_map, *Enum::kRd, [common]() -> SourceOperandInterface* {
    int num = Extractors::RType::ExtractRd(common->inst_word());
    if (num == 0) return new generic::IntLiteralOperand<0>({1});
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kXregPrefix, num),
        kXRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kRs1, [common]() -> SourceOperandInterface* {
    int num = Extractors::RType::ExtractRs1(common->inst_word());
    if (num == 0) return new generic::IntLiteralOperand<0>({1});
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kXregPrefix, num),
        kXRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kRs2, [common]() -> SourceOperandInterface* {
    int num = Extractors::RType::ExtractRs2(common->inst_word());
    if (num == 0) return new generic::IntLiteralOperand<0>({1});
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kXregPrefix, num),
        kXRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kSImm12, [common]() {
    return new ImmediateOperand<int32_t>(
        Extractors::SType::ExtractSImm(common->inst_word()));
  });
  Insert(getter_map, *Enum::kScr, [common]() -> SourceOperandInterface* {
    int csr_indx = Extractors::RType::ExtractRs2(common->inst_word());
    std::string csr_name;
    switch (csr_indx) {
      case 28:
        csr_name = "mtcc";
        break;
      case 29:
        csr_name = "mtdc";
        break;
      case 30:
        csr_name = "mscratchc";
        break;
      case 31:
        csr_name = "mepcc";
        break;
      default:
        return nullptr;
    }
    auto res = common->state()->csr_set()->GetCsr(csr_name);
    if (!res.ok()) {
      return GetRegisterSourceOp<CheriotRegister>(common->state(), csr_name,
                                                  csr_name);
    }
    auto* csr = res.value();
    auto* op = csr->CreateSourceOperand();
    return op;
  });
  Insert(getter_map, *Enum::kSImm20, [common]() {
    return new ImmediateOperand<int32_t>(
        Extractors::UType::ExtractSImm(common->inst_word()));
  });
  Insert(getter_map, *Enum::kUImm20, [common]() {
    return new ImmediateOperand<int32_t>(
        Extractors::Inst32Format::ExtractUImm(common->inst_word()));
  });
  Insert(getter_map, *Enum::kX0,
         []() { return new generic::IntLiteralOperand<0>({1}); });
  Insert(getter_map, *Enum::kX2, [common]() {
    return GetRegisterSourceOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kXregPrefix, 2),
        kXRegisterAliases[2]);
  });
}

template <typename Enum, typename Extractors>
void AddCheriotDestGetters(DestOpGetterMap& getter_map,
                           RiscVCheriotEncodingCommon* common) {
  // Destination operand getters.
  Insert(getter_map, *Enum::kC2, [common](int latency) {
    return GetRegisterDestinationOp<CheriotRegister>(common->state(), "c2",
                                                     latency, "csp");
  });
  Insert(getter_map, *Enum::kC3cd, [common](int latency) {
    int num = Extractors::CL::ExtractRd(common->inst_word());
    return GetRegisterDestinationOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kCregPrefix, num), latency,
        kCRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kC3rd, [common](int latency) {
    int num = Extractors::CL::ExtractRd(common->inst_word());
    if (num == 0) {
      return GetRegisterDestinationOp<CheriotRegister>(common->state(),
                                                       "X0Dest", latency);
    }
    return GetRegisterDestinationOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kXregPrefix, num), latency,
        kXRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kC3rs1, [common](int latency) {
    int num = Extractors::CL::ExtractRs1(common->inst_word());
    return GetRegisterDestinationOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kXregPrefix, num), latency,
        kXRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kCd, [common](int latency) {
    int num = Extractors::RType::ExtractRd(common->inst_word());
    if (num == 0) {
      return GetRegisterDestinationOp<CheriotRegister>(common->state(),
                                                       "X0Dest", latency);
    }
    return GetRegisterDestinationOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kCregPrefix, num), latency,
        kCRegisterAliases[num]);
  });
  Insert(getter_map, *Enum::kCsr, [common](int latency) {
    return GetRegisterDestinationOp<CheriotRegister>(
        common->state(), CheriotState::kCsrName, latency);
  });
  Insert(getter_map, *Enum::kScr,
         [common](int latency) -> DestinationOperandInterface* {
           int csr_indx = Extractors::RType::ExtractRs2(common->inst_word());
           std::string csr_name;
           switch (csr_indx) {
             case 28:
               csr_name = "mtcc";
               break;
             case 29:
               csr_name = "mtdc";
               break;
             case 30:
               csr_name = "mscratchc";
               break;
             case 31:
               csr_name = "mepcc";
               break;
             default:
               return nullptr;
           }
           auto res = common->state()->csr_set()->GetCsr(csr_name);
           if (!res.ok()) {
             return GetRegisterDestinationOp<CheriotRegister>(
                 common->state(), csr_name, latency);
           }
           auto* csr = res.value();
           auto* op = csr->CreateWriteDestinationOperand(latency, csr_name);
           return op;
         });
  Insert(getter_map, *Enum::kRd,
         [common](int latency) -> DestinationOperandInterface* {
           int num = Extractors::RType::ExtractRd(common->inst_word());
           if (num == 0) {
             return GetRegisterDestinationOp<CheriotRegister>(common->state(),
                                                              "X0Dest", 0);
           } else {
             return GetRegisterDestinationOp<CheriotRegister>(
                 common->state(), absl::StrCat(CheriotState::kXregPrefix, num),
                 latency, kXRegisterAliases[num]);
           }
         });
  Insert(getter_map, *Enum::kX1, [common](int latency) {
    return GetRegisterDestinationOp<CheriotRegister>(
        common->state(), absl::StrCat(CheriotState::kXregPrefix, 1), latency,
        kXRegisterAliases[1]);
  });
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT_CHERIOT_GETTERS_H_
