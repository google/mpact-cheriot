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

#include "cheriot/cheriot_test_rig.h"

#include <cstdint>
#include <cstring>
#include <string>

#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/cheriot_test_rig_decoder.h"
#include "cheriot/riscv_cheriot_register_aliases.h"
#include "cheriot/test_rig_packets.h"
#include "mpact/sim/generic/component.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"
#include "mpact/sim/util/memory/tagged_memory_watcher.h"
#include "riscv//riscv_counter_csr.h"
#include "riscv//riscv_register.h"
#include "riscv//riscv_state.h"

namespace mpact::sim::cheriot {

using EC = ::mpact::sim::riscv::ExceptionCode;
using PB = ::mpact::sim::cheriot::CheriotRegister::PermissionBits;
using CheriotEC = ::mpact::sim::cheriot::ExceptionCode;
using ::mpact::sim::riscv::RiscVCounterCsr;
using ::mpact::sim::riscv::RiscVCounterCsrHigh;
using ::mpact::sim::util::TaggedFlatDemandMemory;
using ::mpact::sim::util::TaggedMemoryWatcher;

constexpr char kCheriotTestRigName[] = "CheriotTestRig";

CheriotTestRig::CheriotTestRig()
    : generic::Component(kCheriotTestRigName),
      counter_num_instructions_("num_instructions", 0) {
  // Set up memory.
  tagged_memory_ = new TaggedFlatDemandMemory(8);
  // Set up watcher and add callbacks.
  tagged_memory_watcher_ = new TaggedMemoryWatcher(tagged_memory_);
  CHECK_OK(tagged_memory_watcher_->SetLoadWatchCallback(
      TaggedMemoryWatcher::AddressRange{0, 0x1'0000'0000ULL},
      absl::bind_front(&CheriotTestRig::OnLoad, this)));
  CHECK_OK(tagged_memory_watcher_->SetStoreWatchCallback(
      TaggedMemoryWatcher::AddressRange{0, 0x1'0000'0000ULL},
      absl::bind_front(&CheriotTestRig::OnStore, this)));
  // Set up sim state.
  state_ = new CheriotState(kCheriotTestRigName, tagged_memory_watcher_);
  // Initialize pcc to 0x8000'0000.
  pcc_ = static_cast<CheriotRegister*>(
      state_->registers()->at(CheriotState::kPcName));
  pcc_->set_address(0x8000'0000);
  cheriot_decoder_ = new CheriotTestRigDecoder(state_);
  // Register instruction counter.
  CHECK_OK(AddCounter(&counter_num_instructions_))
      << "Failed to register counter";
  // Make sure the architectural and abi register aliases are added.
  std::string reg_name;
  std::string xreg_name;
  // Add aliases for capability registers.
  for (int i = 0; i < 32; i++) {
    reg_name = absl::StrCat(CheriotState::kCregPrefix, i);
    // Alias the register with x register names.
    // E.g., 'c10' === 'x10'
    xreg_name = absl::StrCat(CheriotState::kXregPrefix, i);
    CHECK_OK(state_->AddRegisterAlias<CheriotRegister>(reg_name, xreg_name));
    // Alias the register with capability abi register names.
    // E.g., 'c10' === 'ca0'
    CHECK_OK(state_->AddRegisterAlias<CheriotRegister>(reg_name,
                                                       kCRegisterAliases[i]));
    // Alias the register with abi register names.
    // E.g., 'c10' === 'a0'
    CHECK_OK(state_->AddRegisterAlias<CheriotRegister>(reg_name,
                                                       kXRegisterAliases[i]));
  }

  for (int i = 0; i < 32; i++) {
    reg_name = absl::StrCat(CheriotState::kFregPrefix, i);
    (void)state_->AddRegister<riscv::RVFpRegister>(reg_name);
    CHECK_OK(state_->AddRegisterAlias<riscv::RVFpRegister>(
        reg_name, kFRegisterAliases[i]));
  }
  // Register trap monitor.
  state_->set_on_trap(absl::bind_front(&CheriotTestRig::OnTrap, this));
  // Allocate data buffers.
  db1_ = db_factory_.Allocate<uint8_t>(1);
  db2_ = db_factory_.Allocate<uint16_t>(1);
  db4_ = db_factory_.Allocate<uint32_t>(1);
  db8_ = db_factory_.Allocate<uint64_t>(1);
  // Initialize minstret/minstreth. Bind the instruction counter to those
  // registers.
  auto minstret_res = state_->csr_set()->GetCsr("minstret");
  auto minstreth_res = state_->csr_set()->GetCsr("minstreth");
  CHECK_OK(minstret_res.status());
  CHECK_OK(minstreth_res.status());
  auto* minstret = static_cast<RiscVCounterCsr<uint32_t, CheriotState>*>(
      minstret_res.value());
  auto* minstreth =
      static_cast<RiscVCounterCsrHigh<CheriotState>*>(minstreth_res.value());
  minstret->set_counter(&counter_num_instructions_);
  minstreth->set_counter(&counter_num_instructions_);

  // Initialize mcycle/mcycleh. Bind the instruction counter to those
  // registers.
  auto mcycle_res = state_->csr_set()->GetCsr("mcycle");
  auto mcycleh_res = state_->csr_set()->GetCsr("mcycleh");
  CHECK_OK(mcycle_res.status());
  CHECK_OK(mcycleh_res.status());
  auto* mcycle =
      static_cast<RiscVCounterCsr<uint32_t, CheriotState>*>(mcycle_res.value());
  auto* mcycleh =
      static_cast<RiscVCounterCsrHigh<CheriotState>*>(mcycleh_res.value());
  mcycle->set_counter(&counter_num_instructions_);
  mcycleh->set_counter(&counter_num_instructions_);
  // Set memory limits according to the memory space for TestRIG.
  state_->set_max_physical_address(0x8000'0000ULL + 64 * 1024);
  state_->set_min_physical_address(0x8000'0000ULL);
  ResetArch();
}

CheriotTestRig::~CheriotTestRig() {
  delete cheriot_decoder_;
  delete state_;
  delete tagged_memory_;
  delete tagged_memory_watcher_;
  // Deallocate data buffers.
  db1_->DecRef();
  db2_->DecRef();
  db4_->DecRef();
  db8_->DecRef();
}

absl::Status CheriotTestRig::Execute(
    const test_rig::InstructionPacket& inst_packet, int fd) {
  switch (trace_version_) {
    case 1:
      return ExecuteV1(inst_packet, fd);
      break;
    case 2:
      return ExecuteV2(inst_packet, fd);
      break;
    default:
      return absl::UnimplementedError(
          absl::StrCat("Trace version ", trace_version_, " is not supported"));
  }
}

absl::Status CheriotTestRig::SetVersion(int version) {
  if (version > 2)
    return absl::UnimplementedError(
        absl::StrCat("Trace version ", version, " is not supported"));
  trace_version_ = version;
  return absl::OkStatus();
}

absl::Status CheriotTestRig::ExecuteV1(
    const test_rig::InstructionPacket& inst_packet, int fd) {
  test_rig::ExecutionPacket ep;
  uint32_t inst_word = inst_packet.rvfi_insn;
  ep.rvfi_halt = 0;
  // Clear memory related fields.
  mem_addr_ = 0;
  mem_r_mask_ = 0;
  mem_w_mask_ = 0;
  std::memset(mem_r_data_, 0, sizeof(mem_r_data_));
  std::memset(mem_w_data_, 0, sizeof(mem_w_data_));
  // If trap was set last time around, set indicator for trap handler.
  ep.rvfi_intr = trap_set_ ? 1 : 0;
  trap_set_ = false;
  uint64_t pc = pcc_->address();
  ep.rvfi_pc_rdata = pc;
  // Decode fills in rd_addr, rs2_addr, rs1_addr.
  CheriotTestRigDecoder::DecodeInfo decode_info;
  auto* inst = cheriot_decoder_->DecodeInstruction(pc, inst_word, decode_info);
  ep.rvfi_rd_addr = decode_info.rd;
  ep.rvfi_rs1_addr = decode_info.rs1;
  ep.rvfi_rs2_addr = decode_info.rs2;
  ep.rvfi_rs1_data = GetRegister(ep.rvfi_rs1_addr);
  ep.rvfi_rs2_data = GetRegister(ep.rvfi_rs2_addr);
  uint64_t next_pc = pc + inst->size();
  // Execute the instruction.
  if (!pcc_->HasPermission(PB::kPermitExecute)) {
    state_->HandleCheriRegException(
        inst, inst->address(), CheriotEC::kCapExPermitExecuteViolation, pcc_);
    inst_word = 0;
  } else if (!pcc_->IsInBounds(pc, state_->has_compact() ? sizeof(uint16_t)
                                                         : sizeof(uint32_t))) {
    state_->HandleCheriRegException(inst, inst->address(),
                                    CheriotEC::kCapExBoundsViolation, pcc_);
    inst_word = 0;
  } else {
    inst->Execute(nullptr);
  }
  // Check for trap.
  if (trap_set_) {
    next_pc = pcc_->address();
    // If there's a trap, clear relevant fields.
    ep.rvfi_trap = 1;
    ep.rvfi_rd_addr = 0;
    ep.rvfi_rs2_addr = 0;
    ep.rvfi_rs1_addr = 0;
    ep.rvfi_rd_wdata = 0;
    ep.rvfi_rs2_data = 0;
    ep.rvfi_rs1_data = 0;
    ep.rvfi_mem_addr = 0;
    ep.rvfi_mem_rdata = 0;
    ep.rvfi_mem_wdata = 0;
    ep.rvfi_mem_rmask = 0;
    ep.rvfi_mem_wmask = 0;
    // If the trap was a memory access trap, set the memory address.
    auto res = state_->csr_set()->GetCsr("mcause");
    auto cause = res.value()->AsUint32();
    if (cause == *EC::kLoadAccessFault || cause == *EC::kStoreAccessFault) {
      res = state_->csr_set()->GetCsr("mtval");
      ep.rvfi_mem_addr = res.value()->AsUint32();
    }
  } else {
    if (state_->branch()) {
      next_pc = pcc_->address();
    }
    ep.rvfi_trap = 0;
    // Update register write data.
    ep.rvfi_rd_wdata = GetRegister(ep.rvfi_rd_addr);
    // Update memory fields.
    ep.rvfi_mem_addr = mem_addr_;
    ep.rvfi_mem_rdata = mem_r_data_[0];
    ep.rvfi_mem_wdata = mem_w_data_[0];
    ep.rvfi_mem_rmask = mem_r_mask_;
    ep.rvfi_mem_wmask = mem_w_mask_;
  }
  state_->set_branch(false);
  counter_num_instructions_.Increment(1);
  ep.rvfi_insn = inst_word;
  ep.rvfi_pc_wdata = next_pc;
  ep.rvfi_order = counter_num_instructions_.GetValue();
  pcc_->set_address(next_pc);
  inst->DecRef();
  auto res = write(fd, &ep, sizeof(ep));
  if (res != sizeof(ep)) {
    LOG(ERROR) << "Error writing to trace socket (" << res << ")\n";
    return absl::InternalError("Error writing to trace socket");
  }
  return absl::OkStatus();
}

absl::Status CheriotTestRig::ExecuteV2(
    const test_rig::InstructionPacket& inst_packet, int fd) {
  test_rig::ExecutionPacketExtInteger ep_ext_integer;
  test_rig::ExecutionPacketExtMemAccess ep_ext_mem_access;
  test_rig::ExecutionPacketV2 ep_v2;
  test_rig::ExecutionPacketMetaData& ep_metadata = ep_v2.basic_data;
  test_rig::ExecutionPacketPC& ep_pc = ep_v2.pc_data;

  uint32_t inst_word = inst_packet.rvfi_insn;
  ep_metadata.rvfi_halt = 0;
  // Clear memory related fields.
  mem_addr_ = 0;
  mem_r_mask_ = 0;
  mem_w_mask_ = 0;
  std::memset(mem_r_data_, 0, sizeof(mem_r_data_));
  std::memset(mem_w_data_, 0, sizeof(mem_w_data_));
  // If trap was set last time around, set indicator for trap handler.
  ep_metadata.rvfi_intr = trap_set_ ? 1 : 0;
  trap_set_ = false;
  uint64_t pc = pcc_->address();
  ep_pc.rvfi_pc_rdata = pc;
  // Decode fills in rd_addr, rs2_addr, rs1_addr.
  CheriotTestRigDecoder::DecodeInfo decode_info;
  auto* inst = cheriot_decoder_->DecodeInstruction(pc, inst_word, decode_info);
  ep_ext_integer.rvfi_rd_addr = decode_info.rd;
  ep_ext_integer.rvfi_rs1_addr = decode_info.rs1;
  ep_ext_integer.rvfi_rs2_addr = decode_info.rs2;
  // Get register source values.
  ep_ext_integer.rvfi_rs1_rdata = GetRegister(ep_ext_integer.rvfi_rs1_addr);
  ep_ext_integer.rvfi_rs2_rdata = GetRegister(ep_ext_integer.rvfi_rs2_addr);
  uint64_t next_pc = pc + inst->size();
  // Execute the instruction.
  if (!pcc_->tag()) {
    state_->HandleCheriRegException(inst, inst->address(),
                                    CheriotEC::kCapExTagViolation, pcc_);
    inst_word = 0;
  } else if (!pcc_->HasPermission(PB::kPermitExecute)) {
    state_->HandleCheriRegException(
        inst, inst->address(), CheriotEC::kCapExPermitExecuteViolation, pcc_);
    inst_word = 0;
  } else if (!pcc_->IsInBounds(pc, state_->has_compact() ? sizeof(uint16_t)
                                                         : sizeof(uint32_t))) {
    state_->HandleCheriRegException(inst, inst->address(),
                                    CheriotEC::kCapExBoundsViolation, pcc_);
    inst_word = 0;
  } else {
    inst->Execute(nullptr);
  }

  // Check for trap.
  if (trap_set_) {
    next_pc = pcc_->address();
    // If there's a trap, clear relevant fields.
    ep_metadata.rvfi_trap = 1;
    ep_ext_integer.rvfi_rd_addr = 0;
    ep_ext_integer.rvfi_rs2_addr = 0;
    ep_ext_integer.rvfi_rs1_addr = 0;
    ep_ext_integer.rvfi_rd_wdata = 0;
    ep_ext_integer.rvfi_rs2_rdata = 0;
    ep_ext_integer.rvfi_rs1_rdata = 0;
    ep_ext_mem_access.rvfi_mem_addr = 0;
    std::memset(ep_ext_mem_access.rvfi_mem_rdata, 0,
                sizeof(ep_ext_mem_access.rvfi_mem_rdata));
    std::memset(ep_ext_mem_access.rvfi_mem_wdata, 0,
                sizeof(ep_ext_mem_access.rvfi_mem_wdata));
    ep_ext_mem_access.rvfi_mem_rmask = 0;
    ep_ext_mem_access.rvfi_mem_wmask = 0;
    // If the trap was a memory access trap, set the memory address.
    auto res = state_->csr_set()->GetCsr("mcause");
    auto cause = res.value()->AsUint32();
    if (cause == *EC::kLoadAccessFault || cause == *EC::kStoreAccessFault) {
      res = state_->csr_set()->GetCsr("mtval");
      ep_ext_mem_access.rvfi_mem_addr = res.value()->AsUint32();
    }
  } else {
    if (state_->branch()) {
      next_pc = pcc_->address();
    }
    ep_metadata.rvfi_trap = 0;
    // Update register write data.
    ep_ext_integer.rvfi_rd_wdata = GetRegister(ep_ext_integer.rvfi_rd_addr);
    // Update memory fields.
    ep_ext_mem_access.rvfi_mem_addr = mem_addr_;
    std::memcpy(ep_ext_mem_access.rvfi_mem_rdata, mem_r_data_,
                sizeof(ep_ext_mem_access.rvfi_mem_rdata));
    std::memcpy(ep_ext_mem_access.rvfi_mem_wdata, mem_w_data_,
                sizeof(ep_ext_mem_access.rvfi_mem_wdata));
    ep_ext_mem_access.rvfi_mem_rmask = mem_r_mask_;
    ep_ext_mem_access.rvfi_mem_wmask = mem_w_mask_;
  }
  state_->set_branch(false);
  counter_num_instructions_.Increment(1);
  ep_metadata.rvfi_mode = test_rig::kMachineMode;
  ep_metadata.rvfi_ixl = test_rig::kXL32;
  ep_metadata.rvfi_insn = inst_word;
  ep_pc.rvfi_pc_wdata = next_pc;
  ep_metadata.rvfi_order = counter_num_instructions_.GetValue();
  ep_metadata.rvfi_valid = 1;
  ep_metadata.rvfi_padding[0] = 0;
  ep_metadata.rvfi_padding[1] = 0;
  pcc_->set_address(next_pc);
  inst->DecRef();
  ep_v2.trace_size = sizeof(ep_v2);
  ep_v2.available_fields = 0;
  // Check to see if the memory access extension should be added.
  if ((ep_ext_mem_access.rvfi_mem_rmask != 0) ||
      (ep_ext_mem_access.rvfi_mem_wmask != 0) ||
      (ep_ext_mem_access.rvfi_mem_addr != 0)) {
    ep_v2.trace_size += sizeof(ep_ext_mem_access);
    ep_v2.available_fields |= test_rig::kMemoryAccess;
  }
  // Check to see if the integer data extension should be added.
  if ((ep_ext_integer.rvfi_rd_addr != 0) ||
      (ep_ext_integer.rvfi_rs1_addr != 0) ||
      (ep_ext_integer.rvfi_rs2_addr != 0)) {
    ep_v2.trace_size += sizeof(ep_ext_integer);
    ep_v2.available_fields |= test_rig::kIntegerData;
  }

  // Write out the execution packet.
  auto res = write(fd, &ep_v2, sizeof(ep_v2));
  if (res != sizeof(ep_v2)) {
    LOG(ERROR) << "Error writing to trace socket (" << res << ")\n";
    return absl::InternalError("Error writing to trace socket");
  }
  // Write out the extension packets.
  if (ep_v2.available_fields & test_rig::kIntegerData) {
    auto res = write(fd, &ep_ext_integer, sizeof(ep_ext_integer));
    if (res != sizeof(ep_ext_integer)) {
      LOG(ERROR) << "Error writing to trace socket (" << res << ")\n";
      return absl::InternalError("Error writing to trace socket");
    }
  }
  if (ep_v2.available_fields & test_rig::kMemoryAccess) {
    auto res = write(fd, &ep_ext_mem_access, sizeof(ep_ext_mem_access));
    if (res != sizeof(ep_ext_mem_access)) {
      LOG(ERROR) << "Error writing to trace socket (" << res << ")\n";
      return absl::InternalError("Error writing to trace socket");
    }
  }
  return absl::OkStatus();
}

// Reset state.
void CheriotTestRig::ResetArch() {
  // Reset state.
  state_->Reset();
  // Reset pcc.
  pcc_->ResetExecuteRoot();
  pcc_->set_address(0x8000'0000);
  // Clear 64KB memory.
  db8_->Set<uint64_t>(0, 0);
  for (uint64_t addr = 0x8000'0000ULL; addr < 0x8001'0000; addr += 8) {
    tagged_memory_->Store(addr, db8_);
  }
  // Reset instruction counter.
  counter_num_instructions_.SetValue(0);
  // Set all capability registers to memory root capability.
  for (auto const& name :
       {"c1",  "c2",  "c3",  "c4",  "c5",  "c6",  "c7",  "c8",
        "c9",  "c10", "c11", "c12", "c13", "c14", "c15", "c16",
        "c17", "c18", "c19", "c20", "c21", "c22", "c23", "c24",
        "c25", "c26", "c27", "c28", "c29", "c30", "c31"}) {
    auto* cap_reg = state_->GetRegister<CheriotRegister>(name).first;
    cap_reg->ResetMemoryRoot();
  }
}

absl::Status CheriotTestRig::Reset(uint8_t halt, int fd) {
  ResetArch();
  trap_set_ = false;
  // Write the appropriate trace packet out.
  switch (trace_version_) {
    case 1:
      return ResetV1(halt, fd);
      break;
    case 2:
      return ResetV2(halt, fd);

    default:
      return absl::UnimplementedError(
          absl::StrCat("Trace version ", trace_version_, " is not supported"));
  }
}

absl::Status CheriotTestRig::ResetV1(uint8_t halt, int fd) {
  test_rig::ExecutionPacket ep;
  std::memset(&ep, 0, sizeof(ep));
  ep.rvfi_halt = halt;
  auto res = write(fd, &ep, sizeof(ep));
  if (res != sizeof(ep)) {
    LOG(ERROR) << "Error writing to trace socket (" << res << ")\n";
    return absl::InternalError("Error writing to trace socket");
  }
  return absl::OkStatus();
}

absl::Status CheriotTestRig::ResetV2(uint8_t halt, int fd) {
  test_rig::ExecutionPacketV2 ep_v2;
  ep_v2.trace_size = sizeof(ep_v2);
  ep_v2.available_fields = 0;
  std::memset(&ep_v2.pc_data, 0, sizeof(ep_v2.pc_data));
  std::memset(&ep_v2.basic_data, 0, sizeof(ep_v2.basic_data));
  ep_v2.basic_data.rvfi_halt = halt;
  auto res = write(fd, &ep_v2, sizeof(ep_v2));
  if (res != sizeof(ep_v2)) {
    LOG(ERROR) << "Error writing to trace socket (" << res << ")\n";
    return absl::InternalError("Error writing to trace socket");
  }
  return absl::OkStatus();
}

// Just capture that a trap occurred.
bool CheriotTestRig::OnTrap(bool is_interrupt, uint64_t trap_value,
                            uint64_t exception_code, uint64_t epc,
                            const Instruction* inst) {
  trap_set_ = true;
  return false;
}

// Capture load information.
void CheriotTestRig::OnLoad(uint64_t address, int size) {
  mem_addr_ = address;
  switch (size) {
    case 1:
      tagged_memory_->Load(address, db1_, nullptr, nullptr);
      mem_r_mask_ = 0x1ULL;
      mem_r_data_[0] = db1_->Get<uint8_t>(0);
      break;
    case 2:
      tagged_memory_->Load(address, db2_, nullptr, nullptr);
      mem_r_mask_ = 0x3ULL;
      mem_r_data_[0] = db2_->Get<uint16_t>(0);
      break;
    case 4:
      tagged_memory_->Load(address, db4_, nullptr, nullptr);
      mem_r_mask_ = 0xfULL;
      mem_r_data_[0] = db4_->Get<uint32_t>(0);
      break;
    case 8:
      tagged_memory_->Load(address, db8_, nullptr, nullptr);
      mem_r_mask_ = 0xffULL;
      mem_r_data_[0] = db8_->Get<uint64_t>(0);
      break;
    default:
      break;
  }
}

// Capture store information.
void CheriotTestRig::OnStore(uint64_t address, int size) {
  mem_addr_ = address;
  switch (size) {
    case 1:
      tagged_memory_->Load(address, db1_, nullptr, nullptr);
      mem_w_mask_ = 0x1ULL;
      mem_w_data_[0] = db1_->Get<uint8_t>(0);
      break;
    case 2:
      tagged_memory_->Load(address, db2_, nullptr, nullptr);
      mem_w_mask_ = 0x3ULL;
      mem_w_data_[0] = db2_->Get<uint16_t>(0);
      break;
    case 4:
      tagged_memory_->Load(address, db4_, nullptr, nullptr);
      mem_w_mask_ = 0xfULL;
      mem_w_data_[0] = db4_->Get<uint32_t>(0);
      break;
    case 8:
      tagged_memory_->Load(address, db8_, nullptr, nullptr);
      mem_w_mask_ = 0xffULL;
      mem_w_data_[0] = db8_->Get<uint64_t>(0);
      break;
    default:
      break;
  }
}

// Get the register value.
uint32_t CheriotTestRig::GetRegister(uint32_t reg_id) {
  auto reg_name = absl::StrCat(CheriotState::kXregPrefix, reg_id);
  auto ptr = state_->registers()->find(reg_name);
  if (ptr == state_->registers()->end()) {
    return 0;
  } else {
    auto* reg = ptr->second;
    auto* creg = static_cast<CheriotRegister*>(reg);
    return creg->address();
  }
}

}  // namespace mpact::sim::cheriot
