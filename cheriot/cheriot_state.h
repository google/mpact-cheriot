/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MPACT_CHERIOT__CHERIOT_STATE_H_
#define MPACT_CHERIOT__CHERIOT_STATE_H_

#include <any>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "mpact/sim/generic/arch_state.h"
#include "mpact/sim/generic/counters.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/operand_interface.h"
#include "mpact/sim/generic/ref_count.h"
#include "mpact/sim/generic/type_helpers.h"
#include "mpact/sim/util/memory/memory_interface.h"
#include "mpact/sim/util/memory/tagged_memory_interface.h"
#include "riscv//riscv_csr.h"
#include "riscv//riscv_fp_state.h"
#include "riscv//riscv_misa.h"
#include "riscv//riscv_pmp.h"
#include "riscv//riscv_register.h"
#include "riscv//riscv_state.h"
#include "riscv//riscv_vector_state.h"
#include "riscv//riscv_xip_xie.h"
#include "riscv//riscv_xstatus.h"

// This file defines the mpact_sim architectural state class for RiscV CHERIoT.
// It is very similar to the RiscVState class defined in mpact_riscv, but due
// to the changes to the register architecture driven by CHERIoT, a new class
// was defined instead of attempting to inherit from RiscVState. That being
// said, several types from mpact_riscv are re-used here.

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::DataBuffer;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::generic::ReferenceCount;
using ::mpact::sim::generic::SimpleCounter;
using ::mpact::sim::riscv::InterruptCode;
using ::mpact::sim::riscv::IsaExtension;
using ::mpact::sim::riscv::PrivilegeMode;
using ::mpact::sim::riscv::RiscVCsrInterface;
using ::mpact::sim::riscv::RiscVCsrSet;
using ::mpact::sim::riscv::RiscVFPState;
using ::mpact::sim::riscv::RiscVMIe;
using ::mpact::sim::riscv::RiscVMIp;
using ::mpact::sim::riscv::RiscVMIsa;
using ::mpact::sim::riscv::RiscVMStatus;
using ::mpact::sim::riscv::RiscVPmp;
using ::mpact::sim::riscv::RiscVSimpleCsr;
using ::mpact::sim::riscv::RVVectorRegister;

// Forward declare the CHERIoT register type.
class CheriotRegister;
class CheriotVectorState;

// CHERIoT exception codes. These are used in addition to the ones defined for
// vanilla RiscV.

enum class ExceptionCode : uint32_t {
  kCapExBoundsViolation = 0x01,
  kCapExTagViolation = 0x02,
  kCapExSealViolation = 0x03,
  kCapExPermitExecuteViolation = 0x11,
  kCapExPermitLoadViolation = 0x12,
  kCapExPermitStoreViolation = 0x13,
  kCapExPermitStoreCapabilityViolation = 0x15,
  kCapExPermitStoreLocalCapabilityViolation = 0x16,
  kCapExPermitAccessSystemRegistersViolation = 0x18,
};

// Load context used for capability tag loads. See below for the context type
// for capability loads.
struct CapabilityTagsLoadContext32 : public generic::ReferenceCount {
  CapabilityTagsLoadContext32(DataBuffer *tags, CheriotRegister *dest)
      : tags(tags), dest(dest) {}
  ~CapabilityTagsLoadContext32() override {
    if (tags != nullptr) tags->DecRef();
  }

  void OnRefCountIsZero() override {
    if (tags != nullptr) tags->DecRef();
    tags = nullptr;
    generic::ReferenceCount::OnRefCountIsZero();
  }
  // Data buffer for the tags. One tag bit is stored in each byte.
  DataBuffer *tags;
  // The destination register.
  CheriotRegister *dest;
};

// Load context used for capability loads.
struct CapabilityLoadContext32 : public generic::ReferenceCount {
  CapabilityLoadContext32(DataBuffer *db, DataBuffer *tag_db,
                          uint32_t permissions, bool clear_tag)
      : db(db),
        tag_db(tag_db),
        permissions(permissions),
        clear_tag(clear_tag) {}
  ~CapabilityLoadContext32() override {
    if (db != nullptr) db->DecRef();
    if (tag_db != nullptr) tag_db->DecRef();
  }

  void OnRefCountIsZero() override {
    if (db != nullptr) db->DecRef();
    db = nullptr;
    if (tag_db != nullptr) tag_db->DecRef();
    tag_db = nullptr;
    generic::ReferenceCount::OnRefCountIsZero();
  }

  // Data buffer for the memory content.
  DataBuffer *db;
  // Data buffer for the tags. One tag bit is stored in each byte.
  DataBuffer *tag_db;
  // The permissions of the capability used for the load.
  uint32_t permissions;
  // If true, clear the tag upon writing the result to the capability register.
  bool clear_tag;
};

class CheriotState;

// Forward declare a template function defined in the .cc file that is
// a friend of the state class.
template <typename T>
void CreateCsrs(CheriotState *, std::vector<RiscVCsrInterface *> &);

class RiscVCheri32PcSourceOperand;

using ::mpact::sim::generic::operator*;  // NOLINT: used below (clang error).

// Struct to track interrupt/trap information.
struct InterruptInfo {
  bool is_interrupt;
  uint32_t cause;
  uint32_t tval;
  uint32_t epc;
};
using InterruptInfoList = std::deque<InterruptInfo>;

class CheriotState : public generic::ArchState {
 public:
  static int constexpr kCapRegQueueSizeMask = 0x11;
  static constexpr uint32_t kCheriExceptionCode = 0x1c;
  static constexpr char kCregPrefix[] = "c";
  static constexpr char kFregPrefix[] = "f";
  static constexpr char kXregPrefix[] = "x";
  static constexpr char kCsrName[] = "csr";
  friend void CreateCsrs<uint32_t>(CheriotState *,
                                   std::vector<RiscVCsrInterface *> &);
  friend void CreateCsrs<uint64_t>(CheriotState *,
                                   std::vector<RiscVCsrInterface *> &);
  auto static constexpr kVregPrefix =
      ::mpact::sim::riscv::RiscVState::kVregPrefix;

  // Memory footprint of a capability register.
  static constexpr int kCapabilitySizeInBytes = 8;
  // Pc name.
  static constexpr char kPcName[] = "pcc";
  // Constructors and destructor.
  CheriotState(std::string_view id, util::TaggedMemoryInterface *memory,
               util::AtomicMemoryOpInterface *atomic_memory);
  CheriotState(std::string_view id, util::TaggedMemoryInterface *memory)
      : CheriotState(id, memory, nullptr) {}
  ~CheriotState() override;

  // Deleted constructors and operators.
  CheriotState(const CheriotState &) = delete;
  CheriotState &operator=(const CheriotState &) = delete;
  CheriotState(CheriotState &&) = delete;
  CheriotState &operator=(CheriotState &&) = delete;

  // Reset all registers and CSRs to initial values.
  void Reset();
  // Return a pair consisting of pointer to the named register and a bool that
  // is true if the register had to be created, and false if it was found
  // in the register map (or if nullptr is returned).
  template <typename RegisterType>
  std::pair<RegisterType *, bool> GetRegister(absl::string_view name) {
    // If the register already exists, return a pointer to the register.
    auto ptr = registers()->find(std::string(name));
    if (ptr != registers()->end())
      return std::make_pair(static_cast<RegisterType *>(ptr->second), false);
    // Create a new register and return a pointer to the object.
    return std::make_pair(AddRegister<RegisterType>(name), true);
  }

  // Specialization for RiscV vector registers.
  template <>
  std::pair<RVVectorRegister *, bool> GetRegister<RVVectorRegister>(
      absl::string_view name) {
    int vector_byte_width = vector_register_width();
    if (vector_byte_width == 0) return std::make_pair(nullptr, false);
    auto ptr = registers()->find(std::string(name));
    if (ptr != registers()->end())
      return std::make_pair(static_cast<RVVectorRegister *>(ptr->second),
                            false);
    // Create a new register and return a pointer to the object.
    return std::make_pair(
        AddRegister<RVVectorRegister>(name, vector_byte_width), true);
  }

  // Add register alias.
  template <typename RegisterType>
  absl::Status AddRegisterAlias(absl::string_view current_name,
                                absl::string_view new_name) {
    auto ptr = registers()->find(std::string(current_name));
    if (ptr == registers()->end()) {
      return absl::NotFoundError(
          absl::StrCat("Register '", current_name, "' does not exist."));
    }
    AddRegister(new_name, ptr->second);
    return absl::OkStatus();
  }
  // This is called by instruction semantic functions to register a CHERIoT
  // specific exception.
  void HandleCheriRegException(const Instruction *inst, uint64_t epc,
                               ExceptionCode code, const CheriotRegister *reg);

  // Methods called by instruction semantic functions to load from memory.
  void LoadMemory(const Instruction *inst, uint64_t address, DataBuffer *db,
                  Instruction *child_inst, ReferenceCount *context);
  void LoadMemory(const Instruction *inst, DataBuffer *address_db,
                  DataBuffer *mask_db, int el_size, DataBuffer *db,
                  Instruction *child_inst, ReferenceCount *context);
  // Methods called by instruction semantic functions to store to memory.
  void StoreMemory(const Instruction *inst, uint64_t address, DataBuffer *db);
  void StoreMemory(const Instruction *inst, DataBuffer *address_db,
                   DataBuffer *mask_db, int el_size, DataBuffer *db);
  // Methods called by instruction semantic functions to load and store
  // capabilities.
  void LoadCapability(const Instruction *instruction, uint32_t address,
                      DataBuffer *db, DataBuffer *tags, Instruction *child,
                      CapabilityLoadContext32 *context);
  void StoreCapability(const Instruction *instruction, uint32_t address,
                       DataBuffer *db, DataBuffer *tags);

  // Debug memory methods.
  void DbgLoadMemory(uint64_t address, DataBuffer *db);
  void DbgStoreMemory(uint64_t address, DataBuffer *db);
  // Called by the fence instruction semantic function to signal a fence
  // operation.
  void Fence(const Instruction *inst, int fm, int predecessor, int successor);
  // Synchronize instruction and data streams.
  void FenceI(const Instruction *inst);
  // System call.
  void ECall(const Instruction *inst);
  // Breakpoint.
  void EBreak(const Instruction *inst);
  // WFI
  void WFI(const Instruction *inst);
  // Ceases execution on the core. This is a non-standard instruction that
  // quiesces traffic for embedded cores before halting. The core must be reset
  // to come out of this state.
  void Cease(const Instruction *inst);
  // This method is called to trigger a RiscV trap.
  void Trap(bool is_interrupt, uint64_t trap_value, uint64_t exception_code,
            uint64_t epc, const Instruction *inst);
  // Add ebreak handler.
  void AddEbreakHandler(absl::AnyInvocable<bool(const Instruction *)> handler) {
    on_ebreak_.emplace_back(std::move(handler));
  }
  // This function is called after any event that may have caused an interrupt
  // to be registered as pending or enabled. If the interrupt can be taken
  // it registers it as available.
  void CheckForInterrupt() override;
  // This function is called when the return pc for the available interrupt
  // is known. If there is no available interrupt, it just returns.
  void TakeAvailableInterrupt(uint64_t epc);

  // Indicates that the program has returned from handling an interrupt. This
  // decrements the interrupt handler depth and should be called by the
  // implementations of mret, sret, and uret.
  void SignalReturnFromInterrupt();

  // Returns the depth of the interrupt handler currently being executed, or
  // zero if no interrupt handler is being executed.
  int InterruptHandlerDepth() const {
    return counter_interrupts_taken_.GetValue() -
           counter_interrupt_returns_.GetValue();
  }
  // Returns the interrupt counters. This allows code to be connected to the
  // counters when the value changes.
  SimpleCounter<int64_t> *counter_interrupts_taken() {
    return &counter_interrupts_taken_;
  }

  SimpleCounter<int64_t> *counter_interrupt_returns() {
    return &counter_interrupt_returns_;
  }

  // Returns true if a capability register with the given base should be
  // revoked.
  bool MustRevoke(uint32_t address) const;

  // Accessors.
  // Returns true if an interrupt is available for the core to take or false
  // otherwise.
  inline bool is_interrupt_available() const { return is_interrupt_available_; }
  // Resets the is_interrupt_available flag to false. This should only be called
  // when resetting the RISCV core, as 'is_interrupt_available' is Normally
  // reset during the interrupt handling flow.
  inline void reset_is_interrupt_available() {
    is_interrupt_available_ = false;
  }
  void set_tagged_memory(util::TaggedMemoryInterface *tagged_memory) {
    tagged_memory_ = tagged_memory;
  }
  util::TaggedMemoryInterface *tagged_memory() const { return tagged_memory_; }
  util::AtomicMemoryOpInterface *atomic_tagged_memory() const {
    return atomic_tagged_memory_;
  }
  void set_atomic_tagged_memory(
      util::AtomicMemoryOpInterface *atomic_tagged_memory) {
    atomic_tagged_memory_ = atomic_tagged_memory;
  }

  void set_branch(bool value) { branch_ = value; }
  bool branch() const { return branch_; }

  void set_max_physical_address(uint64_t max_physical_address);
  uint64_t max_physical_address() const { return max_physical_address_; }
  void set_min_physical_address(uint64_t min_physical_address);
  uint64_t min_physical_address() const { return min_physical_address_; }
  // These root capabilities are clean versions of each type of capability with
  // maximum permissions.
  const CheriotRegister *executable_root() const { return executable_root_; }
  const CheriotRegister *sealing_root() const { return sealing_root_; }
  const CheriotRegister *memory_root() const { return memory_root_; }
  // Special capability registers. Pcc replaces the pc. Cgp is a global pointer
  // capability that is aliased with c3.
  CheriotRegister *pcc() const { return pcc_; }
  CheriotRegister *cgp() const { return cgp_; }
  // True if the misa register encodes support for compact instructions.
  bool has_compact() const {
    return (misa_->AsUint64() & *IsaExtension::kCompressed) != 0;
  }
  // Returns the number of tags that can be loaded in a single load tags
  // instruction.
  int num_tags_per_load() const { return num_tags_per_load_; }
  // Provides access to the set of CSRs of this architectural state instance.
  RiscVCsrSet *csr_set() { return csr_set_; }
  // Setters for handlers for ecall, and trap. The handler returns true
  // if the instruction/event was handled, and false otherwise.

  void set_on_ecall(absl::AnyInvocable<bool(const Instruction *)> callback) {
    on_ecall_ = std::move(callback);
  }

  void set_on_wfi(absl::AnyInvocable<bool(const Instruction *)> callback) {
    on_wfi_ = std::move(callback);
  }

  void set_on_cease(absl::AnyInvocable<bool(const Instruction *)> callback) {
    on_cease_ = std::move(callback);
  }

  void set_on_trap(
      absl::AnyInvocable<bool(bool /*is_interrupt*/, uint64_t /*trap_value*/,
                              uint64_t /*exception_code*/, uint64_t /*epc*/,
                              const Instruction *)>
          callback) {
    on_trap_ = std::move(callback);
  }

  RiscVFPState *rv_fp() { return rv_fp_; }
  void set_rv_fp(RiscVFPState *rv_fp) { rv_fp_ = rv_fp; }
  CheriotVectorState *rv_vector() { return rv_vector_; }
  void set_rv_vector(CheriotVectorState *rv_vector) { rv_vector_ = rv_vector; }
  void set_vector_register_width(int value) { vector_register_width_ = value; }
  int vector_register_width() const { return vector_register_width_; }
  RiscVMStatus *mstatus() { return mstatus_; }
  RiscVMIsa *misa() { return misa_; }
  RiscVMIp *mip() { return mip_; }
  RiscVMIe *mie() { return mie_; }
  CheriotRegister *mtcc() { return mtcc_; }
  CheriotRegister *mepcc() { return mepcc_; }
  CheriotRegister *mscratchc() { return mscratchc_; }
  CheriotRegister *mtdc() { return mtdc_; }
  CheriotRegister *temp_reg() { return temp_reg_; }
  RiscVCsrInterface *mcause() { return mcause_; }
  RiscVCheri32PcSourceOperand *pc_src_operand() { return pc_src_operand_; }
  const InterruptInfoList &interrupt_info_list() const {
    return interrupt_info_list_;
  }
  uint64_t revocation_mem_base() const { return revocation_mem_base_; }
  uint64_t revocation_ram_base() const { return revocation_ram_base_; }

  // Tracing accessors.
  bool tracing_active() const { return tracing_active_; }
  void set_tracing_active(bool active) { tracing_active_ = active; }
  uint64_t load_address() const { return load_address_; }
  DataBuffer *load_db() const { return load_db_; }
  void set_load_db(DataBuffer *db) { load_db_ = db; }
  DataBuffer *load_tags() const { return load_tags_; }
  void set_load_tags(DataBuffer *tags) { load_tags_ = tags; }
  uint64_t store_address() const { return store_address_; }
  DataBuffer *store_db() const { return store_db_; }
  void set_store_db(DataBuffer *db) { store_db_ = db; }
  DataBuffer *store_tags() const { return store_tags_; }
  void set_store_tags(DataBuffer *tags) { store_tags_ = tags; }

 private:
  InterruptCode PickInterrupt(uint32_t interrupts);
  // A map from register name to entry in the mtval register.
  absl::flat_hash_map<std::string, uint32_t> cap_index_map_;
  // These are root capabilities
  CheriotRegister *executable_root_ = nullptr;
  CheriotRegister *sealing_root_ = nullptr;
  CheriotRegister *memory_root_ = nullptr;
  // Special capability registers.
  CheriotRegister *pcc_ = nullptr;
  CheriotRegister *cgp_ = nullptr;
  bool branch_ = false;
  int vector_register_width_ = 0;
  uint64_t max_physical_address_;
  uint64_t min_physical_address_ = 0;
  int num_tags_per_load_;
  util::TaggedMemoryInterface *tagged_memory_;
  util::AtomicMemoryOpInterface *atomic_tagged_memory_;
  RiscVCsrSet *csr_set_;
  std::vector<absl::AnyInvocable<bool(const Instruction *)>> on_ebreak_;
  absl::AnyInvocable<bool(const Instruction *)> on_ecall_;
  absl::AnyInvocable<bool(bool, uint64_t, uint64_t, uint64_t,
                          const Instruction *)>
      on_trap_;
  absl::AnyInvocable<bool(const Instruction *)> on_wfi_;
  absl::AnyInvocable<bool(const Instruction *)> on_cease_;
  std::vector<RiscVCsrInterface *> csr_vec_;
  RiscVFPState *rv_fp_ = nullptr;
  CheriotVectorState *rv_vector_ = nullptr;
  // For interrupt handling.
  bool is_interrupt_available_ = false;
  SimpleCounter<int64_t> counter_interrupts_taken_;
  SimpleCounter<int64_t> counter_interrupt_returns_;
  InterruptCode available_interrupt_code_ = InterruptCode::kNone;
  InterruptInfoList interrupt_info_list_;
  // By default, execute in machine mode.
  PrivilegeMode privilege_mode_ = PrivilegeMode::kMachine;
  // Handles to frequently used CSRs.
  RiscVMStatus *mstatus_ = nullptr;
  RiscVMIsa *misa_ = nullptr;
  RiscVMIp *mip_ = nullptr;
  RiscVMIe *mie_ = nullptr;
  RiscVSimpleCsr<uint32_t> *mshwm_ = nullptr;
  RiscVSimpleCsr<uint32_t> *mshwmb_ = nullptr;
  CheriotRegister *mtcc_ = nullptr;
  CheriotRegister *mepcc_ = nullptr;
  CheriotRegister *mscratchc_ = nullptr;
  CheriotRegister *mtdc_ = nullptr;
  CheriotRegister *temp_reg_ = nullptr;
  RiscVPmp *pmp_ = nullptr;
  RiscVCsrInterface *mtval_ = nullptr;
  RiscVCsrInterface *mcause_ = nullptr;
  RiscVCheri32PcSourceOperand *pc_src_operand_ = nullptr;
  // DataBuffer and info used to check for revocation.
  DataBuffer *revocation_db_ = nullptr;
  uint64_t revocation_mem_base_;
  uint64_t revocation_ram_base_;
  // Active tracing flag.
  bool tracing_active_ = false;
  // Members for collecting trace data.
  uint64_t load_address_;
  DataBuffer *load_db_ = nullptr;
  DataBuffer *load_tags_ = nullptr;
  uint64_t store_address_;
  DataBuffer *store_db_ = nullptr;
  DataBuffer *store_tags_ = nullptr;
};

// This class implements the source operand interface on top of a capability
// register so that its value (contained address) can be read as an operand.
class RiscVCheri32PcSourceOperand : public generic::SourceOperandInterface {
 public:
  explicit RiscVCheri32PcSourceOperand(CheriotState *state) : state_(state) {}
  RiscVCheri32PcSourceOperand() = delete;
  RiscVCheri32PcSourceOperand(const RiscVCheri32PcSourceOperand &) = delete;
  RiscVCheri32PcSourceOperand &operator=(const RiscVCheri32PcSourceOperand &) =
      delete;
  ~RiscVCheri32PcSourceOperand() override = default;
  // Methods for accessing the nth value element.
  bool AsBool(int index) override { return static_cast<bool>(GetPC()); }
  int8_t AsInt8(int index) override { return static_cast<int8_t>(GetPC()); }
  uint8_t AsUint8(int index) override { return static_cast<uint8_t>(GetPC()); }
  int16_t AsInt16(int index) override { return static_cast<int16_t>(GetPC()); }
  uint16_t AsUint16(int) override { return static_cast<uint16_t>(GetPC()); }
  int32_t AsInt32(int index) override { return static_cast<int32_t>(GetPC()); }
  uint32_t AsUint32(int index) override {
    return static_cast<uint32_t>(GetPC());
  }
  int64_t AsInt64(int index) override { return static_cast<int64_t>(GetPC()); }
  uint64_t AsUint64(int index) override { return GetPC(); }

  // Return a pointer to the object instance that implements the state in
  // question (or nullptr) if no such object "makes sense". This is used if
  // the object requires additional manipulation - such as a fifo that needs
  // to be popped. If no such manipulation is required, nullptr should be
  // returned.
  std::any GetObject() const override { return std::any(state_->pcc()); }
  // Return the shape of the operand (the number of elements in each dimension).
  // For instance {1} indicates a scalar quantity, whereas {128} indicates an
  // 128 element vector quantity.
  std::vector<int> shape() const override { return {1}; };
  // Return a string representation of the operand suitable for display in
  // disassembly.
  std::string AsString() const override { return "PC"; };

 private:
  uint64_t GetPC();
  CheriotState *state_;
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__CHERIOT_STATE_H_
