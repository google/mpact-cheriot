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

#include "cheriot/cheriot_state.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/riscv_cheriot_csr_enum.h"
#include "cheriot/riscv_cheriot_minstret.h"
#include "mpact/sim/generic/arch_state.h"
#include "mpact/sim/generic/type_helpers.h"
#include "mpact/sim/util/memory/memory_interface.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"
#include "mpact/sim/util/memory/tagged_memory_interface.h"
#include "riscv//riscv_csr.h"
#include "riscv//riscv_misa.h"
#include "riscv//riscv_state.h"

ABSL_FLAG(uint64_t, revocation_ram_base, 0x8000'0000,
          "Ram base for revocation.");
ABSL_FLAG(uint64_t, revocation_mem_base, 0x8300'0000,
          "Revocation memory base.");

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::operator*;  // NOLINT: used below (clang error).
using ::mpact::sim::riscv::IsaExtension;
using ::mpact::sim::riscv::RiscVXlen;
using EC = ::mpact::sim::riscv::ExceptionCode;
using ::mpact::sim::riscv::RiscVCsrEnum;
using ::mpact::sim::riscv::RiscVCsrInterface;
using ::mpact::sim::riscv::RiscVSimpleCsr;

// These helper templates are used to store information about the CSR registers
// used in CHERIoT RiscV (32 bits).
template <typename T>
struct CsrInfo {};

template <>
struct CsrInfo<uint32_t> {
  using T = uint32_t;
  static constexpr T kMhartidRMask = std::numeric_limits<T>::max();
  static constexpr T kMhartidWMask = 0;
  static constexpr T kMstatusInitialValue = 0x1800;
  static constexpr T kMisaInitialValue =
      (*RiscVXlen::RV32 << 30) | *IsaExtension::kIntegerMulDiv |
      *IsaExtension::kRVIBaseIsa | *IsaExtension::kGExtension |
      *IsaExtension::kSinglePrecisionFp | *IsaExtension::kDoublePrecisionFp |
      *IsaExtension::kCompressed | *IsaExtension::kAtomic |
      *IsaExtension::kSupervisorMode;
  static constexpr T kMisaRMask = 0xc3ff'ffff;
  static constexpr T kMisaWMask = 0x0;
};

// Three templated helper functions used to create individual CSRs.

// This creates the CSR and assigns it to a pointer in the state object. Type
// can be inferred from the state object pointer.
template <typename T, typename... Ps>
T *CreateCsr(CheriotState *state, T *&ptr,
             std::vector<RiscVCsrInterface *> &csr_vec, Ps... pargs) {
  auto *csr = new T(pargs...);
  auto result = state->csr_set()->AddCsr(csr);
  if (!result.ok()) {
    LOG(ERROR) << absl::StrCat("Failed to add csr '", csr->name(),
                               "': ", result.message());
    delete csr;
    return nullptr;
  }
  csr_vec.push_back(csr);
  ptr = csr;
  return csr;
}

// This creates the CSR and assigns it to a pointer in the state object, however
// that pointer is of abstract type, so the CSR type cannot be inferred, but
// has to be specified in the call.
template <typename T, typename... Ps>
T *CreateCsr(CheriotState *state, RiscVCsrInterface *&ptr,
             std::vector<RiscVCsrInterface *> &csr_vec, Ps... pargs) {
  auto *csr = new T(pargs...);
  auto result = state->csr_set()->AddCsr(csr);
  if (!result.ok()) {
    LOG(ERROR) << absl::StrCat("Failed to add csr '", csr->name(),
                               "': ", result.message());
    delete csr;
    return nullptr;
  }
  csr_vec.push_back(csr);
  ptr = csr;
  return csr;
}

// This creates the CSR, but does not assign it to a pointer in the state
// object. That means the type cannot be inferred, but has to be specified
// in the call.
template <typename T, typename... Ps>
T *CreateCsr(CheriotState *state, std::vector<RiscVCsrInterface *> &csr_vec,
             Ps... pargs) {
  auto *csr = new T(pargs...);
  auto result = state->csr_set()->AddCsr(csr);
  if (!result.ok()) {
    LOG(ERROR) << absl::StrCat("Failed to add csr '", csr->name(),
                               "': ", result.message());
    delete csr;
    return nullptr;
  }
  csr_vec.push_back(csr);
  return csr;
}

// Templated helper function that is used to create the set of CSRs needed
// for simulation.
template <typename T>
void CreateCsrs(CheriotState *state,
                std::vector<RiscVCsrInterface *> &csr_vec) {
  absl::Status result;
  // Create CSRs.
  // misa
  auto *misa = CreateCsr(state, state->misa_, csr_vec,
                         CsrInfo<T>::kMisaInitialValue, state);
  CHECK_NE(misa, nullptr);
  // mtvec is replaced by mtcc

  // mcause
  CHECK_NE(
      CreateCsr<RiscVSimpleCsr<T>>(state, state->mcause_, csr_vec, "mcause",
                                   RiscVCsrEnum::kMCause, 0, state),
      nullptr);

  // Mip and Mie are always 32 bit.
  // mip
  auto *mip = CreateCsr(state, state->mip_, csr_vec, 0, state);
  CHECK_NE(mip, nullptr);

  // mie
  auto *mie = CreateCsr(state, state->mie_, csr_vec, 0, state);
  CHECK_NE(mie, nullptr);

  // mhartid
  CHECK_NE(CreateCsr<RiscVSimpleCsr<T>>(
               state, csr_vec, "mhartid", *RiscVCheriotCsrEnum::kMHartId, 0,
               CsrInfo<T>::kMhartidRMask, CsrInfo<T>::kMhartidWMask, state),
           nullptr);

  // mepc is replaced by mepcc

  // mscratch
  CHECK_NE(CreateCsr<RiscVSimpleCsr<T>>(state, csr_vec, "mscratch",
                                        *RiscVCsrEnum::kMScratch, 0, state),
           nullptr);

  // medeleg - machine mode exception delegation register. Not used.

  // mideleg - machine mode interrupt delegation register. Not used.

  // mstatus
  auto *mstatus =
      CreateCsr(state, state->mstatus_, csr_vec,
                CsrInfo<uint32_t>::kMstatusInitialValue, state, misa);
  CHECK_NE(mstatus, nullptr);
  // mtval
  auto *mtval = CreateCsr<RiscVSimpleCsr<T>>(
      state, state->mtval_, csr_vec, "mtval", *RiscVCsrEnum::kMTval, 0, state);
  CHECK_NE(mtval, nullptr);

  // minstret/minstreth
  CHECK_NE(CreateCsr<RiscVCheriotMInstret>(state, csr_vec, "minstret", state),
           nullptr);
  CHECK_NE(CreateCsr<RiscVCheriotMInstreth>(state, csr_vec, "minstreth", state),
           nullptr);

  // Stack high water mark CSRs. Mshwm gets updated automatically during
  // execution. mshwm
  auto *mshwm = CreateCsr<RiscVSimpleCsr<T>>(
      state, state->mshwm_, csr_vec, "mshwm", *RiscVCheriotCsrEnum::kMshwm,
      /*initial_value=*/0,
      /*read_mask=*/0xffff'fff0, /*write_mask=*/0xffff'fff0, state);
  CHECK_NE(mshwm, nullptr);
  // mshwmb
  auto *mshwmb = CreateCsr<RiscVSimpleCsr<T>>(
      state, state->mshwmb_, csr_vec, "mshwmb", *RiscVCheriotCsrEnum::kMshwmb,
      /*initial_value=*/0,
      /*read_mask=*/0xffff'fff0, /*write_mask=*/0xffff'fff0, state);
  CHECK_NE(mshwmb, nullptr);

  // mccsr.
  auto *mccsr = CreateCsr<RiscVSimpleCsr<T>>(
      state, csr_vec, "mccsr", *RiscVCheriotCsrEnum::kMCcsr,
      /*initial_value=*/0x3,
      /*read_mask=*/0x3, /*write_mask=*/0x2, state);
  CHECK_NE(mccsr, nullptr);

  // Supervisor level CSRs
  // None in CHERIoT.

  // User level CSRs
  // None in CHERIoT.

  // Simulator CSRs

  // Access current privilege mode. Omitted.
}

// This value is in the RV32ISA manual to support MMU, although in "BARE" mode
// only the bottom 32-bit is valid.
constexpr uint64_t kRiscv32MaxMemorySize = 0x3f'ffff'ffffULL;

CheriotState::CheriotState(std::string_view id,
                           util::TaggedMemoryInterface *memory,
                           util::AtomicMemoryOpInterface *atomic_memory)
    : generic::ArchState(id),
      tagged_memory_(memory),
      atomic_tagged_memory_(atomic_memory) {
  for (auto &[name, index] : std::vector<std::pair<std::string, unsigned>>{
           {"c0", 0b0'00000},   {"c1", 0b0'00001},   {"c2", 0b0'00010},
           {"c3", 0b0'00011},   {"c4", 0b0'00100},   {"c5", 0b0'00101},
           {"c6", 0b0'00110},   {"c7", 0b0'00111},   {"c8", 0b0'01000},
           {"c9", 0b0'01001},   {"c10", 0b0'01010},  {"c11", 0b0'01011},
           {"c12", 0b0'01100},  {"c13", 0b0'01101},  {"c14", 0b0'01110},
           {"c15", 0b0'01111},  {"c16", 0b0'10000},  {"c17", 0b0'10001},
           {"c18", 0b0'10010},  {"c19", 0b0'10011},  {"c20", 0b0'10100},
           {"c21", 0b0'10101},  {"c22", 0b0'10110},  {"c23", 0b0'10111},
           {"c24", 0b0'11000},  {"c25", 0b0'11001},  {"c26", 0b0'11010},
           {"c27", 0b0'11011},  {"c28", 0b0'11100},  {"c29", 0b0'11101},
           {"c30", 0b0'11110},  {"c31", 0b0'11111},  {"pcc", 0b1'00000},
           {"mtcc", 0b1'11100}, {"mtdc", 0b1'11101}, {"mscratchc", 0b1'11110},
           {"mepcc", 0b1'11111}}) {
    cap_index_map_.emplace(name, index);
  }
  // Create root capabilities and the special capability CSRs.
  executable_root_ = new CheriotRegister(this, "executable_root");
  executable_root_->ResetExecuteRoot();
  sealing_root_ = new CheriotRegister(this, "sealing_root");
  sealing_root_->ResetSealingRoot();
  memory_root_ = new CheriotRegister(this, "memory_root");
  memory_root_->ResetMemoryRoot();
  mtcc_ = AddRegister<CheriotRegister>("mtcc");
  mtcc_->ResetExecuteRoot();
  mepcc_ = AddRegister<CheriotRegister>("mepcc");
  mepcc_->ResetExecuteRoot();
  mtdc_ = AddRegister<CheriotRegister>("mtdc");
  mtdc_->ResetMemoryRoot();
  mscratchc_ = AddRegister<CheriotRegister>("mscratchc");
  mscratchc_->ResetSealingRoot();
  pcc_ = AddRegister<CheriotRegister>("pcc");
  auto status = AddRegisterAlias<CheriotRegister>("pcc", "pc");
  if (!status.ok()) {
    LOG(ERROR) << "Error creating pc alias of 'pcc': " << status.message();
    return;
  }
  pcc_->ResetExecuteRoot();
  temp_reg_ = new CheriotRegister(this, "temp_reg");
  // Add the general capability registers.
  for (int i = 0; i < 32; i++) {
    AddRegister<CheriotRegister>(absl::StrCat("c", i));
  }
  status = AddRegisterAlias<CheriotRegister>("c3", "cgp");
  if (!status.ok()) {
    LOG(ERROR) << "Error creating cgp alias of 'c3': " << status.message();
    return;
  }
  auto [cgp_reg, unused] = GetRegister<CheriotRegister>("cgp");
  cgp_ = cgp_reg;
  // Create the other CSRs.
  csr_set_ = new RiscVCsrSet();
  CreateCsrs<uint32_t>(this, csr_vec_);
  if (tagged_memory_ == nullptr) {
    owned_tagged_memory_ = new util::TaggedFlatDemandMemory(
        CheriotRegister::kCapabilitySizeInBytes);
    tagged_memory_ = owned_tagged_memory_;
  }
  pc_src_operand_ = new RiscVCheri32PcSourceOperand(this);
  set_pc_operand(pc_src_operand_);
  // Create the revocation data buffer.
  revocation_db_ = db_factory()->Allocate<uint8_t>(1);
  revocation_ram_base_ = absl::GetFlag(FLAGS_revocation_ram_base);
  revocation_mem_base_ = absl::GetFlag(FLAGS_revocation_mem_base);

  set_max_physical_address(kRiscv32MaxMemorySize);
}

CheriotState::~CheriotState() {
  delete executable_root_;
  delete sealing_root_;
  delete memory_root_;
  delete owned_tagged_memory_;
  delete pc_src_operand_;
  for (auto *csr : csr_vec_) delete csr;
  delete csr_set_;
  delete temp_reg_;
  revocation_db_->DecRef();
}

void CheriotState::Reset() {
  for (auto &[unused, reg_ptr] : *registers()) {
    reg_ptr->data_buffer()->Set<uint32_t>(0, 0);
  }
  // Reset CSRs.
  pcc_->ResetExecuteRoot();
  mtcc_->ResetExecuteRoot();
  mepcc_->ResetExecuteRoot();
  mtdc_->ResetMemoryRoot();
  mscratchc_->ResetSealingRoot();
  mstatus_->Set(CsrInfo<uint32_t>::kMstatusInitialValue);
  mtval_->Set(0UL);
  mshwm_->Set(0UL);
  mshwmb_->Set(0UL);
  mip_->Set(0UL);
  mie_->Set(0UL);
  csr_set_->GetCsr("minstret").value()->Set(0UL);
  csr_set_->GetCsr("minstreth").value()->Set(0UL);
  csr_set_->GetCsr("mcause").value()->Set(0UL);
  csr_set_->GetCsr("misa").value()->Set(CsrInfo<uint32_t>::kMisaInitialValue);
}

void CheriotState::HandleCheriRegException(const Instruction *instruction,
                                           uint64_t epc, ExceptionCode code,
                                           const CheriotRegister *reg) {
  // Map the CHERIoT exception to a RiscV trap.
  unsigned mcause = kCheriExceptionCode;
  uint32_t mtval = *code & 0b1'1111;
  auto const &name = reg->name();
  auto iter = cap_index_map_.find(name);
  uint32_t cap_index = 0x1f;  // Set to a default value.
  if (iter != cap_index_map_.end()) {
    cap_index = iter->second;
  }
  mtval |= cap_index << 5;
  Trap(/*is_interrupt*/ false, mtval, mcause, epc, instruction);
}

void CheriotState::set_max_physical_address(uint64_t max_physical_address) {
  max_physical_address_ = std::min(max_physical_address, kRiscv32MaxMemorySize);
}

void CheriotState::set_min_physical_address(uint64_t min_physical_address) {
  min_physical_address_ = std::min(min_physical_address, max_physical_address_);
}

void CheriotState::LoadCapability(const Instruction *instruction,
                                  uint32_t address, DataBuffer *db,
                                  DataBuffer *tags, Instruction *child,
                                  CapabilityLoadContext32 *context) {
  // Check for alignment.
  uint64_t mask = db->size<uint8_t>() - 1;
  if ((address & mask) != 0) {
    Trap(/*is_interrupt*/ false, address, *EC::kLoadAddressMisaligned,
         instruction == nullptr ? 0 : instruction->address(), instruction);
    return;
  }
  // Check for physical address violation.
  if (address < min_physical_address_ || address > max_physical_address_) {
    Trap(/*is_interrupt*/ false, address, *EC::kLoadAccessFault,
         instruction == nullptr ? 0 : instruction->address(), instruction);
    return;
  }
  // Forward the load.
  tagged_memory_->Load(address, db, tags, child, context);
  if (!tracing_active_) return;
  load_address_ = address;
  load_db_ = db;
  load_db_->IncRef();
  load_tags_ = tags;
  load_tags_->IncRef();
}

void CheriotState::StoreCapability(const Instruction *instruction,
                                   uint32_t address, DataBuffer *db,
                                   DataBuffer *tags) {
  // Check for alignment.
  uint64_t mask = db->size<uint8_t>() - 1;
  if ((address & mask) != 0) {
    Trap(/*is_interrupt*/ false, address, *EC::kStoreAddressMisaligned,
         instruction == nullptr ? 0 : instruction->address(), instruction);
    return;
  }
  // Check for physical address violation.
  if (address < min_physical_address_ || address > max_physical_address_) {
    Trap(/*is_interrupt*/ false, address, *EC::kStoreAccessFault,
         instruction == nullptr ? 0 : instruction->address(), instruction);
    return;
  }
  // Check for stack accesses relative to mshwm/mshwmb.
  if ((address >= mshwmb_->GetUint32()) && (address < mshwm_->GetUint32())) {
    mshwm_->Set(address);
  }
  // Forward the store.
  tagged_memory_->Store(address, db, tags);
  if (!tracing_active_) return;
  store_address_ = address;
  store_db_ = db;
  store_db_->IncRef();
  store_tags_ = tags;
  store_tags_->IncRef();
}

void CheriotState::LoadMemory(const Instruction *inst, uint64_t address,
                              DataBuffer *db, Instruction *child_inst,
                              ReferenceCount *context) {
  // Check for alignment.
  uint64_t mask = db->size<uint8_t>() - 1;
  if ((address & mask) != 0) {
    Trap(/*is_interrupt*/ false, address, *EC::kLoadAddressMisaligned,
         inst == nullptr ? 0 : inst->address(), inst);
    return;
  }
  // Check for physical address violation.
  if (address < min_physical_address_ || address > max_physical_address_) {
    Trap(/*is_interrupt*/ false, address, *EC::kLoadAccessFault,
         inst == nullptr ? 0 : inst->address(), inst);
    return;
  }
  // Forward the load.
  tagged_memory_->Load(address, db, child_inst, context);
  if (!tracing_active_) return;
  load_address_ = address;
  load_db_ = db;
  load_db_->IncRef();
  load_tags_ = nullptr;
}

void CheriotState::LoadMemory(const Instruction *inst, DataBuffer *address_db,
                              DataBuffer *mask_db, int el_size, DataBuffer *db,
                              Instruction *child_inst,
                              ReferenceCount *context) {
  // Check for alignment.
  uint64_t mask = el_size - 1;
  for (auto address : address_db->Get<uint64_t>()) {
    if ((address & mask) != 0) {
      Trap(/*is_interrupt*/ false, address, *EC::kLoadAddressMisaligned,
           inst == nullptr ? 0 : inst->address(), inst);
      return;
    }
  }
  // Check for physical address violation.
  for (auto address : address_db->Get<uint64_t>()) {
    if (address < min_physical_address_ || address > max_physical_address_) {
      Trap(/*is_interrupt*/ false, address, *EC::kLoadAccessFault,
           inst == nullptr ? 0 : inst->address(), inst);
      return;
    }
  }
  // Forward the load.
  tagged_memory_->Load(address_db, mask_db, el_size, db, child_inst, context);
}

void CheriotState::StoreMemory(const Instruction *inst, uint64_t address,
                               DataBuffer *db) {
  // Check for alignment.
  uint64_t mask = db->size<uint8_t>() - 1;
  if ((address & mask) != 0) {
    Trap(/*is_interrupt*/ false, address, *EC::kStoreAddressMisaligned,
         inst == nullptr ? 0 : inst->address(), inst);
    return;
  }
  // Check for physical address violation.
  if (address < min_physical_address_ || address > max_physical_address_) {
    Trap(/*is_interrupt*/ false, address, *EC::kStoreAccessFault,
         inst == nullptr ? 0 : inst->address(), inst);
    return;
  }
  // Check for stack accesses relative to mshwm/mshwmb.
  uint32_t address32 = static_cast<uint32_t>(address);
  if ((address32 >= mshwmb_->GetUint32()) &&
      (address32 < mshwm_->GetUint32())) {
    mshwm_->Set(address32);
  }
  // Forward the store.
  tagged_memory_->Store(address, db);
  if (!tracing_active_) return;
  store_address_ = address;
  store_db_ = db;
  store_db_->IncRef();
  store_tags_ = nullptr;
}

void CheriotState::StoreMemory(const Instruction *inst, DataBuffer *address_db,
                               DataBuffer *mask_db, int el_size,
                               DataBuffer *db) {
  // Check for alignment.
  uint64_t mask = el_size - 1;
  for (auto address : address_db->Get<uint64_t>()) {
    if ((address & mask) != 0) {
      Trap(/*is_interrupt*/ false, address, *EC::kStoreAddressMisaligned,
           inst == nullptr ? 0 : inst->address(), inst);
      return;
    }
  }
  // Check for physical address violation.
  for (auto address : address_db->Get<uint64_t>()) {
    if (address < min_physical_address_ || address > max_physical_address_) {
      Trap(/*is_interrupt*/ false, address, *EC::kStoreAccessFault,
           inst == nullptr ? 0 : inst->address(), inst);
      return;
    }
  }
  // Check for stack accesses relative to mshwm/mshwmb.
  for (auto address : address_db->Get<uint64_t>()) {
    uint32_t address32 = static_cast<uint32_t>(address);
    if ((address32 >= mshwmb_->GetUint32()) &&
        (address32 < mshwm_->GetUint32())) {
      mshwm_->Set(address32);
    }
  }
  // Forward the store.
  tagged_memory_->Store(address_db, mask_db, el_size, db);
}

void CheriotState::DbgLoadMemory(uint64_t address, DataBuffer *db) {
  tagged_memory_->Load(address, db, nullptr, nullptr);
}

void CheriotState::Fence(const Instruction *inst, int fm, int predecessor,
                         int successor) {
  // TODO: Add fence operation once operations have non-zero latency.
}

void CheriotState::FenceI(const Instruction *inst) {
  // TODO: Add instruction fence operation when needed.
}

void CheriotState::ECall(const Instruction *inst) {
  // If there is a handler, call it.
  if (on_ecall_ != nullptr) {
    auto res = on_ecall_(inst);
    // If the handler returns true, the ecall has been handled, just return.
    if (res) return;
  }

  // Otherwise trap.
  std::string where = (inst != nullptr)
                          ? absl::StrCat(absl::Hex(inst->address()))
                          : "unknown location";

  EC code;
  code = EC::kEnvCallFromMMode;

  uint64_t epc = inst->address();
  Trap(/*is_interrupt*/ false, 0, *code, epc, inst);
}

void CheriotState::EBreak(const Instruction *inst) {
  // Call the handlers.
  for (auto &handler : on_ebreak_) {
    bool res = handler(inst);
    // If a handler returns true, the ebreak has been handled. Just return.
    if (res) return;
  }
  // Otherwise trap.
  // Set the return address to the current instruction.
  auto epc = (inst != nullptr) ? inst->address() : 0;
  Trap(/*is_interrupt=*/false, /*trap_value=*/epc, 3, epc, inst);
}

void CheriotState::WFI(const Instruction *inst) {
  // Call the handler.
  if (on_wfi_ != nullptr) {
    bool res = on_wfi_(inst);
    // If the handler returns true, the wfi has been handled. Just return.
    if (res) return;
  }
  // If no handler is specified, or if no handlers returns true, treat it
  // as a nop.
  std::string where = (inst != nullptr)
                          ? absl::StrCat(absl::Hex(inst->address()))
                          : "unknown location";

  LOG(INFO) << "No handler for wfi: treating as nop: " << where;
}

void CheriotState::Cease(const Instruction *inst) {
  // Call the handler.
  if (on_cease_ != nullptr) {
    const bool res = on_cease_(inst);
    if (res) return;
  }

  // If no handler is specified, then CEASE is treated as an infinite loop.
  // TODO(torerik): set next pc to the right value.

  const std::string where = (inst != nullptr)
                                ? absl::StrCat(absl::Hex(inst->address()))
                                : "unknown location";

  LOG(INFO) << "No handler for cease: treating as an infinite loop: " << where;
}

void CheriotState::Trap(bool is_interrupt, uint64_t trap_value,
                        uint64_t exception_code, uint64_t epc,
                        const Instruction *inst) {
  // LOG(INFO) << "Trap: " << std::hex << is_interrupt << " " << trap_value << "
  // " << exception_code << " " << epc; Call the handler.
  if (on_trap_ != nullptr) {
    bool res = on_trap_(is_interrupt, trap_value, exception_code, epc, inst);
    // If the handler returns true, the trap has been handled. Just return.
    if (res) return;
  }
  // Get trap destination.
  int trap_vector_mode = mtcc_->address() & 0x3ULL;
  uint64_t trap_target = mtcc_->address() & ~0x3ULL;
  if (trap_vector_mode == 1) {
    trap_target += 4 * exception_code;
  }

  // Set mepc by copying pcc to mepc and setting the address to epc.
  mepcc_->CopyFrom(*pcc());
  mepcc_->set_address(epc);
  // Set xcause.
  mcause_->Set(exception_code);
  if (is_interrupt) {
    mcause_->SetBits(static_cast<uint32_t>(0x8000'0000));
  }
  // Set mstatus bits accordingly.

  // Set the privilege mode to return to after the interrupt.
  mstatus_->set_mpp(*PrivilegeMode::kMachine);
  // Save the current interrupt enable to mpie.
  mstatus_->set_mpie(mstatus_->mie());
  // Disable further interrupts.
  mstatus_->set_mie(0);

  // Advance data buffer delay line until empty. Flush pending writes to
  // register and possibly pc.
  while (!data_buffer_delay_line()->IsEmpty()) {
    data_buffer_delay_line()->Advance();
  }

  // Set mtval.
  mtval_->Write(trap_value);

  // Update the PC from the mtvec_ capability. Update the address in case of
  // vectored mode.
  pcc()->CopyFrom(*mtcc_);
  pcc()->set_address(trap_target);
  set_branch(true);
  // TODO(torerik): set next pc
  mstatus_->Submit();
}

// CheckForInterrupt is called whenever any relevant bits in the interrupt
// enable and set bits are changed. It should always be scheduled to execute
// from the function_delay_line, that way it is executed after an instruction
// has completed execution.
void CheriotState::CheckForInterrupt() {
  // Get global interrupts enable bit.
  bool mie = mstatus_->mie();
  // No interrupts can be taken.
  if (!mie) return;

  // Get pending and enabled interrupts.
  uint32_t interrupts = mip_->AsUint32() & mie_->AsUint32();
  // If there are no enabled pending interrupts, just return.
  if (interrupts == 0) return;

  InterruptCode code = PickInterrupt(interrupts);

  available_interrupt_code_ = code;
  is_interrupt_available_ = true;
}

// Take the interrupt that is pending.
void CheriotState::TakeAvailableInterrupt(uint64_t epc) {
  // Make sure an interrupt is set as pending by CheckForInterrupt.
  if (!is_interrupt_available_) return;
  // Initiate the interrupt.
  Trap(/*is_interrupt*/ true, 0, *available_interrupt_code_, epc, nullptr);
  // Clear pending interrupt.
  is_interrupt_available_ = false;
  ++interrupt_handler_depth_;
  available_interrupt_code_ = InterruptCode::kNone;
}

// The priority order of the interrupts are as follows:
// mei, msi, mti, sei, ssi, sti, uei, usi, uti.
InterruptCode CheriotState::PickInterrupt(uint32_t interrupts) {
  if (interrupts & (1 << *InterruptCode::kMachineExternalInterrupt))
    return InterruptCode::kMachineExternalInterrupt;
  if (interrupts & (1 << *InterruptCode::kMachineSoftwareInterrupt))
    return InterruptCode::kMachineSoftwareInterrupt;
  if (interrupts & (1 << *InterruptCode::kMachineTimerInterrupt))
    return InterruptCode::kMachineTimerInterrupt;

  // No supervisor or user mode.

  return InterruptCode::kNone;
}

// Check if the address is for a capability that has been revoked. If so,
// return true;
bool CheriotState::MustRevoke(uint32_t address) const {
  uint64_t revocation_address =
      address & ~(static_cast<uint64_t>(kCapabilitySizeInBytes) - 1ULL);
  // If the address is less than the revocation memory base, return false.
  if (revocation_address < revocation_ram_base()) return false;
  uint64_t offset = (revocation_address - revocation_ram_base());
  uint64_t revocation_offset = offset >> 6;
  tagged_memory_->Load(revocation_mem_base() + revocation_offset,
                       revocation_db_, nullptr, nullptr);
  uint8_t revocation_bits = revocation_db_->Get<uint8_t>(0);
  int bit_offset = (offset >> 3) & 0b111;
  return revocation_bits & (1 << bit_offset);
}

uint64_t RiscVCheri32PcSourceOperand::GetPC() {
  auto *pcc = state_->pcc();
  // PCC should always be a valid capability, otherwise an exception would
  // have been taken. It should also have execute permissions. The only thing
  // to check for is that the address is within bounds.
  if (!pcc->IsInBounds(pcc->address(), state_->has_compact() ? 2 : 4)) {
    state_->HandleCheriRegException(nullptr, pcc->address(),
                                    ExceptionCode::kCapExBoundsViolation, pcc);
  }
  return pcc->address();
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
