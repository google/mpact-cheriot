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

#include "cheriot/riscv_cheriot_instructions.h"

#include <any>
#include <cstdint>
#include <type_traits>

#include "absl/log/log.h"
#include "absl/numeric/bits.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/operand_interface.h"
#include "mpact/sim/generic/register.h"
#include "mpact/sim/generic/type_helpers.h"
#include "riscv//riscv_register.h"
#include "riscv//riscv_state.h"

// This file contains the implementations of the RiscV CHERIoT instruction
// semantic functions. These instructions are defined in section 9 in the
// Microsoft Tech Report MSR-TR-2023-6 "CHERIoT: Rethinking security for
// low-cost embedded systems"."

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::operator*;  // NOLINT: is used below (clang error).
using CapReg = CheriotRegister;
using EC = ::mpact::sim::cheriot::ExceptionCode;
using RV32Register = ::mpact::sim::riscv::RV32Register;
using ::mpact::sim::generic::RegisterBase;

// Helpers to get capability register source and destination registers.
static inline CapReg* GetCapSource(const Instruction* instruction, int i) {
  return static_cast<CapReg*>(
      std::any_cast<RegisterBase*>(instruction->Source(i)->GetObject()));
}

static inline CapReg* GetCapDest(const Instruction* instruction, int i) {
  return static_cast<CapReg*>(
      std::any_cast<RegisterBase*>(instruction->Destination(i)->GetObject()));
}
// Writing an integer result requires invalidating the capability and setting
// it to null.
template <typename Result>
static inline void WriteCapIntResult(const Instruction* instruction, int i,
                                     Result value) {
  auto* cap_reg = GetCapDest(instruction, i);
  cap_reg->data_buffer()->Set<Result>(0, value);
  cap_reg->Invalidate();
  cap_reg->set_is_null();
}

// Sign extension helper function.
template <typename T>
static inline T SignExtend(T value, int size) {
  using ST = typename std::make_signed<T>::type;
  ST svalue = value;
  int shift_amount = sizeof(T) * 8 - size;
  svalue = (svalue << shift_amount) >> shift_amount;
  return static_cast<T>(svalue);
}

// Instruction semantic function bodies.

void CheriotAuicap(const Instruction* instruction) {
  auto* cap_src = GetCapSource(instruction, 0);
  auto offset = generic::GetInstructionSource<uint32_t>(instruction, 1);
  auto* cap_dest = GetCapDest(instruction, 0);
  cap_dest->CopyFrom(*cap_src);
  uint32_t address = cap_src->address() + offset;
  cap_dest->data_buffer()->Set<uint32_t>(0, address);
  if (cap_dest->IsSealed()) {
    cap_dest->Invalidate();
  }
  if (!cap_dest->IsRepresentable()) cap_dest->Invalidate();
}

void CheriotCAndPerm(const Instruction* instruction) {
  auto* cap_src = GetCapSource(instruction, 0);
  auto perms = cap_src->permissions();
  auto perms_to_keep = generic::GetInstructionSource<uint32_t>(instruction, 1);
  auto new_perms = perms & perms_to_keep;
  auto* cap_dest = GetCapDest(instruction, 0);
  bool valid = !cap_src->IsSealed();
  cap_dest->CopyFrom(*cap_src);
  cap_dest->ClearPermissions(perms ^ new_perms);
  if (!valid) cap_dest->Invalidate();
}

void CheriotCClearTag(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto* cd = GetCapDest(instruction, 0);
  if (cd != cs1) cd->CopyFrom(*cs1);
  cd->Invalidate();
}

void CheriotCGetAddr(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  WriteCapIntResult<uint32_t>(instruction, 0, cs1->address());
}

void CheriotCGetBase(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto [base, unused] = cs1->ComputeBounds();
  WriteCapIntResult(instruction, 0, base);
}

void CheriotCGetHigh(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  WriteCapIntResult<uint32_t>(instruction, 0, cs1->Compress());
}

void CheriotCGetLen(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto [base, top] = cs1->ComputeBounds();
  uint64_t length = top - base;
  if (length == 0x1'0000'0000ULL) length = 0xffff'ffff;
  WriteCapIntResult<uint32_t>(instruction, 0, length);
}

void CheriotCGetPerm(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  WriteCapIntResult<uint32_t>(instruction, 0, cs1->permissions());
}

void CheriotCGetTag(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  WriteCapIntResult<uint32_t>(instruction, 0, cs1->tag());
}

void CheriotCGetTop(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto [unused, top] = cs1->ComputeBounds();
  // auto top = cs1->top();
  if (top == 0x1'0000'0000ULL) {
    top = 0xffff'ffff;
  }
  WriteCapIntResult<uint32_t>(instruction, 0, top);
}

void CheriotCGetType(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  uint32_t object_type = cs1->object_type();
  object_type &= 0b0111;
  if ((object_type != 0) && (!cs1->HasPermission(CapReg::kPermitExecute))) {
    object_type |= 0b1000;
  }
  WriteCapIntResult<uint32_t>(instruction, 0, object_type);
}

void CheriotCIncAddr(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto inc = generic::GetInstructionSource<uint32_t>(instruction, 1);
  auto* cd = GetCapDest(instruction, 0);
  uint32_t new_addr = cs1->address() + inc;
  bool valid = true;
  if (cs1->IsSealed()) valid = false;
  cd->CopyFrom(*cs1);
  cd->SetAddress(new_addr);
  if (!cd->IsRepresentable() || !valid) cd->Invalidate();
}

// Helper function to check for exceptions for Jal and J.
static bool CheriotCJChecks(const Instruction* instruction, uint64_t new_pc,
                            const CheriotRegister* pcc) {
  auto* state = static_cast<CheriotState*>(instruction->state());
  if (!state->has_compact() && (new_pc & 0b10)) {
    state->Trap(/*is_interrupt*/ false, new_pc,
                *riscv::ExceptionCode::kInstructionAddressMisaligned,
                instruction->address(), instruction);
    return false;
  }
  return true;
}

void CheriotCJal(const Instruction* instruction) {
  auto* state = static_cast<CheriotState*>(instruction->state());
  auto offset = generic::GetInstructionSource<uint32_t>(instruction, 0);
  uint64_t new_pc = offset + instruction->address();
  auto* pcc = state->pcc();
  if (!CheriotCJChecks(instruction, new_pc, pcc)) return;
  // Update link register.
  auto* cd = GetCapDest(instruction, 0);
  cd->CopyFrom(*pcc);
  cd->set_address(instruction->address() + instruction->size());
  bool interrupt_enable = state->mstatus()->mie();
  if (state->core_version() == CheriotState::kVersion0Dot5) {
    (void)cd->Seal(*state->sealing_root(),
                   interrupt_enable
                       ? CapReg::kInterruptEnablingBackwardSentry
                       : CapReg::kInterruptDisablingBackwardSentry);
  }
  // Update pcc.
  pcc->set_address(new_pc);
  state->set_branch(true);
}

void CheriotCJalCra(const Instruction* instruction) {
  auto* state = static_cast<CheriotState*>(instruction->state());
  auto offset = generic::GetInstructionSource<uint32_t>(instruction, 0);
  uint64_t new_pc = offset + instruction->address();
  auto* pcc = state->pcc();
  if (!CheriotCJChecks(instruction, new_pc, pcc)) return;
  // Update link register.
  auto* cd = GetCapDest(instruction, 0);
  cd->CopyFrom(*pcc);
  cd->set_address(instruction->address() + instruction->size());
  bool interrupt_enable = state->mstatus()->mie();
  (void)cd->Seal(*state->sealing_root(),
                 interrupt_enable ? CapReg::kInterruptEnablingBackwardSentry
                                  : CapReg::kInterruptDisablingBackwardSentry);
  // Update pcc.
  pcc->set_address(new_pc);
  state->set_branch(true);
}

void CheriotCJ(const Instruction* instruction) {
  auto* state = static_cast<CheriotState*>(instruction->state());
  auto offset = generic::GetInstructionSource<uint32_t>(instruction, 0);
  uint64_t new_pc = offset + instruction->address();
  auto* pcc = state->pcc();
  if (!CheriotCJChecks(instruction, new_pc, pcc)) return;
  // Update pcc.
  pcc->set_address(new_pc);
  state->set_branch(true);
}

// Helper function to check for exceptions for Jr and Jalr.
static bool CheriotCJrCheck(const Instruction* instruction, uint64_t new_pc,
                            uint32_t offset, const CheriotRegister* cs1,
                            bool has_dest, bool uses_ra) {
  auto* state = static_cast<CheriotState*>(instruction->state());
  if (!cs1->tag()) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   EC::kCapExTagViolation, cs1);
    return false;
  }
  bool ok = false;
  ok |= !has_dest && uses_ra && cs1->IsBackwardSentry();
  ok |= !has_dest && !uses_ra &&
        ((cs1->object_type() == CapReg::kUnsealed) ||
         (cs1->object_type() == CapReg::kInterruptInheritingSentry));
  ok |=
      has_dest && ((cs1->object_type() == CapReg::kUnsealed) ||
                   (cs1->object_type() == CapReg::kInterruptInheritingSentry));
  ok |= has_dest && uses_ra && (cs1->object_type() >= CapReg::kUnsealed) &&
        (cs1->object_type() <= CapReg::kInterruptEnablingForwardSentry);
  if ((cs1->IsSealed() && offset != 0) || !ok) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   EC::kCapExSealViolation, cs1);
    return false;
  }
  if (!cs1->HasPermission(CapReg::kPermitExecute)) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   EC::kCapExPermitExecuteViolation, cs1);
    return false;
  }
  if (!state->has_compact() && (new_pc & 0b10)) {
    state->Trap(/*is_interrupt*/ false, new_pc,
                *riscv::ExceptionCode::kInstructionAddressMisaligned,
                instruction->address(), instruction);
    return false;
  }
  return true;
}

static inline void CheriotCJalrHelper(const Instruction* instruction,
                                      bool has_dest, bool uses_ra) {
  auto* state = static_cast<CheriotState*>(instruction->state());
  auto* cs1 = GetCapSource(instruction, 0);
  auto offset = generic::GetInstructionSource<uint32_t>(instruction, 1);
  auto* pcc = state->pcc();
  auto new_pc = offset + cs1->address();
  new_pc &= ~0b1ULL;
  if (!CheriotCJrCheck(instruction, new_pc, offset, cs1, has_dest, uses_ra)) {
    return;
  }
  auto* mstatus = state->mstatus();
  if (has_dest) {
    // Update link register.
    state->temp_reg()->CopyFrom(*pcc);
    state->temp_reg()->set_address(instruction->address() +
                                   instruction->size());
    bool interrupt_enable = (mstatus->GetUint32() & 0b1000) != 0;
    if ((state->core_version() == CheriotState::kVersion0Dot5) || uses_ra) {
      auto status = state->temp_reg()->Seal(
          *state->sealing_root(),
          interrupt_enable ? CapReg::kInterruptEnablingBackwardSentry
                           : CapReg::kInterruptDisablingBackwardSentry);
      if (!status.ok()) {
        LOG(ERROR) << "Failed to seal: " << status;
        return;
      }
    }
  }
  // Update pcc.
  pcc->CopyFrom(*cs1);
  // If the new pcc is a sentry, unseal and set/clear mie accordingly.
  if (pcc->IsSentry()) {
    if (pcc->object_type() != CapReg::kInterruptInheritingSentry) {
      bool interrupt_enable =
          (pcc->object_type() == CapReg::kInterruptEnablingForwardSentry) ||
          (pcc->object_type() == CapReg::kInterruptEnablingBackwardSentry);
      mstatus->set_mie(interrupt_enable);
      mstatus->Submit();
    }
    (void)pcc->Unseal(*state->sealing_root(), pcc->object_type());
  }
  pcc->set_address(new_pc);
  state->set_branch(true);
  if (has_dest) {
    auto* cd = GetCapDest(instruction, 0);
    cd->CopyFrom(*state->temp_reg());
  }
}

void CheriotCJalr(const Instruction* instruction) {
  CheriotCJalrHelper(instruction, /*has_dest=*/true, /*uses_ra=*/false);
}

void CheriotCJalrCra(const Instruction* instruction) {
  CheriotCJalrHelper(instruction, /*has_dest=*/true, /*uses_ra=*/true);
}

void CheriotCJrCra(const Instruction* instruction) {
  CheriotCJalrHelper(instruction, /*has_dest=*/false, /*uses_ra=*/true);
}

void CheriotCJr(const Instruction* instruction) {
  CheriotCJalrHelper(instruction, /*has_dest=*/false, /*uses_ra=*/false);
}

void CheriotCJalrZero(const Instruction* instruction) {
  auto* state = static_cast<CheriotState*>(instruction->state());
  auto* cs1 = GetCapSource(instruction, 0);
  state->HandleCheriRegException(instruction, instruction->address(),
                                 EC::kCapExTagViolation, cs1);
}

void CheriotCLc(const Instruction* instruction) {
  auto* state = static_cast<CheriotState*>(instruction->state());
  auto offset = generic::GetInstructionSource<uint32_t>(instruction, 1);
  auto* cs1 = GetCapSource(instruction, 0);
  uint32_t address = cs1->address() + offset;
  if (!cs1->tag()) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   EC::kCapExTagViolation, cs1);
    return;
  }
  if (cs1->IsSealed()) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   EC::kCapExSealViolation, cs1);
    return;
  }
  if (!cs1->HasPermission(CapReg::kPermitLoad)) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   EC::kCapExPermitLoadViolation, cs1);
    return;
  }
  if (!cs1->IsInBounds(address, CapReg::kCapabilitySizeInBytes)) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   EC::kCapExBoundsViolation, cs1);
    return;
  }
  if ((address & ((1 << CapReg::kGranuleShift) - 1)) != 0) {
    state->Trap(/*is_interrupt*/ false, address,
                *riscv::ExceptionCode::kLoadAddressMisaligned,
                instruction->address(), instruction);
    return;
  }
  auto* db = state->db_factory()->Allocate(CapReg::kCapabilitySizeInBytes);
  db->set_latency(0);
  auto* tag_db = state->db_factory()->Allocate(1);
  auto* context = new CapabilityLoadContext32(db, tag_db, cs1->permissions(),
                                              /*clear_tag=*/false);
  state->LoadCapability(instruction, address, db, tag_db, instruction->child(),
                        context);
  context->DecRef();
}

void CheriotCLcChild(const Instruction* instruction) {
  auto* state = static_cast<CheriotState*>(instruction->state());
  auto* context = static_cast<CapabilityLoadContext32*>(instruction->context());
  auto* cd = GetCapDest(instruction, 0);
  cd->Expand(context->db->Get<uint32_t>(0), context->db->Get<uint32_t>(1),
             context->tag_db->Get<uint8_t>(0));
  // If the source capability did not have load/store capability, invalidate.
  if ((context->permissions & CapReg::kPermitLoadStoreCapability) == 0) {
    cd->Invalidate();
  }
  if (cd->tag()) {
    if ((context->permissions & CapReg::kPermitLoadGlobal) == 0) {
      cd->ClearPermissions(CapReg::kPermitGlobal);
      if (!cd->IsSealed()) cd->ClearPermissions(CapReg::kPermitLoadGlobal);
    }
    if (!cd->IsSealed() &&
        ((context->permissions & CapReg::kPermitLoadMutable) == 0)) {
      cd->ClearPermissions(CapReg::kPermitStore | CapReg::kPermitLoadMutable);
    }
    // If it's not a sealing cap, check for revocation.
    if ((cd->permissions() & (CapReg::kPermitSeal | CapReg::kPermitUnseal |
                              CapReg::kUserPerm0)) == 0) {
      auto granule_addr = cd->base() & ~((1ULL << CapReg::kGranuleShift) - 1);
      if (state->MustRevoke(granule_addr)) {
        cd->Invalidate();
      }
    }
  }
}

void CheriotCMove(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto* cd = GetCapDest(instruction, 0);
  cd->CopyFrom(*cs1);
}

static uint32_t GetExponent(uint32_t length) {
  constexpr uint32_t kMaxLenBase = (1 << 9) - 1;
  if (length > kMaxLenBase * (1 << 14)) return 24;
  // Compute the power of 2 value that is the ceiling of the rounded division
  uint32_t alignment = absl::bit_ceil((length + kMaxLenBase - 1) / kMaxLenBase);
  return absl::bit_width(alignment) - 1;
}

void CheriotCRepresentableAlignmentMask(const Instruction* instruction) {
  auto rs1 = generic::GetInstructionSource<uint32_t>(instruction, 0);
  auto exp = GetExponent(rs1);
  WriteCapIntResult<uint32_t>(instruction, 0, 0xffff'ffffU << exp);
}

void CheriotCRoundRepresentableLength(const Instruction* instruction) {
  auto rs1 = generic::GetInstructionSource<uint32_t>(instruction, 0);
  auto exp = GetExponent(rs1);
  uint32_t mask = (1 << exp) - 1;
  uint32_t length = ((rs1 + mask) / (mask + 1)) * (mask + 1);
  WriteCapIntResult(instruction, 0, length);
}

void CheriotCSc(const Instruction* instruction) {
  auto* state = static_cast<CheriotState*>(instruction->state());
  auto* cs1 = GetCapSource(instruction, 0);
  auto* cs2 = GetCapSource(instruction, 2);
  uint32_t imm = generic::GetInstructionSource<uint32_t>(instruction, 1);
  uint32_t address = cs1->address() + imm;
  uint8_t tag = cs2->tag();
  if (!cs1->tag()) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   EC::kCapExTagViolation, cs1);
    return;
  }
  if (cs1->IsSealed()) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   EC::kCapExSealViolation, cs1);
    return;
  }
  if (!cs1->HasPermission(CapReg::kPermitStore)) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   EC::kCapExPermitStoreViolation, cs1);
    return;
  }
  if (!cs1->HasPermission(CapReg::kPermitLoadStoreCapability) && cs2->tag()) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   EC::kCapExPermitStoreCapabilityViolation,
                                   cs1);
    return;
  }
  if (!cs1->HasPermission(CapReg::kPermitStoreLocalCapability) && cs2->tag() &&
      (!cs2->HasPermission(CapReg::kPermitGlobal) || cs2->IsBackwardSentry())) {
    tag = 0;
  }
  if (!cs1->IsInBounds(address, CapReg::kCapabilitySizeInBytes)) {
    state->HandleCheriRegException(instruction, instruction->address(),
                                   EC::kCapExBoundsViolation, cs1);
    return;
  }
  if ((address & ((1 << CapReg::kGranuleShift) - 1)) != 0) {
    state->Trap(/*is_interrupt*/ false, address,
                *riscv::ExceptionCode::kStoreAddressMisaligned,
                instruction->address(), instruction);
    return;
  }
  auto* db = state->db_factory()->Allocate(CapReg::kCapabilitySizeInBytes);
  auto* tag_db = state->db_factory()->Allocate(1);
  db->Set<uint32_t>(0, cs2->address());
  db->Set<uint32_t>(1, cs2->Compress());
  tag_db->Set<uint8_t>(0, tag);
  state->StoreCapability(instruction, address, db, tag_db);
  db->DecRef();
  tag_db->DecRef();
}

void CheriotCSeal(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto* cs2 = GetCapSource(instruction, 1);
  auto* cd = GetCapDest(instruction, 0);
  bool valid = true;
  // If cs1 is sealed, invalidate cd.
  if (cs1->IsSealed()) valid = false;
  uint32_t object_type = cs2->address();
  bool permitted_otype = false;
  switch (object_type) {
    case CapReg::kInterruptInheritingSentry:
    case CapReg::kInterruptDisablingForwardSentry:
    case CapReg::kInterruptEnablingForwardSentry:
    case CapReg::kInterruptDisablingBackwardSentry:
    case CapReg::kInterruptEnablingBackwardSentry:
    case CapReg::kSealedExecutable6:
    case CapReg::kSealedExecutable7:
      permitted_otype = cs1->HasPermission(CapReg::kPermitExecute);
      break;
    default:
      permitted_otype = !cs1->HasPermission(CapReg::kPermitExecute) &&
                        (object_type > 8) && (object_type <= 15);
      break;
  }
  bool permitted = cs2->tag() && cs2->HasPermission(CapReg::kPermitSeal) &&
                   (object_type < cs2->top()) && (object_type >= cs2->base()) &&
                   permitted_otype && cs2->IsUnsealed();
  auto cs2_address = cs2->address();
  cd->CopyFrom(*cs1);
  cd->set_object_type(
      cs2_address &
      ((1 << (CapReg::kObjectType[0] - CapReg::kObjectType[1] + 1))) - 1);
  if (!permitted || !valid) cd->Invalidate();
}

void CheriotCSetAddr(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto rs2 = generic::GetInstructionSource<uint32_t>(instruction, 1);
  auto* cd = GetCapDest(instruction, 0);
  auto valid = true;
  if (cs1->IsSealed()) valid = false;
  cd->CopyFrom(*cs1);
  cd->SetAddress(rs2);
  if (!cd->IsRepresentable() || !valid) cd->Invalidate();
}

void CheriotCSetBounds(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto rs2 = generic::GetInstructionSource<uint32_t>(instruction, 1);
  auto* cd = GetCapDest(instruction, 0);
  auto cs1_address = cs1->address();
  bool valid = true;
  // If cs1 is sealed, then invalidate the capability.
  if (cs1->IsSealed()) valid = false;
  // If the bounds are such that the new requested capability is not
  // representable, invalidate.
  auto [cs1_base, cs1_top] = cs1->ComputeBounds();
  auto new_top =
      static_cast<uint64_t>(cs1_address) + static_cast<uint64_t>(rs2);
  valid &= (cs1_address >= cs1_base) && (new_top <= cs1_top);
  cd->CopyFrom(*cs1);
  (void)cd->SetBounds(cs1_address, rs2);
  if (!valid) cd->Invalidate();
}

void CheriotCSetBoundsExact(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto rs2 = generic::GetInstructionSource<uint32_t>(instruction, 1);
  auto* cd = GetCapDest(instruction, 0);
  bool valid = true;
  auto cs1_address = cs1->address();
  // If cs1 is sealed, then invalidate the capability.
  if (cs1->IsSealed()) valid = false;
  // If outside the requested representable range, invalidate.
  auto [cs1_base, cs1_top] = cs1->ComputeBounds();
  auto new_top =
      static_cast<uint64_t>(cs1_address) + static_cast<uint64_t>(rs2);
  valid &= (cs1_address >= cs1_base) && (new_top <= cs1_top);
  // Invalidate if not exact.
  cd->CopyFrom(*cs1);
  bool exact = cd->SetBounds(cs1_address, rs2);
  if (!exact || !valid) cd->Invalidate();
}

void CheriotCSetEqualExact(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto* cs2 = GetCapSource(instruction, 1);
  uint32_t equal =
      (cs1->tag() == cs2->tag()) && (cs1->Compress() == cs2->Compress());
  WriteCapIntResult(instruction, 0, equal);
}

void CheriotCSetHigh(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto rs2 = generic::GetInstructionSource<uint32_t>(instruction, 1);
  auto* cd = GetCapDest(instruction, 0);
  cd->Expand(cs1->address(), rs2, false);
}

void CheriotCSpecialR(const Instruction* instruction) {
  auto* state = static_cast<CheriotState*>(instruction->state());
  // Decode will ensure that register scr is valid.
  auto* scr = GetCapSource(instruction, 0);
  auto* cd = GetCapDest(instruction, 0);
  if (!state->pcc()->HasPermission(CapReg::kPermitAccessSystemRegisters)) {
    state->HandleCheriRegException(
        instruction, instruction->address(),
        EC::kCapExPermitAccessSystemRegistersViolation, state->pcc());
    return;
  }
  cd->CopyFrom(*scr);
}

void CheriotCSpecialRW(const Instruction* instruction) {
  auto* state = static_cast<CheriotState*>(instruction->state());
  // Decode will ensure that register scr is valid.
  auto* cs1 = GetCapSource(instruction, 0);
  auto* scr = GetCapSource(instruction, 1);
  auto* cd = GetCapDest(instruction, 0);
  if (!state->pcc()->HasPermission(CapReg::kPermitAccessSystemRegisters)) {
    state->HandleCheriRegException(
        instruction, instruction->address(),
        EC::kCapExPermitAccessSystemRegistersViolation, state->pcc());
    return;
  }
  auto* temp_reg = state->temp_reg();
  temp_reg->CopyFrom(*cs1);
  cd->CopyFrom(*scr);
  // If it's the mepcc register, make sure to clear any lsb.
  if (scr->name() == "mepcc") {
    if (temp_reg->address() & 0x1ULL) {
      temp_reg->set_address(temp_reg->address() & ~0x1);
      temp_reg->Invalidate();
    } else if (temp_reg->IsSealed() ||
               !temp_reg->HasPermission(CapReg::kPermitExecute)) {
      temp_reg->Invalidate();
    }
  } else if (scr->name() == "mtcc") {
    if (temp_reg->address() & 0x3ULL) {
      temp_reg->set_address(temp_reg->address() & ~0x3ULL);
      temp_reg->Invalidate();
    } else if (temp_reg->IsSealed() ||
               !temp_reg->HasPermission(CapReg::kPermitExecute)) {
      temp_reg->Invalidate();
    }
  }
  scr->CopyFrom(*state->temp_reg());
}

void CheriotCSub(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto* cs2 = GetCapSource(instruction, 1);
  WriteCapIntResult(instruction, 0, cs1->address() - cs2->address());
}

void CheriotCTestSubset(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto* cs2 = GetCapSource(instruction, 1);
  auto [cs1_base, cs1_top] = cs1->ComputeBounds();
  auto [cs2_base, cs2_top] = cs2->ComputeBounds();
  // Verify that cs2 is a subset of cs1.
  bool result =
      cs1->tag() == cs2->tag() &&
      // cs2 has a valid range smaller or equal to cs1.
      cs1_base <= cs2_base && cs1_top >= cs2_top &&
      // cs2 permissions are a subset of cs1 permissions.
      ((cs1->permissions() & cs2->permissions()) == cs2->permissions());
  WriteCapIntResult(instruction, 0, static_cast<uint32_t>(result));
}

void CheriotCUnseal(const Instruction* instruction) {
  auto* cs1 = GetCapSource(instruction, 0);
  auto* cs2 = GetCapSource(instruction, 1);
  auto* cd = GetCapDest(instruction, 0);
  bool valid = true;
  if (!cs2->tag() || !cs1->IsSealed() || cs2->IsSealed() ||
      (cs2->address() != cs1->object_type()) ||
      !cs2->HasPermission(CapReg::kPermitUnseal) ||
      (cs2->address() < cs2->base()) || (cs2->address() >= cs2->top())) {
    valid = false;
  }
  auto cs2_permissions = cs2->permissions();
  cd->CopyFrom(*cs1);
  if ((cs2_permissions & CapReg::kPermitGlobal) == 0) {
    cd->ClearPermissions(CapReg::kPermitGlobal);
  }
  cd->set_object_type(CapReg::kUnsealed);
  if (!valid) cd->Invalidate();
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
