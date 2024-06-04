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

#include "cheriot/cheriot_top.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/numeric/bits.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/notification.h"
#include "cheriot/cheriot_debug_interface.h"
#include "cheriot/cheriot_decoder.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_enums.h"
#include "cheriot/riscv_cheriot_fp_state.h"
#include "cheriot/riscv_cheriot_register_aliases.h"
#include "mpact/sim/generic/component.h"
#include "mpact/sim/generic/core_debug_interface.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/decode_cache.h"
#include "mpact/sim/generic/type_helpers.h"
#include "mpact/sim/util/memory/atomic_memory.h"
#include "mpact/sim/util/memory/memory_interface.h"
#include "mpact/sim/util/memory/memory_watcher.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"
#include "mpact/sim/util/memory/tagged_memory_interface.h"
#include "mpact/sim/util/memory/tagged_memory_watcher.h"
#include "re2/re2.h"
#include "riscv//riscv_action_point.h"
#include "riscv//riscv_breakpoint.h"
#include "riscv//riscv_csr.h"
#include "riscv//riscv_register.h"

namespace mpact {
namespace sim {
namespace cheriot {

constexpr char kCheriotName[] = "CherIoT";

using EC = ::mpact::sim::cheriot::ExceptionCode;
using PB = ::mpact::sim::cheriot::CheriotRegister::PermissionBits;

CheriotTop::CheriotTop(std::string name)
    : Component(name),
      owns_memory_(true),
      counter_num_instructions_("num_instructions", 0),
      counter_num_cycles_("num_cycles", 0),
      counter_pc_("pc", 0),
      cap_reg_re_{
          R"((\w+)\.(top|base|length|tag|permissions|object_type|reserved))"} {
  data_memory_ = new util::TaggedFlatDemandMemory(8);
  inst_memory_ = data_memory_;
  Initialize();
}

CheriotTop::CheriotTop(std::string name, util::TaggedMemoryInterface *memory)
    : Component(name),
      inst_memory_(memory),
      data_memory_(memory),
      atomic_memory_(nullptr),
      owns_memory_(false),
      counter_num_instructions_("num_instructions", 0),
      counter_num_cycles_("num_cycles", 0),
      counter_pc_("pc", 0),
      cap_reg_re_{
          R"((\w+)\.(top|base|length|tag|permissions|object_type|reserved))"} {
  Initialize();
}

CheriotTop::CheriotTop(std::string name, util::MemoryInterface *inst_memory,
                       util::TaggedMemoryInterface *data_memory)
    : Component(name),
      inst_memory_(inst_memory),
      data_memory_(data_memory),
      atomic_memory_(nullptr),
      owns_memory_(false),
      counter_num_instructions_("num_instructions", 0),
      counter_num_cycles_("num_cycles", 0),
      counter_pc_("pc", 0),
      cap_reg_re_{
          R"((\w+)\.(top|base|length|tag|permissions|object_type|reserved))"} {
  Initialize();
}

CheriotTop::CheriotTop(std::string name, util::TaggedMemoryInterface *memory,
                       util::MemoryInterface *atomic_memory_if)
    : Component(name),
      inst_memory_(memory),
      data_memory_(memory),
      atomic_memory_if_(atomic_memory_if),
      owns_memory_(false),
      counter_num_instructions_("num_instructions", 0),
      counter_num_cycles_("num_cycles", 0),
      counter_pc_("pc", 0),
      cap_reg_re_{
          R"((\w+)\.(top|base|length|tag|permissions|object_type|reserved))"} {
  Initialize();
}

CheriotTop::CheriotTop(std::string name, util::MemoryInterface *inst_memory,
                       util::TaggedMemoryInterface *data_memory,
                       util::MemoryInterface *atomic_memory_if)
    : Component(name),
      inst_memory_(inst_memory),
      data_memory_(data_memory),
      atomic_memory_if_(atomic_memory_if),
      owns_memory_(false),
      counter_num_instructions_("num_instructions", 0),
      counter_num_cycles_("num_cycles", 0),
      counter_pc_("pc", 0),
      cap_reg_re_{
          R"((\w+)\.(top|base|length|tag|permissions|object_type|reserved))"} {
  Initialize();
}

CheriotTop::~CheriotTop() {
  // If the simulator is still running, request a halt (set halted_ to true),
  // and wait until the simulator finishes before continuing the destructor.
  if (run_status_ == RunStatus::kRunning) {
    run_halted_->WaitForNotification();
    delete run_halted_;
    run_halted_ = nullptr;
  }

  if (branch_trace_db_ != nullptr) branch_trace_db_->DecRef();

  delete rv_bp_manager_;
  delete cheriot_decode_cache_;
  delete cheriot_decoder_;
  delete state_;
  delete fp_state_;
  delete tagged_watcher_;
  delete atomic_watcher_;
  delete atomic_memory_;
  if (owns_memory_) delete inst_memory_;
}

void CheriotTop::Initialize() {
  // Create the simulation state.
  tagged_watcher_ = new util::TaggedMemoryWatcher(data_memory_);
  atomic_watcher_ = new util::MemoryWatcher(atomic_memory_if_);
  atomic_memory_ = new util::AtomicMemory(atomic_watcher_);
  state_ = new CheriotState(kCheriotName, tagged_watcher_, atomic_memory_);
  fp_state_ = new RiscVCheriotFPState(state_);
  state_->set_rv_fp(fp_state_);
  pcc_ = static_cast<CheriotRegister *>(
      state_->registers()->at(CheriotState::kPcName));
  // Set up the decoder and decode cache.
  cheriot_decoder_ = new CheriotDecoder(state_, inst_memory_);
  // Register instruction opcode counters.
  for (int i = 0; i < static_cast<int>(isa32::OpcodeEnum::kPastMaxValue); i++) {
    counter_opcode_[i].Initialize(absl::StrCat("num_", isa32::kOpcodeNames[i]),
                                  0);
    CHECK_OK(AddCounter(&counter_opcode_[i]))
        << absl::StrCat("Failed to register opcode counter for :'",
                        isa32::kOpcodeNames[i], "'");
  }
  cheriot_decode_cache_ =
      generic::DecodeCache::Create({16 * 1024, 2}, cheriot_decoder_);
  // Register instruction counter.
  CHECK_OK(AddCounter(&counter_num_instructions_))
      << "Failed to register instruction counter";
  // Register pc counter.
  CHECK_OK(AddCounter(&counter_pc_)) << "Failed to register pc counter";

  // Breakpoints.
  rv_action_point_manager_ = new riscv::RiscVActionPointManager(
      inst_memory_, absl::bind_front(&generic::DecodeCache::Invalidate,
                                     cheriot_decode_cache_));
  rv_bp_manager_ = new riscv::RiscVBreakpointManager(
      rv_action_point_manager_,
      [this](HaltReason halt_reason) { RequestHalt(halt_reason, nullptr); });
  // Set the software breakpoint callback.
  state_->AddEbreakHandler([this](const Instruction *inst) {
    if (rv_action_point_manager_->IsActionPointActive(inst->address())) {
      // Need to request a halt so that the action point can be stepped past
      // after executing the actions. However, an action may override the
      // particular halt reason (e.g., breakpoints).
      RequestHalt(HaltReason::kActionPoint, inst);
      rv_action_point_manager_->PerformActions(inst->address());
      return true;
    }
    return false;
  });

  // Make sure the architectural and abi register aliases are added.
  std::string reg_name;
  std::string xreg_name;
  for (int i = 0; i < 32; i++) {
    reg_name = absl::StrCat(CheriotState::kCregPrefix, i);
    (void)state_->AddRegister<CheriotRegister>(reg_name);
    (void)state_->AddRegisterAlias<CheriotRegister>(reg_name,
                                                    kCRegisterAliases[i]);
    (void)state_->AddRegisterAlias<CheriotRegister>(reg_name,
                                                    kXRegisterAliases[i]);
    xreg_name = absl::StrCat(CheriotState::kXregPrefix, i);
    (void)state_->AddRegisterAlias<CheriotRegister>(reg_name, xreg_name);
  }
  for (int i = 0; i < 32; i++) {
    reg_name = absl::StrCat(CheriotState::kFregPrefix, i);
    (void)state_->AddRegister<riscv::RVFpRegister>(reg_name);
    (void)state_->AddRegisterAlias<riscv::RVFpRegister>(reg_name,
                                                        kFRegisterAliases[i]);
  }
  // Branch trace.
  branch_trace_db_ = db_factory_.Allocate<BranchTraceEntry>(kBranchTraceSize);
  branch_trace_ =
      reinterpret_cast<BranchTraceEntry *>(branch_trace_db_->raw_ptr());
  for (int i = 0; i < kBranchTraceSize; i++) {
    branch_trace_[i] = {0, 0, 0};
  }
}

bool CheriotTop::ExecuteInstruction(Instruction *inst) {
  if (!pcc_->tag()) {
    state_->HandleCheriRegException(inst, inst->address(),
                                    EC::kCapExTagViolation, pcc_);
    return true;
  }
  if (!pcc_->HasPermission(PB::kPermitExecute)) {
    state_->HandleCheriRegException(inst, inst->address(),
                                    EC::kCapExPermitExecuteViolation, pcc_);
    return true;
  }
  if (!pcc_->IsInBounds(inst->address(), inst->size())) {
    state_->HandleCheriRegException(inst, inst->address(),
                                    EC::kCapExBoundsViolation, pcc_);
    return true;
  }
  inst->Execute(nullptr);
  counter_pc_.SetValue(inst->address());
  // Comment out instruction logging during execution.
  // LOG(INFO) << "[" << std::hex << inst->address() << "] " <<
  // inst->AsString();
  return true;
}

absl::Status CheriotTop::Halt() {
  // If it is already halted, just return.
  if (run_status_ == RunStatus::kHalted) {
    return absl::OkStatus();
  }
  // If it is not running, then there's an error.
  if (run_status_ != RunStatus::kRunning) {
    return absl::FailedPreconditionError(
        "CheriotTop::Halt: Core is not running");
  }
  halt_reason_ = *HaltReason::kUserRequest;
  halted_ = true;
  return absl::OkStatus();
}

absl::Status CheriotTop::StepPastBreakpoint() {
  uint64_t pc = state_->pc_operand()->AsUint64(0);
  uint64_t bpt_pc = pc;
  // Disable the breakpoint.
  rv_action_point_manager_->WriteOriginalInstruction(pc);
  // Execute the real instruction.
  auto real_inst = cheriot_decode_cache_->GetDecodedInstruction(pc);
  real_inst->IncRef();
  uint64_t next_pc = pc + real_inst->size();
  bool executed = false;
  do {
    executed = ExecuteInstruction(real_inst);
    counter_num_cycles_.Increment(1);
    state_->AdvanceDelayLines();
  } while (!executed);
  // Increment counters.
  counter_opcode_[real_inst->opcode()].Increment(1);
  counter_num_instructions_.Increment(1);
  real_inst->DecRef();
  // Re-enable the breakpoint.
  // Re-enable the breakpoint.
  rv_action_point_manager_->WriteBreakpointInstruction(bpt_pc);
  // Get the next pc value.
  uint64_t pcc_val = pcc_->data_buffer()->Get<uint32_t>(0);
  if (state_->branch()) {
    state_->set_branch(false);
    AddToBranchTrace(pc, pcc_val);
    next_pc = pcc_val;
    if (break_on_control_flow_change_) {
      halted_ = true;
      halt_reason_ = *HaltReason::kHardwareBreakpoint;
    }
  }
  SetPc(next_pc);
  return absl::OkStatus();
}

absl::StatusOr<int> CheriotTop::Step(int num) {
  if (num <= 0) {
    return absl::InvalidArgumentError("Step count must be > 0");
  }
  if (halt_reason_ == *HaltReason::kProgramDone) {
    return absl::FailedPreconditionError("Step: Program has completed.");
  }
  // If the simulator is running, return with an error.
  if (run_status_ != RunStatus::kHalted) {
    return absl::FailedPreconditionError(
        "CheriotTop::Step: Core must be halted");
  }
  run_status_ = RunStatus::kSingleStep;
  int count = 0;
  halted_ = false;
  // First check to see if the previous halt was due to a breakpoint. If so,
  // verify that the breakpoint is there, then step over the breakpoint.
  if (need_to_step_over_) {
    need_to_step_over_ = false;
    auto status = StepPastBreakpoint();
    if (!status.ok()) return status;
    count++;
  }

  // Step the simulator forward until the number of steps have been achieved, or
  // there is a halt request.

  // Clear the halt reason.
  halt_reason_ = *HaltReason::kNone;
  // At the top of the loop this holds the address of the instruction to be
  // executed next. Post-loop it holds the address of the next instruction to
  // be executed.
  uint64_t next_pc = pcc_->data_buffer()->Get<uint32_t>(0);
  // This holds the value of the current pc, and post-loop, the address of
  // the most recently executed instruction.
  uint64_t pc = next_pc;
  while (!halted_ && (count < num)) {
    SetPc(pc);
    auto *inst = cheriot_decode_cache_->GetDecodedInstruction(pc);
    // Set the next_pc to the next sequential instruction.
    next_pc += inst->size();
    bool executed = false;
    do {
      executed = ExecuteInstruction(inst);
      counter_num_cycles_.Increment(1);
      state_->AdvanceDelayLines();
      // Check for interrupt.
      if (state_->is_interrupt_available()) {
        uint64_t epc = pc;
        if (executed) {
          epc = state_->branch() ? pcc_->data_buffer()->Get<uint32_t>(0)
                                 : next_pc;
        }
        state_->TakeAvailableInterrupt(epc);
      }
    } while (!executed);
    count++;
    // Update counters.
    counter_opcode_[inst->opcode()].Increment(1);
    counter_num_instructions_.Increment(1);
    // Get the next pc value.
    uint64_t pcc_val = pcc_->data_buffer()->Get<uint32_t>(0);
    if (state_->branch()) {
      state_->set_branch(false);
      AddToBranchTrace(pc, pcc_val);
      next_pc = pcc_val;
      if (break_on_control_flow_change_) {
        halted_ = true;
        halt_reason_ = *HaltReason::kHardwareBreakpoint;
      }
    }
    if (!halted_) {
      pc = next_pc;
      continue;
    }
    // If it's an action point, just step over and continue.
    if (halt_reason_ == *HaltReason::kActionPoint) {
      auto status = StepPastBreakpoint();
      if (!status.ok()) return status;
      // Reset the halt reason and continue;
      halted_ = false;
      halt_reason_ = *HaltReason::kNone;
      need_to_step_over_ = false;
      pc = state_->pc_operand()->AsUint64(0);
      continue;
    }
    break;
  }
  // Update the pc register, now that it can be read.
  if (halt_reason_ == *HaltReason::kSoftwareBreakpoint) {
    // If at a breakpoint, keep the pc at the current value.
    SetPc(pc);
  } else {
    // Otherwise set it to point to the next instruction.
    SetPc(next_pc);
  }
  // If there is no halt request, there is no specific halt reason.
  if (!halted_) {
    halt_reason_ = *HaltReason::kNone;
  }
  run_status_ = RunStatus::kHalted;
  return count;
}

absl::Status CheriotTop::Run() {
  if (halt_reason_ == *HaltReason::kProgramDone) {
    return absl::FailedPreconditionError("Run: Program has completed.");
  }
  // Verify that the core isn't running already.
  if (run_status_ == RunStatus::kRunning) {
    return absl::FailedPreconditionError(
        "CheriotTop::Run: core is already running");
  }
  // First check to see if the previous halt was due to a breakpoint. If so,
  // need to step over the breakpoint.
  if (need_to_step_over_) {
    need_to_step_over_ = false;
    auto status = StepPastBreakpoint();
    if (!status.ok()) return status;
  }
  run_status_ = RunStatus::kRunning;
  halt_reason_ = *HaltReason::kNone;
  halted_ = false;

  // The simulator is now run in a separate thread so as to allow a user
  // interface to continue operating. Allocate a new run_halted_ Notification
  // object, as they are single use only.
  run_halted_ = new absl::Notification();
  // The thread is detached so it executes without having to be joined.
  std::thread([this]() {
    // At the top of the loop this holds the address of the instruction to be
    // executed next. Post-loop it holds the address of the next instruction to
    // be executed.
    uint64_t next_pc = pcc_->data_buffer()->Get<uint32_t>(0);
    // This holds the value of the current pc, and post-loop, the address of
    // the most recently executed instruction.
    uint64_t pc = next_pc;
    while (!halted_) {
      auto *inst = cheriot_decode_cache_->GetDecodedInstruction(pc);
      SetPc(pc);
      // Set the PC destination operand to next_seq_pc. Any branch that is
      // executed will overwrite this.
      next_pc += inst->size();
      bool executed = false;
      do {
        // Try executing the instruction. If it fails, advance a cycle
        // and try again.
        executed = ExecuteInstruction(inst);
        counter_num_cycles_.Increment(1);
        state_->AdvanceDelayLines();
        // Check for interrupt.
        if (state_->is_interrupt_available()) {
          uint64_t epc = pc;
          if (executed) {
            epc = state_->branch() ? pcc_->data_buffer()->Get<uint32_t>(0)
                                   : next_pc;
          }
          state_->TakeAvailableInterrupt(epc);
        }
      } while (!executed);
      // Update counters.
      counter_opcode_[inst->opcode()].Increment(1);
      counter_num_instructions_.Increment(1);
      // Get the next pc value.
      // Get the next pc value.
      uint64_t pcc_val = pcc_->data_buffer()->Get<uint32_t>(0);
      if (state_->branch()) {
        state_->set_branch(false);
        AddToBranchTrace(pc, pcc_val);
        next_pc = pcc_val;
        if (break_on_control_flow_change_) {
          halted_ = true;
          halt_reason_ = *HaltReason::kHardwareBreakpoint;
        }
      }
      if (!halted_) {
        pc = next_pc;
        continue;
      }
      // If it's an action point, just step over and continue executing, as
      // this is not a full breakpoint.
      if (halt_reason_ == *HaltReason::kActionPoint) {
        auto status = StepPastBreakpoint();
        if (!status.ok()) {
          // If there is an error, signal a simulator error.
          halt_reason_ = *HaltReason::kSimulatorError;
          break;
        };
        // Reset the halt reason and continue;
        halted_ = false;
        halt_reason_ = *HaltReason::kNone;
        pc = state_->pc_operand()->AsUint64(0);
        continue;
      }
      break;
    }
    // Update the pc register, now that it can be read.
    if (halt_reason_ == *HaltReason::kSoftwareBreakpoint) {
      // If at a breakpoint, keep the pc at the current value.
      SetPc(pc);
    } else {
      // Otherwise set it to point to the next instruction.
      SetPc(next_pc);
    }
    run_status_ = RunStatus::kHalted;
    // Notify that the run has completed.
    run_halted_->Notify();
  }).detach();
  return absl::OkStatus();
}

absl::Status CheriotTop::Wait() {
  // If the simulator isn't running, then just return after deleting
  // the notification object.
  if (run_status_ != RunStatus::kRunning) {
    delete run_halted_;
    run_halted_ = nullptr;
    return absl::OkStatus();
  }

  // Wait for the simulator to finish - i.e., a notification on run_halted_.
  run_halted_->WaitForNotification();
  // Now delete the notification object - it is single use only.
  delete run_halted_;
  run_halted_ = nullptr;
  return absl::OkStatus();
}

absl::StatusOr<CheriotTop::RunStatus> CheriotTop::GetRunStatus() {
  return run_status_;
}

absl::StatusOr<CheriotTop::HaltReasonValueType>
CheriotTop::GetLastHaltReason() {
  return halt_reason_;
}

absl::StatusOr<uint64_t> CheriotTop::ReadRegister(const std::string &name) {
  auto iter = state_->registers()->find(name);
  // If the register was not found, see if it refers to a capability component.
  // Capability components are named c<n>.top, c<n>.base, etc.
  if (iter == state_->registers()->end()) {
    std::string component;
    std::string cap_reg_name;
    if (RE2::FullMatch(name, *cap_reg_re_, &cap_reg_name, &component)) {
      iter = state_->registers()->find(cap_reg_name);
      if (iter == state_->registers()->end()) {
        return absl::NotFoundError(
            absl::StrCat("Register '", name, "' not found"));
      }
      auto *cap_reg = static_cast<CheriotRegister *>(iter->second);
      if (component == "top") return cap_reg->top();
      if (component == "base") return cap_reg->base();
      if (component == "length") return cap_reg->length();
      if (component == "tag") return cap_reg->tag();
      if (component == "permissions") return cap_reg->permissions();
      if (component == "object_type") return cap_reg->object_type();
      if (component == "reserved") return cap_reg->reserved();
      return absl::NotFoundError(
          absl::StrCat("Register '", name, "' not found"));
    }
  }
  // Was the register found? If not try CSRs.
  if (iter == state_->registers()->end()) {
    auto result = state_->csr_set()->GetCsr(name);
    if (result.ok()) {
      auto *csr = *result;
      return csr->GetUint32();
    }
    // See if it is $branch_trace_head.
    if (name == "$branch_trace_head") return branch_trace_head_;
    if (name == "$branch_trace_size") return branch_trace_size_;
    if (!result.ok()) {
      return absl::NotFoundError(
          absl::StrCat("Register '", name, "' not found"));
    }
  }

  auto *db = (iter->second)->data_buffer();
  uint64_t value;
  switch (db->size<uint8_t>()) {
    case 1:
      value = static_cast<uint64_t>(db->Get<uint8_t>(0));
      break;
    case 2:
      value = static_cast<uint64_t>(db->Get<uint16_t>(0));
      break;
    case 4:
      value = static_cast<uint64_t>(db->Get<uint32_t>(0));
      break;
    case 8:
      value = static_cast<uint64_t>(db->Get<uint64_t>(0));
      break;
    default:
      return absl::InternalError("Register size is not 1, 2, 4, or 8 bytes");
  }
  return value;
}

absl::Status CheriotTop::WriteRegister(const std::string &name,
                                       uint64_t value) {
  // The registers aren't protected by a mutex, so let's not write them while
  // the simulator is running.
  if (run_status_ != RunStatus::kHalted) {
    return absl::FailedPreconditionError("WriteRegister: Core must be halted");
  }
  auto iter = state_->registers()->find(name);
  // If the register was not found, see if it refers to a capability component.
  // Capability components are named c<n>.top, c<n>.base, etc.
  if (iter == state_->registers()->end()) {
    std::string component;
    std::string cap_reg_name;
    if (RE2::FullMatch(name, *cap_reg_re_, &cap_reg_name, &component)) {
      auto *cap_reg = static_cast<CheriotRegister *>(iter->second);
      if (component == "top") {
        value = std::min<uint64_t>(value, 0x1'0000'0000ULL);
        if (value < cap_reg->base()) {
          return absl::InvalidArgumentError("Top must be greater than base");
        }
        cap_reg->SetBounds(cap_reg->base(), value - cap_reg->base());
        return absl::OkStatus();
      }
      if (component == "base") {
        value = std::min<uint64_t>(value, 0xffff'ffffULL);
        if (value > cap_reg->top()) {
          return absl::InvalidArgumentError("Base must be less than top");
        }
        cap_reg->SetBounds(value, cap_reg->top() - value);
        return absl::OkStatus();
      }
      if (component == "length") {
        value = std::min<uint64_t>(value, 0x1'0000'0000ULL);
        cap_reg->SetBounds(cap_reg->base(), value);
        return absl::OkStatus();
      }
      if (component == "tag") {
        cap_reg->set_tag(static_cast<bool>(value));
        return absl::OkStatus();
      }
      if (component == "permissions") {
        cap_reg->set_permissions(value & PB::kPermitMask);
        return absl::OkStatus();
      }
      if (component == "object_type") {
        cap_reg->set_object_type(value);
        return absl::OkStatus();
      }
      if (component == "reserved") {
        cap_reg->set_reserved(value);
        return absl::OkStatus();
      }
      return absl::NotFoundError(
          absl::StrCat("Register '", name, "' not found"));
    }
  }
  // Was the register found? If not try CSRs.
  if (iter == state_->registers()->end()) {
    auto result = state_->csr_set()->GetCsr(name);
    if (name == "$branch_trace_size") {
      return ResizeBranchTrace(value);
    }
    if (!result.ok()) {
      return absl::NotFoundError(
          absl::StrCat("Register '", name, "' not found"));
    }
    auto *csr = *result;
    csr->Set(static_cast<uint32_t>(value));
  }

  // If stopped at a software breakpoint and the pc is changed, change the
  // halt reason, since the next instruction won't be were we stopped.
  if (((name == "pcc") || (name == "pc")) &&
      (halt_reason_ == *HaltReason::kSoftwareBreakpoint)) {
    halt_reason_ = *HaltReason::kNone;
  }

  auto *db = (iter->second)->data_buffer();
  switch (db->size<uint8_t>()) {
    case 1:
      db->Set<uint8_t>(0, static_cast<uint8_t>(value));
      break;
    case 2:
      db->Set<uint16_t>(0, static_cast<uint16_t>(value));
      break;
    case 4:
      db->Set<uint32_t>(0, static_cast<uint32_t>(value));
      break;
    case 8:
      db->Set<uint64_t>(0, static_cast<uint64_t>(value));
      break;
    default:
      return absl::InternalError("Register size is not 1, 2, 4, or 8 bytes");
  }
  return absl::OkStatus();
}

absl::StatusOr<DataBuffer *> CheriotTop::GetRegisterDataBuffer(
    const std::string &name) {
  // The registers aren't protected by a mutex, so let's not access them while
  // the simulator is running.
  if (run_status_ != RunStatus::kHalted) {
    return absl::FailedPreconditionError(
        "GetRegisterDataBuffer: Core must be halted");
  }
  if (name == "$branch_trace") return branch_trace_db_;
  auto iter = state_->registers()->find(name);
  if (iter == state_->registers()->end()) {
    return absl::NotFoundError(absl::StrCat("Register '", name, "' not found"));
  }
  return iter->second->data_buffer();
}

absl::StatusOr<size_t> CheriotTop::ReadMemory(uint64_t address, void *buffer,
                                              size_t length) {
  if (run_status_ != RunStatus::kHalted) {
    return absl::FailedPreconditionError("ReadMemory: Core must be halted");
  }
  if (address > state_->max_physical_address()) {
    return absl::InvalidArgumentError("Invalid memory address");
  }
  length = std::min(length, state_->max_physical_address() - address + 1);
  auto *db = db_factory_.Allocate(length);
  // Load bypassing any watch points/semihosting.
  state_->tagged_memory()->Load(address, db, nullptr, nullptr);
  std::memcpy(buffer, db->raw_ptr(), length);
  db->DecRef();
  return length;
}

absl::StatusOr<size_t> CheriotTop::ReadTagMemory(uint64_t address, void *buf,
                                                 size_t length) {
  if (run_status_ != RunStatus::kHalted) {
    return absl::FailedPreconditionError("ReadTagMemory: Core must be halted");
  }
  if (address > state_->max_physical_address()) {
    return absl::InvalidArgumentError("Invalid memory address");
  }
  uint64_t length64 = static_cast<uint64_t>(length);
  length = std::min(length64, state_->max_physical_address() - address + 1);
  auto *tag_db = db_factory_.Allocate<uint8_t>(length);
  state_->tagged_memory()->Load(address, nullptr, tag_db, nullptr, nullptr);
  std::memcpy(buf, tag_db->raw_ptr(), length);
  tag_db->DecRef();
  return length;
}

absl::StatusOr<size_t> CheriotTop::WriteMemory(uint64_t address,
                                               const void *buffer,
                                               size_t length) {
  if (run_status_ != RunStatus::kHalted) {
    return absl::FailedPreconditionError("WriteMemory: Core must be halted");
  }
  if (address > state_->max_physical_address()) {
    return absl::InvalidArgumentError("Invalid memory address");
  }
  uint64_t length64 = static_cast<uint64_t>(length);
  length = std::min(length64, state_->max_physical_address() - address + 1);
  auto *db = db_factory_.Allocate(length);
  std::memcpy(db->raw_ptr(), buffer, length);
  // Store bypassing any watch points/semihosting.
  state_->tagged_memory()->Store(address, db);
  db->DecRef();
  return length;
}

bool CheriotTop::HasBreakpoint(uint64_t address) {
  return rv_bp_manager_->HasBreakpoint(address);
}

absl::Status CheriotTop::SetSwBreakpoint(uint64_t address) {
  // Don't try if the simulator is running.
  if (run_status_ != RunStatus::kHalted) {
    return absl::FailedPreconditionError(
        "SetSwBreakpoint: Core must be halted");
  }
  // If there is no breakpoint manager, return an error.
  if (rv_bp_manager_ == nullptr) {
    return absl::InternalError("Breakpoints are not enabled");
  }
  // Try setting the breakpoint.
  return rv_bp_manager_->SetBreakpoint(address);
}

absl::Status CheriotTop::ClearSwBreakpoint(uint64_t address) {
  // Don't try if the simulator is running.
  if (run_status_ != RunStatus::kHalted) {
    return absl::FailedPreconditionError(
        "ClearSwBreakpoint: Core must be halted");
  }
  if (rv_bp_manager_ == nullptr) {
    return absl::InternalError("Breakpoints are not enabled");
  }
  return rv_bp_manager_->ClearBreakpoint(address);
}

absl::Status CheriotTop::ClearAllSwBreakpoints() {
  // Don't try if the simulator is running.
  if (run_status_ != RunStatus::kHalted) {
    return absl::FailedPreconditionError(
        "ClearAllSwBreakpoints: Core must be halted");
  }
  if (rv_bp_manager_ == nullptr) {
    return absl::InternalError("Breakpoints are not enabled");
  }
  rv_bp_manager_->ClearAllBreakpoints();
  return absl::OkStatus();
}

// Methods for Action points forward to the rv_action_point_manager_ methods.

absl::StatusOr<int> CheriotTop::SetActionPoint(
    uint64_t address, absl::AnyInvocable<void(uint64_t, int)> action) {
  if (rv_action_point_manager_ == nullptr) {
    return absl::InternalError("Action points are not enabled");
  }
  auto res = rv_action_point_manager_->SetAction(address, std::move(action));
  if (!res.ok()) return res;
  return res.value();
}

absl::Status CheriotTop::ClearActionPoint(uint64_t address, int id) {
  if (rv_action_point_manager_ == nullptr) {
    return absl::InternalError("Action points are not enabled");
  }
  return rv_action_point_manager_->ClearAction(address, id);
}

absl::Status CheriotTop::EnableAction(uint64_t address, int id) {
  if (rv_action_point_manager_ == nullptr) {
    return absl::InternalError("Action points are not enabled");
  }
  return rv_action_point_manager_->EnableAction(address, id);
}

absl::Status CheriotTop::DisableAction(uint64_t address, int id) {
  if (rv_action_point_manager_ == nullptr) {
    return absl::InternalError("Action points are not enabled");
  }
  return rv_action_point_manager_->DisableAction(address, id);
}

// Set a data watchpoint for the given address range and access type.
absl::Status CheriotTop::SetDataWatchpoint(uint64_t address, size_t length,
                                           AccessType access_type) {
  if ((access_type == AccessType::kLoad) ||
      (access_type == AccessType::kLoadStore)) {
    auto rd_tagged_status = tagged_watcher_->SetLoadWatchCallback(
        util::TaggedMemoryWatcher::AddressRange(address, address + length - 1),
        [this](uint64_t address, int size) {
          set_halt_string(absl::StrFormat(
              "Watchpoint triggered due to load from %08x", address));
          RequestHalt(*HaltReason::kDataWatchPoint, nullptr);
        });
    if (!rd_tagged_status.ok()) return rd_tagged_status;

    auto rd_atomic_status = atomic_watcher_->SetLoadWatchCallback(
        util::MemoryWatcher::AddressRange(address, address + length - 1),
        [this](uint64_t address, int size) {
          set_halt_string(absl::StrFormat(
              "Watchpoint triggered due to load from %08x", address));
          RequestHalt(*HaltReason::kDataWatchPoint, nullptr);
        });
    if (!rd_atomic_status.ok()) {
      // Error recovery - ignore return value.
      (void)tagged_watcher_->ClearLoadWatchCallback(address);
      return rd_atomic_status;
    }
  }
  if ((access_type == AccessType::kStore) ||
      (access_type == AccessType::kLoadStore)) {
    auto wr_tagged_status = tagged_watcher_->SetStoreWatchCallback(
        util::TaggedMemoryWatcher::AddressRange(address, address + length - 1),
        [this](uint64_t address, int size) {
          set_halt_string(absl::StrFormat(
              "Watchpoint triggered due to store to %08x", address));
          RequestHalt(*HaltReason::kDataWatchPoint, nullptr);
        });
    if (!wr_tagged_status.ok()) {
      if (access_type == AccessType::kLoadStore) {
        // Error recovery - ignore return value.
        (void)tagged_watcher_->ClearLoadWatchCallback(address);
        (void)atomic_watcher_->ClearLoadWatchCallback(address);
      }
      return wr_tagged_status;
    }

    auto wr_atomic_status = atomic_watcher_->SetStoreWatchCallback(
        util::MemoryWatcher::AddressRange(address, address + length - 1),
        [this](uint64_t address, int size) {
          set_halt_string(absl::StrFormat(
              "Watchpoint triggered due to store to %08x", address));
          RequestHalt(*HaltReason::kDataWatchPoint, nullptr);
        });
    if (!wr_atomic_status.ok()) {
      // Error recovery - ignore return value.
      (void)tagged_watcher_->ClearStoreWatchCallback(address);
      if (access_type == AccessType::kLoadStore) {
        (void)tagged_watcher_->ClearLoadWatchCallback(address);
        (void)atomic_watcher_->ClearLoadWatchCallback(address);
      }
      return wr_atomic_status;
    }
  }
  return absl::OkStatus();
}

absl::Status CheriotTop::ClearDataWatchpoint(uint64_t address,
                                             AccessType access_type) {
  if ((access_type == AccessType::kLoad) ||
      (access_type == AccessType::kLoadStore)) {
    auto rd_tagged_status = tagged_watcher_->ClearLoadWatchCallback(address);
    if (!rd_tagged_status.ok()) return rd_tagged_status;

    auto rd_atomic_status = atomic_watcher_->ClearLoadWatchCallback(address);
    if (!rd_atomic_status.ok()) return rd_atomic_status;
  }
  if ((access_type == AccessType::kStore) ||
      (access_type == AccessType::kLoadStore)) {
    auto wr_tagged_status = tagged_watcher_->ClearStoreWatchCallback(address);
    if (!wr_tagged_status.ok()) return wr_tagged_status;

    auto wr_atomic_status = atomic_watcher_->ClearStoreWatchCallback(address);
    if (!wr_atomic_status.ok()) return wr_atomic_status;
  }
  return absl::OkStatus();
}

void CheriotTop::SetBreakOnControlFlowChange(bool value) {
  break_on_control_flow_change_ = value;
}

absl::StatusOr<Instruction *> CheriotTop::GetInstruction(uint64_t address) {
  auto inst = cheriot_decode_cache_->GetDecodedInstruction(address);
  return inst;
}

absl::StatusOr<std::string> CheriotTop::GetDisassembly(uint64_t address) {
  // Don't try if the simulator is running.
  if (run_status_ != RunStatus::kHalted) {
    return absl::FailedPreconditionError("GetDissasembly: Core must be halted");
  }

  Instruction *inst = nullptr;
  // If requesting the disassembly for an instruction at an action point, we
  // need to write the original instruction back to memory before getting the
  // disassembly.
  bool inst_swap = rv_action_point_manager_->IsActionPointActive(address);
  if (inst_swap) {
    rv_action_point_manager_->WriteOriginalInstruction(address);
  }
  // Get the decoded instruction.
  inst = cheriot_decode_cache_->GetDecodedInstruction(address);
  auto disasm = inst != nullptr ? inst->AsString() : "Invalid instruction";
  // Swap back if required.
  if (inst_swap) {
    rv_action_point_manager_->WriteBreakpointInstruction(address);
  }
  return disasm;
}

void CheriotTop::RequestHalt(HaltReasonValueType halt_reason,
                             const Instruction *inst) {
  // First set the halt_reason_, then the halt flag.
  halt_reason_ = halt_reason;
  halted_ = true;
  // If the halt reason is either sw breakpoint or action point, set
  // need_to_step_over to true.
  if ((halt_reason_ == *HaltReason::kSoftwareBreakpoint) ||
      (halt_reason_ == *HaltReason::kActionPoint)) {
    need_to_step_over_ = true;
  }
}

void CheriotTop::RequestHalt(HaltReason halt_reason, const Instruction *inst) {
  RequestHalt(*halt_reason, inst);
}

void CheriotTop::SetPc(uint64_t value) {
  if (pcc_->data_buffer()->size<uint8_t>() == 4) {
    pcc_->data_buffer()->Set<uint32_t>(0, static_cast<uint32_t>(value));
  } else {
    pcc_->data_buffer()->Set<uint64_t>(0, value);
  }
}

absl::Status CheriotTop::ResizeBranchTrace(size_t size) {
  if (absl::popcount(size) != 1) {
    return absl::InvalidArgumentError("Invalid size - must be a power of 2");
  }
  auto *new_db = db_factory_.Allocate<BranchTraceEntry>(size);
  auto *new_trace = reinterpret_cast<BranchTraceEntry *>(new_db->raw_ptr());
  if (new_db == nullptr) {
    return absl::InternalError("Failed to allocate new branch trace buffer");
  }
  // Copy entries from the old buffer to the new buffer, but do it so that
  // the most recent entry of the old buffer is at the end of the newly
  // allocated buffer. That way, if the new buffer is smaller, we don't have to
  // do too much special handling.
  int new_index = size - 1;
  int old_index = branch_trace_head_;
  while ((new_index >= 0) && (branch_trace_[old_index].count > 0)) {
    new_trace[new_index] = branch_trace_[old_index];
    new_index--;
    old_index--;
    if (old_index < 0) {
      old_index = branch_trace_size_ - 1;
    }
    // Stop if we get to the beginning of the old trace.
    if (old_index == branch_trace_head_) break;
  }
  while (new_index >= 0) {
    new_trace[new_index] = {0, 0, 0};
    new_index--;
  }
  branch_trace_db_->DecRef();
  branch_trace_db_ = new_db;
  branch_trace_ = new_trace;
  branch_trace_size_ = size;
  branch_trace_mask_ = branch_trace_size_ - 1;
  branch_trace_head_ = branch_trace_mask_;
  return absl::OkStatus();
}

void CheriotTop::AddToBranchTrace(uint64_t from, uint64_t to) {
  // Get the most recent entry.
  auto &entry = branch_trace_[branch_trace_head_];
  // If the branch is the same as the previous, just increment its count.
  if ((from == entry.from) && (to == entry.to)) {
    entry.count++;
    return;
  }
  branch_trace_head_ = (branch_trace_head_ + 1) & branch_trace_mask_;
  branch_trace_[branch_trace_head_] = {static_cast<uint32_t>(from),
                                       static_cast<uint32_t>(to), 1};
}

void CheriotTop::EnableStatistics() {
  for (auto &[unused, counter_ptr] : counter_map()) {
    if (counter_ptr->GetName() == "pc") continue;
    counter_ptr->SetIsEnabled(true);
  }
}

void CheriotTop::DisableStatistics() {
  for (auto &[unused, counter_ptr] : counter_map()) {
    if (counter_ptr->GetName() == "pc") continue;
    counter_ptr->SetIsEnabled(false);
  }
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
