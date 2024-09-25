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

#ifndef MPACT_CHERIOT__CHERIOT_TOP_H_
#define MPACT_CHERIOT__CHERIOT_TOP_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "cheriot/cheriot_debug_interface.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "mpact/sim/generic/action_point_manager_base.h"
#include "mpact/sim/generic/breakpoint_manager.h"
#include "mpact/sim/generic/component.h"
#include "mpact/sim/generic/core_debug_interface.h"
#include "mpact/sim/generic/counters.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/decode_cache.h"
#include "mpact/sim/generic/decoder_interface.h"
#include "mpact/sim/util/memory/cache.h"
#include "mpact/sim/util/memory/memory_interface.h"
#include "mpact/sim/util/memory/memory_watcher.h"
#include "mpact/sim/util/memory/tagged_memory_watcher.h"
#include "re2/re2.h"
#include "riscv//riscv_action_point_memory_interface.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::ActionPointManagerBase;
using ::mpact::sim::generic::BreakpointManager;
using ::mpact::sim::generic::DecoderInterface;
using ::mpact::sim::riscv::RiscVActionPointMemoryInterface;
using ::mpact::sim::util::Cache;

struct BranchTraceEntry {
  uint32_t from;
  uint32_t to;
  uint32_t count;
};

// Top level class for the CherIoT simulator. This is the main interface for
// interacting and controlling execution of programs running on the simulator.
// This class brings together the decoder, the architecture state, and control.
class CheriotTop : public generic::Component, public CheriotDebugInterface {
 public:
  static constexpr int kBranchTraceSize = 16;
  using RunStatus = generic::CoreDebugInterface::RunStatus;
  using HaltReason = generic::CoreDebugInterface::HaltReason;

  CheriotTop(std::string name, CheriotState *state, DecoderInterface *decoder);
  ~CheriotTop() override;

  // Methods inherited from CoreDebugInterface.
  absl::Status Halt() override;
  absl::Status Halt(HaltReason halt_reason) override;
  absl::Status Halt(HaltReasonValueType halt_reason) override;
  absl::StatusOr<int> Step(int num) override;
  absl::Status Run() override;
  absl::Status Wait() override;

  absl::StatusOr<RunStatus> GetRunStatus() override;
  absl::StatusOr<HaltReasonValueType> GetLastHaltReason() override;

  // Register access by register name.
  absl::StatusOr<uint64_t> ReadRegister(const std::string &name) override;
  absl::Status WriteRegister(const std::string &name, uint64_t value) override;
  absl::StatusOr<generic::DataBuffer *> GetRegisterDataBuffer(
      const std::string &name) override;
  // Read and Write memory methods bypass any semihosting.
  absl::StatusOr<size_t> ReadMemory(uint64_t address, void *buf,
                                    size_t length) override;
  absl::StatusOr<size_t> WriteMemory(uint64_t address, const void *buf,
                                     size_t length) override;
  absl::StatusOr<size_t> ReadTagMemory(uint64_t address, void *buf,
                                       size_t length) override;

  // Breakpoints.
  bool HasBreakpoint(uint64_t address) override;
  absl::Status SetSwBreakpoint(uint64_t address) override;
  absl::Status ClearSwBreakpoint(uint64_t address) override;
  absl::Status ClearAllSwBreakpoints() override;

  // Action points.
  absl::StatusOr<int> SetActionPoint(
      uint64_t address,
      absl::AnyInvocable<void(uint64_t, int)> action) override;
  absl::Status ClearActionPoint(uint64_t address, int id) override;
  absl::Status EnableAction(uint64_t address, int id) override;
  absl::Status DisableAction(uint64_t address, int id) override;
  // Watch points.
  absl::Status SetDataWatchpoint(uint64_t address, size_t length,
                                 AccessType access_type) override;
  absl::Status ClearDataWatchpoint(uint64_t address,
                                   AccessType access_type) override;
  void SetBreakOnControlFlowChange(bool value) override;

  // If successful, returns a pointer to the instruction at the given address.
  // The instruction object is IncRef'ed, and the caller must DecRef the object
  // when it is done with it.
  absl::StatusOr<Instruction *> GetInstruction(uint64_t address) override;
  absl::StatusOr<std::string> GetDisassembly(uint64_t address) override;

  // Called when a halt is requested.
  void RequestHalt(HaltReason halt_reason, const Instruction *inst);
  void RequestHalt(HaltReasonValueType halt_reason, const Instruction *inst);

  // Resize branch trace.
  absl::Status ResizeBranchTrace(size_t size);

  // Enable/disable the registered statistics counters.
  void EnableStatistics();
  void DisableStatistics();

  // Accessors.
  CheriotState *state() const { return state_; }
  // The following are not const as callers may need to call non-const methods
  // of the counter.
  generic::SimpleCounter<uint64_t> *counter_num_instructions() {
    return &counter_num_instructions_;
  }
  generic::SimpleCounter<uint64_t> *counter_num_cycles() {
    return &counter_num_cycles_;
  }
  generic::SimpleCounter<uint64_t> *counter_pc() { return &counter_pc_; }
  // Memory watchers used for data watch points.
  util::TaggedMemoryWatcher *tagged_watcher() { return tagged_watcher_; }
  util::MemoryWatcher *memory_watcher() { return memory_watcher_; }

  const std::string &halt_string() const { return halt_string_; }
  void set_halt_string(std::string halt_string) { halt_string_ = halt_string; }

 private:
  // Initialize the top.
  void Initialize();
  // Execute instruction. Returns true if the instruction was executed (or
  // an exception was triggered).
  bool ExecuteInstruction(Instruction *inst);
  // Helper method to step past a breakpoint.
  absl::Status StepPastBreakpoint();
  // Set the pc value.
  void SetPc(uint64_t value);
  void ICacheFetch(uint64_t address);
  // Branch tracing.
  void AddToBranchTrace(uint64_t from, uint64_t to);
  // The DB factory is used to manage data buffers for memory read/writes.
  generic::DataBufferFactory db_factory_;
  // Current status and last halt reasons.
  RunStatus run_status_ = RunStatus::kHalted;
  HaltReasonValueType halt_reason_ = *HaltReason::kNone;
  // Halting flag. This is set to true when execution must halt.
  bool halted_ = false;
  absl::Notification *run_halted_ = nullptr;
  // The local CherIoT state.
  CheriotState *state_;
  // Flag that indicates an instruction needs to be stepped over.
  bool need_to_step_over_ = false;
  // Action point memory interface.
  RiscVActionPointMemoryInterface *rv_ap_memory_if_ = nullptr;
  // Action point manager.
  ActionPointManagerBase *rv_ap_manager_ = nullptr;
  // Breakpoint manager.
  BreakpointManager *rv_bp_manager_ = nullptr;
  // Textual description of halt reason.
  std::string halt_string_;
  // The pc register instance.
  CheriotRegister *pcc_;
  // RiscV32 decoder instance.
  generic::DecoderInterface *cheriot_decoder_ = nullptr;
  // Decode cache, memory and memory watcher.
  generic::DecodeCache *cheriot_decode_cache_ = nullptr;
  util::AtomicMemoryOpInterface *atomic_memory_ = nullptr;
  util::TaggedMemoryWatcher *tagged_watcher_ = nullptr;
  util::MemoryWatcher *memory_watcher_ = nullptr;
  // Branch trace info - uses a circular buffer. The size is defined by the
  // constant kBranchTraceSize in the .cc file.
  BranchTraceEntry *branch_trace_;
  // Data buffer used to hold the branch trace info. This is used so that it
  // can be returned to the debug command shell using the GetRegisterDataBuffer
  // call.
  DataBuffer *branch_trace_db_ = nullptr;
  // Points to the most recently written entry in the circular buffer.
  int branch_trace_head_ = 0;
  int branch_trace_mask_ = kBranchTraceSize - 1;
  int branch_trace_size_ = kBranchTraceSize;
  // Counter for the number of instructions simulated.
  std::vector<generic::SimpleCounter<uint64_t>> counter_opcode_;
  generic::SimpleCounter<uint64_t> counter_num_instructions_;
  generic::SimpleCounter<uint64_t> counter_num_cycles_;
  // Counter used for profiling by connecting it to a profiler. This allows
  // the pc to be written to the counter, and the profiling can be enabled/
  // disabled with the other counters.
  generic::SimpleCounter<uint64_t> counter_pc_;
  absl::flat_hash_map<uint32_t, std::string> register_id_map_;
  // RegEx for parsing capability register component name.
  LazyRE2 cap_reg_re_;
  // Flag for breaking on a control flow change.
  bool break_on_control_flow_change_ = false;
  // ICache.
  Cache *icache_ = nullptr;
  DataBuffer *inst_db_ = nullptr;
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__CHERIOT_TOP_H_
