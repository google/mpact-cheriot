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

#include "cheriot/riscv_cheriot_encoding.h"

#include <cstdint>
#include <string>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_bin_decoder.h"
#include "cheriot/riscv_cheriot_decoder.h"
#include "cheriot/riscv_cheriot_enums.h"
#include "cheriot/riscv_cheriot_register_aliases.h"
#include "mpact/sim/generic/immediate_operand.h"
#include "mpact/sim/generic/literal_operand.h"
#include "mpact/sim/generic/type_helpers.h"
#include "riscv//riscv_register.h"

namespace mpact {
namespace sim {
namespace cheriot {
namespace isa32 {

using ::mpact::sim::generic::operator*;  // NOLINT: is used below (clang error).
using ::mpact::sim::riscv::RVFpRegister;

// Generic helper functions to create register operands.
template <typename RegType>
inline DestinationOperandInterface *GetRegisterDestinationOp(
    CheriotState *state, std::string name, int latency) {
  auto *reg = state->GetRegister<RegType>(name).first;
  return reg->CreateDestinationOperand(latency);
}

template <typename RegType>
inline DestinationOperandInterface *GetRegisterDestinationOp(
    CheriotState *state, std::string name, int latency, std::string op_name) {
  auto *reg = state->GetRegister<RegType>(name).first;
  return reg->CreateDestinationOperand(latency, op_name);
}

template <typename T>
inline DestinationOperandInterface *GetCSRSetBitsDestinationOp(
    CheriotState *state, std::string name, int latency, std::string op_name) {
  auto result = state->csr_set()->GetCsr(name);
  if (!result.ok()) {
    LOG(ERROR) << "No such CSR '" << name << "'";
    return nullptr;
  }
  auto *csr = result.value();
  auto *op = csr->CreateSetDestinationOperand(latency, op_name);
  return op;
}

template <typename RegType>
inline SourceOperandInterface *GetRegisterSourceOp(CheriotState *state,
                                                   std::string name) {
  auto *reg = state->GetRegister<RegType>(name).first;
  auto *op = reg->CreateSourceOperand();
  return op;
}

template <typename RegType>
inline SourceOperandInterface *GetRegisterSourceOp(CheriotState *state,
                                                   std::string name,
                                                   std::string op_name) {
  auto *reg = state->GetRegister<RegType>(name).first;
  auto *op = reg->CreateSourceOperand(op_name);
  return op;
}

RiscVCheriotEncoding::RiscVCheriotEncoding(CheriotState *state)
    : state_(state) {
  InitializeSourceOperandGetters();
  InitializeDestinationOperandGetters();
}

void RiscVCheriotEncoding::InitializeSourceOperandGetters() {
  // Source operand getters.
  source_op_getters_.emplace(
      *SourceOpEnum::kAAq, [this]() -> SourceOperandInterface * {
        if (encoding::inst32_format::ExtractAq(inst_word_)) {
          return new generic::IntLiteralOperand<1>();
        }
        return new generic::IntLiteralOperand<0>();
      });
  source_op_getters_.emplace(
      *SourceOpEnum::kARl, [this]() -> SourceOperandInterface * {
        if (encoding::inst32_format::ExtractRl(inst_word_)) {
          return new generic::IntLiteralOperand<1>();
        }
        return new generic::IntLiteralOperand<0>();
      });
  source_op_getters_.emplace(*SourceOpEnum::kBImm12, [this]() {
    return new generic::ImmediateOperand<int32_t>(
        encoding::inst32_format::ExtractBImm(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kC2, [this]() {
    return GetRegisterSourceOp<CheriotRegister>(state_, "c2", "csp");
  });
  source_op_getters_.emplace(*SourceOpEnum::kC3cs1, [this]() {
    auto num = encoding::c_s::ExtractRs1(inst_word_);
    return GetRegisterSourceOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kCregPrefix, num),
        kCRegisterAliases[num]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kC3cs2, [this]() {
    auto num = encoding::c_s::ExtractRs2(inst_word_);
    return GetRegisterSourceOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kCregPrefix, num),
        kCRegisterAliases[num]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kC3rs1, [this]() {
    auto num = encoding::c_s::ExtractRs1(inst_word_);
    return GetRegisterSourceOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kXregPrefix, num),
        kXRegisterAliases[num]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kC3rs2, [this]() {
    auto num = encoding::c_s::ExtractRs2(inst_word_);
    return GetRegisterSourceOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kXregPrefix, num),
        kXRegisterAliases[num]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kCcs2, [this]() {
    auto num = encoding::c_s_s::ExtractRs2(inst_word_);
    return GetRegisterSourceOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kCregPrefix, num),
        kCRegisterAliases[num]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kCgp, [this]() {
    return GetRegisterSourceOp<CheriotRegister>(state_, "c3", "c3");
  });
  source_op_getters_.emplace(*SourceOpEnum::kCSRUimm5, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::inst32_format::ExtractIUimm5(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kCfrs2, [this]() {
    auto num = encoding::c_r::ExtractRs2(inst_word_);
    return GetRegisterSourceOp<RVFpRegister>(
        state_, absl::StrCat(CheriotState::kFregPrefix, num),
        kFRegisterAliases[num]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kCrs1, [this]() {
    auto num = encoding::c_r::ExtractRs1(inst_word_);
    return GetRegisterSourceOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kXregPrefix, num),
        kXRegisterAliases[num]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kCrs2, [this]() {
    auto num = encoding::c_r::ExtractRs2(inst_word_);
    return GetRegisterSourceOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kXregPrefix, num),
        kXRegisterAliases[num]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kCs1, [this]() {
    auto num = encoding::r_type::ExtractRs1(inst_word_);
    return GetRegisterSourceOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kCregPrefix, num),
        kCRegisterAliases[num]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kCs2, [this]() {
    auto num = encoding::r_type::ExtractRs2(inst_word_);
    return GetRegisterSourceOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kCregPrefix, num),
        kCRegisterAliases[num]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kCsr, [this]() {
    auto csr_indx = encoding::i_type::ExtractUImm12(inst_word_);
    auto res = state_->csr_set()->GetCsr(csr_indx);
    if (!res.ok()) {
      return new generic::ImmediateOperand<uint32_t>(csr_indx);
    }
    auto *csr = res.value();
    return new generic::ImmediateOperand<uint32_t>(csr_indx, csr->name());
  });
  /*
  source_op_getters_.emplace(*SourceOpEnum::kFrs1, [this]() {
    int num = encoding::r_type::ExtractRs1(inst_word_);
    return GetRegisterSourceOp<RVFpRegister>(
        state_, absl::StrCat(CheriotState::kFregPrefix, num),
        kFRegisterAliases[num]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kFrs2, [this]() {
    int num = encoding::r_type::ExtractRs2(inst_word_);
    return GetRegisterSourceOp<RVFpRegister>(
        state_, absl::StrCat(CheriotState::kFregPrefix, num),
        kFRegisterAliases[num]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kFrs3, [this]() {
    int num = encoding::r4_type::ExtractRs3(inst_word_);
    return GetRegisterSourceOp<RVFpRegister>(
        state_, absl::StrCat(CheriotState::kFregPrefix, num),
        kFRegisterAliases[num]);
  });
  */
  source_op_getters_.emplace(*SourceOpEnum::kICbImm8, [this]() {
    return new generic::ImmediateOperand<int32_t>(
        encoding::inst16_format::ExtractBimm(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kICiImm6, [this]() {
    return new generic::ImmediateOperand<int32_t>(
        encoding::c_i::ExtractImm6(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kICiImm612, [this]() {
    return new generic::ImmediateOperand<int32_t>(
        encoding::inst16_format::ExtractImm18(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kICiUimm6, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::inst16_format::ExtractUimm6(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kICiUimm6x4, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::inst16_format::ExtractCiImmW(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kICiImm6x16, [this]() {
    return new generic::ImmediateOperand<int32_t>(
        encoding::inst16_format::ExtractCiImm10(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kICiUimm6x8, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::inst16_format::ExtractCiImmD(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kICiwUimm8x4, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::inst16_format::ExtractCiwImm10(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kICjImm11, [this]() {
    return new generic::ImmediateOperand<int32_t>(
        encoding::inst16_format::ExtractJimm(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kIClUimm5x4, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::inst16_format::ExtractClImmW(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kIClUimm5x8, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::inst16_format::ExtractClImmD(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kICshUimm6, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::c_s_h::ExtractUimm6(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kICshImm6, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::c_s_h::ExtractImm6(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kICssUimm6x4, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::inst16_format::ExtractCssImmW(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kICssUimm6x8, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::inst16_format::ExtractCssImmD(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kIImm12, [this]() {
    return new generic::ImmediateOperand<int32_t>(
        encoding::inst32_format::ExtractImm12(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kIUimm5, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::i5_type::ExtractRUimm5(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kIUimm12, [this]() {
    return new generic::ImmediateOperand<uint32_t>(
        encoding::inst32_format::ExtractUImm12(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kJImm12, [this]() {
    return new generic::ImmediateOperand<int32_t>(
        encoding::inst32_format::ExtractImm12(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kJImm20, [this]() {
    return new generic::ImmediateOperand<int32_t>(
        encoding::inst32_format::ExtractJImm(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kPcc, [this]() {
    return GetRegisterSourceOp<CheriotRegister>(state_, "pcc", "pcc");
  });
  /*
  source_op_getters_.emplace(*SourceOpEnum::kRm,
                             [this]() -> SourceOperandInterface * {
                               uint32_t rm = (inst_word_ >> 12) & 0x7;
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
  */
  source_op_getters_.emplace(
      *SourceOpEnum::kRd, [this]() -> SourceOperandInterface * {
        int num = encoding::r_type::ExtractRd(inst_word_);
        if (num == 0) return new generic::IntLiteralOperand<0>({1});
        return GetRegisterSourceOp<CheriotRegister>(
            state_, absl::StrCat(CheriotState::kXregPrefix, num),
            kXRegisterAliases[num]);
      });
  source_op_getters_.emplace(
      *SourceOpEnum::kRs1, [this]() -> SourceOperandInterface * {
        int num = encoding::r_type::ExtractRs1(inst_word_);
        if (num == 0) return new generic::IntLiteralOperand<0>({1});
        return GetRegisterSourceOp<CheriotRegister>(
            state_, absl::StrCat(CheriotState::kXregPrefix, num),
            kXRegisterAliases[num]);
      });
  source_op_getters_.emplace(
      *SourceOpEnum::kRs2, [this]() -> SourceOperandInterface * {
        int num = encoding::r_type::ExtractRs2(inst_word_);
        if (num == 0) return new generic::IntLiteralOperand<0>({1});
        return GetRegisterSourceOp<CheriotRegister>(
            state_, absl::StrCat(CheriotState::kXregPrefix, num),
            kXRegisterAliases[num]);
      });
  source_op_getters_.emplace(*SourceOpEnum::kSImm12, [this]() {
    return new generic::ImmediateOperand<int32_t>(
        encoding::s_type::ExtractSImm(inst_word_));
  });
  source_op_getters_.emplace(
      *SourceOpEnum::kScr, [this]() -> SourceOperandInterface * {
        int csr_indx = encoding::r_type::ExtractRs2(inst_word_);
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
        auto res = state_->csr_set()->GetCsr(csr_name);
        if (!res.ok()) {
          return GetRegisterSourceOp<CheriotRegister>(state_, csr_name,
                                                      csr_name);
        }
        auto *csr = res.value();
        auto *op = csr->CreateSourceOperand();
        return op;
      });
  source_op_getters_.emplace(*SourceOpEnum::kSImm20, [this]() {
    return new generic::ImmediateOperand<int32_t>(
        encoding::u_type::ExtractSImm(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kUImm20, [this]() {
    return new generic::ImmediateOperand<int32_t>(
        encoding::inst32_format::ExtractUImm(inst_word_));
  });
  source_op_getters_.emplace(*SourceOpEnum::kX0, []() {
    return new generic::IntLiteralOperand<0>({1});
  });
  source_op_getters_.emplace(*SourceOpEnum::kX2, [this]() {
    return GetRegisterSourceOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kXregPrefix, 2),
        kXRegisterAliases[2]);
  });
  source_op_getters_.emplace(*SourceOpEnum::kNone, []() { return nullptr; });
}

void RiscVCheriotEncoding::InitializeDestinationOperandGetters() {
  // Destination operand getters.
  dest_op_getters_.emplace(*DestOpEnum::kC2, [this](int latency) {
    return GetRegisterDestinationOp<CheriotRegister>(state_, "c2", latency,
                                                     "csp");
  });
  dest_op_getters_.emplace(*DestOpEnum::kC3cd, [this](int latency) {
    int num = encoding::c_l::ExtractRd(inst_word_);
    return GetRegisterDestinationOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kCregPrefix, num), latency,
        kCRegisterAliases[num]);
  });
  dest_op_getters_.emplace(*DestOpEnum::kC3rd, [this](int latency) {
    int num = encoding::c_l::ExtractRd(inst_word_);
    if (num == 0) {
      return GetRegisterDestinationOp<CheriotRegister>(state_, "X0Dest",
                                                       latency);
    }
    return GetRegisterDestinationOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kXregPrefix, num), latency,
        kXRegisterAliases[num]);
  });
  dest_op_getters_.emplace(*DestOpEnum::kC3rs1, [this](int latency) {
    int num = encoding::c_l::ExtractRs1(inst_word_);
    return GetRegisterDestinationOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kXregPrefix, num), latency,
        kXRegisterAliases[num]);
  });
  dest_op_getters_.emplace(*DestOpEnum::kCd, [this](int latency) {
    int num = encoding::r_type::ExtractRd(inst_word_);
    if (num == 0) {
      return GetRegisterDestinationOp<CheriotRegister>(state_, "X0Dest",
                                                       latency);
    }
    return GetRegisterDestinationOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kCregPrefix, num), latency,
        kCRegisterAliases[num]);
  });
  dest_op_getters_.emplace(*DestOpEnum::kCsr, [this](int latency) {
    return GetRegisterDestinationOp<CheriotRegister>(
        state_, CheriotState::kCsrName, latency);
  });
  dest_op_getters_.emplace(*DestOpEnum::kFrd, [this](int latency) {
    int num = encoding::r_type::ExtractRd(inst_word_);
    return GetRegisterDestinationOp<RVFpRegister>(
        state_, absl::StrCat(CheriotState::kFregPrefix, num), latency,
        kFRegisterAliases[num]);
  });
  dest_op_getters_.emplace(
      *DestOpEnum::kScr, [this](int latency) -> DestinationOperandInterface * {
        int csr_indx = encoding::r_type::ExtractRs2(inst_word_);
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
        auto res = state_->csr_set()->GetCsr(csr_name);
        if (!res.ok()) {
          return GetRegisterDestinationOp<CheriotRegister>(state_, csr_name,
                                                           latency);
        }
        auto *csr = res.value();
        auto *op = csr->CreateWriteDestinationOperand(latency, csr_name);
        return op;
      });
  dest_op_getters_.emplace(
      *DestOpEnum::kRd, [this](int latency) -> DestinationOperandInterface * {
        int num = encoding::r_type::ExtractRd(inst_word_);
        if (num == 0) {
          return GetRegisterDestinationOp<CheriotRegister>(state_, "X0Dest", 0);
        } else {
          return GetRegisterDestinationOp<RVFpRegister>(
              state_, absl::StrCat(CheriotState::kXregPrefix, num), latency,
              kXRegisterAliases[num]);
        }
      });
  dest_op_getters_.emplace(*DestOpEnum::kX1, [this](int latency) {
    return GetRegisterDestinationOp<CheriotRegister>(
        state_, absl::StrCat(CheriotState::kXregPrefix, 1), latency,
        kXRegisterAliases[1]);
  });
  /*
  dest_op_getters_.emplace(*DestOpEnum::kFflags, [this](int latency) {
    return GetCSRSetBitsDestinationOp<uint32_t>(state_, "fflags", latency, "");
  });
  */
  dest_op_getters_.emplace(*DestOpEnum::kNone,
                           [](int latency) { return nullptr; });
}

// Parse the instruction word to determine the opcode.
void RiscVCheriotEncoding::ParseInstruction(uint32_t inst_word) {
  inst_word_ = inst_word;
  if ((inst_word_ & 0x3) == 3) {
    auto [opcode, format] =
        mpact::sim::cheriot::encoding::DecodeRiscVCheriotInst32WithFormat(
            inst_word_);
    opcode_ = opcode;
    format_ = format;
    return;
  }

  auto [opcode, format] =
      mpact::sim::cheriot::encoding::DecodeRiscVCheriotInst16WithFormat(
          static_cast<uint16_t>(inst_word_ & 0xffff));
  opcode_ = opcode;
  format_ = format;
}

DestinationOperandInterface *RiscVCheriotEncoding::GetDestination(
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

SourceOperandInterface *RiscVCheriotEncoding::GetSource(SlotEnum, int,
                                                        OpcodeEnum opcode,
                                                        SourceOpEnum source_op,
                                                        int source_no) {
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

}  // namespace isa32
}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
