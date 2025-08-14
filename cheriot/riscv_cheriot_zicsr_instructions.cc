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

#include "cheriot/riscv_cheriot_zicsr_instructions.h"

#include <any>
#include <cstdint>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_csr_enum.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/register.h"
#include "mpact/sim/generic/type_helpers.h"
#include "riscv//riscv_csr.h"
#include "riscv//riscv_state.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::operator*;  // NOLINT: is used below (clang error).
using ::mpact::sim::riscv::PrivilegeMode;
using PB = ::mpact::sim::cheriot::CheriotRegister::PermissionBits;
using RV_EC = ::mpact::sim::riscv::ExceptionCode;
using CH_EC = ::mpact::sim::cheriot::ExceptionCode;
using CapReg = ::mpact::sim::cheriot::CheriotRegister;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::generic::RegisterBase;

// Helper to get capability destination registers.
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

template <typename T>
inline T ReadCsr(RiscVCsrInterface*) {}

template <>
inline uint32_t ReadCsr<uint32_t>(RiscVCsrInterface* csr) {
  return csr->AsUint32();
}
template <>
inline uint64_t ReadCsr<uint64_t>(RiscVCsrInterface* csr) {
  return csr->AsUint64();
}

// Helper function to check that the CSR permission is valid. If not, throw
// an illegal instruction exception.
bool CheckCsrPermission(int csr_index, Instruction* instruction,
                        bool is_write) {
  auto required_mode = (csr_index >> 8) & 0x3;
  auto* state = static_cast<CheriotState*>(instruction->state());
  auto current_mode = PrivilegeMode::kMachine;
  // If the register isn't available in CHERIoT, throw an exception.
  if (required_mode == *PrivilegeMode::kSupervisor) {
    state->Trap(/*is_interrupt*/ false, 0, *RV_EC::kIllegalInstruction,
                instruction->address(), instruction);
    return false;
  }
  // If the privilege mode is too low, throw an exception.
  if (*current_mode < required_mode) {
    state->Trap(/*is_interrupt*/ false, 0, *RV_EC::kIllegalInstruction,
                instruction->address(), instruction);
    return false;
  }
  // Accesses to fflags, frm, and fcsr are all ok.
  if ((csr_index >= *RiscVCheriotCsrEnum::kFFlags) &&
      (csr_index <= *RiscVCheriotCsrEnum::kFCsr)) {
    return true;
  }
  // Reads to MCycle, MInstret, MHpmcounterN are all ok.
  if (!is_write && (((csr_index >= *RiscVCheriotCsrEnum::kMCycle) &&
                     (csr_index <= *RiscVCheriotCsrEnum::kMHpmcounter31)) ||
                    ((csr_index >= *RiscVCheriotCsrEnum::kMCycleH) &&
                     (csr_index <= *RiscVCheriotCsrEnum::kMHpmcounter31H)))) {
    return true;
  }
  // Check pcc capability.
  if ((required_mode != *PrivilegeMode::kUser) &&
      (!state->pcc()->HasPermission(PB::kPermitAccessSystemRegisters))) {
    state->HandleCheriRegException(
        instruction, instruction->address(),
        CH_EC::kCapExPermitAccessSystemRegistersViolation, state->pcc());
    return false;
  }
  return true;
}

// Templated helper functions.

// Read the CSR, write a new value back.
template <typename T>
static inline void RVZiCsrrw(Instruction* instruction) {
  // Get a handle to the state instance.
  auto state = static_cast<CheriotState*>(instruction->state());
  // Get the csr index.
  int csr_index = instruction->Source(1)->AsInt32(0);
  if (!CheckCsrPermission(csr_index, instruction, /*is_write=*/true)) return;
  // Read the csr.
  auto result = state->csr_set()->GetCsr(csr_index);
  if (!result.ok()) {
    // Signal error if it failed.
    LOG(ERROR) << absl::StrCat("Instruction at address 0x",
                               absl::Hex(instruction->address()),
                               " failed to read CSR 0x", absl::Hex(csr_index),
                               ": ", result.status().message());
    return;
  }
  // Get the new value.
  T new_value = generic::GetInstructionSource<T>(instruction, 0);
  auto* csr = result.value();
  // Update the register.
  auto csr_val = ReadCsr<T>(csr);
  WriteCapIntResult(instruction, 0, csr_val);
  // Write the new value to the csr.
  csr->Write(new_value);
}

// Read the CSR, set the bits specified by the new value and write back.
template <typename T>
static inline void RVZiCsrrs(Instruction* instruction) {
  // Get a handle to the state instance.
  auto state = static_cast<CheriotState*>(instruction->state());
  // Get the csr index.
  int csr_index = instruction->Source(1)->AsInt32(0);
  if (!CheckCsrPermission(csr_index, instruction, /*is_write=*/true)) return;
  // Read the csr.
  auto result = state->csr_set()->GetCsr(csr_index);
  if (!result.ok()) {
    // Signal error if it failed.
    LOG(ERROR) << absl::StrCat("Instruction at address 0x",
                               absl::Hex(instruction->address()),
                               " failed to read CSR 0x", absl::Hex(csr_index),
                               ": ", result.status().message());
    return;
  }
  // Get the new value.
  T new_value = generic::GetInstructionSource<T>(instruction, 0);
  auto* csr = result.value();
  // Update the register.
  auto csr_val = ReadCsr<T>(csr);
  WriteCapIntResult(instruction, 0, csr_val);
  // Write the new value to the csr.
  csr->SetBits(new_value);
}

// Read the CSR, clear the bits specified by the new value and write back.
template <typename T>
static inline void RVZiCsrrc(Instruction* instruction) {
  // Get a handle to the state instance.
  auto state = static_cast<CheriotState*>(instruction->state());
  // Get the csr index.
  int csr_index = instruction->Source(1)->AsInt32(0);
  if (!CheckCsrPermission(csr_index, instruction, /*is_write=*/true)) return;
  // Read the csr.
  auto result = state->csr_set()->GetCsr(csr_index);
  if (!result.ok()) {
    // Signal error if it failed.
    LOG(ERROR) << absl::StrCat("Instruction at address 0x",
                               absl::Hex(instruction->address()),
                               " failed to read CSR 0x", absl::Hex(csr_index),
                               ": ", result.status().message());
    return;
  }
  // Get the new value.
  T new_value = generic::GetInstructionSource<T>(instruction, 0);
  auto* csr = result.value();
  // Write the current value of the CSR to the destination register.
  auto csr_val = ReadCsr<T>(csr);
  WriteCapIntResult(instruction, 0, csr_val);
  // Write the new value to the csr.
  csr->ClearBits(new_value);
}

// Do not read the CSR, just write the new value back.
template <typename T>
static inline void RVZiCsrrwNr(Instruction* instruction) {
  // Get a handle to the state instance.
  auto state = static_cast<CheriotState*>(instruction->state());
  // Get the csr index.
  int csr_index = instruction->Source(1)->AsInt32(0);
  if (!CheckCsrPermission(csr_index, instruction, /*is_write=*/true)) return;
  // Read the csr.
  auto result = state->csr_set()->GetCsr(csr_index);
  if (!result.ok()) {
    LOG(ERROR) << absl::StrCat("Instruction at address 0x",
                               absl::Hex(instruction->address()),
                               " failed to write CSR 0x", absl::Hex(csr_index),
                               ": ", result.status().message());
    return;
  }
  auto* csr = result.value();
  // Write the new value to the csr.
  T new_value = generic::GetInstructionSource<T>(instruction, 0);
  csr->Write(new_value);
}

// Do not write a value back to the CSR, just read it.
template <typename T>
static inline void RVZiCsrrNw(Instruction* instruction) {
  // Get a handle to the state instance.
  auto state = static_cast<CheriotState*>(instruction->state());
  // Get the csr index.
  int csr_index = instruction->Source(0)->AsInt32(0);
  if (!CheckCsrPermission(csr_index, instruction, /*is_write=*/false)) return;
  // Read the csr.
  auto result = state->csr_set()->GetCsr(csr_index);
  if (!result.ok()) {
    LOG(ERROR) << absl::StrCat("Instruction at address 0x",
                               absl::Hex(instruction->address()),
                               " failed to read CSR 0x", absl::Hex(csr_index),
                               ": ", result.status().message());
    return;
  }
  // Get the CSR object.
  auto* csr = result.value();
  auto csr_val = ReadCsr<T>(csr);
  WriteCapIntResult(instruction, 0, csr_val);
}

using RegisterType = CheriotRegister;
using UintReg = RegisterType::ValueType;

// Read the CSR, write a new value back.
void RiscVZiCsrrw(Instruction* instruction) { RVZiCsrrw<UintReg>(instruction); }

// Read the CSR, set the bits specified by the new value and write back.
void RiscVZiCsrrs(Instruction* instruction) { RVZiCsrrs<UintReg>(instruction); }

// Read the CSR, clear the bits specified by the new value and write back.
void RiscVZiCsrrc(Instruction* instruction) { RVZiCsrrc<UintReg>(instruction); }

// Do not read the CSR, just write the new value back.
void RiscVZiCsrrwNr(Instruction* instruction) {
  RVZiCsrrwNr<UintReg>(instruction);
}

// Do not write a value back to the CSR, just read it.
void RiscVZiCsrrNw(Instruction* instruction) {
  RVZiCsrrNw<UintReg>(instruction);
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
