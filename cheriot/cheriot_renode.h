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

#ifndef MPACT_CHERIOT__CHERIOT_RENODE_H_
#define MPACT_CHERIOT__CHERIOT_RENODE_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cheriot/cheriot_cli_forwarder.h"
#include "cheriot/cheriot_instrumentation_control.h"
#include "cheriot/cheriot_renode_cli_top.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/cheriot_top.h"
#include "cheriot/debug_command_shell.h"
#include "mpact/sim/generic/core_debug_interface.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/util/memory/atomic_memory.h"
#include "mpact/sim/util/memory/memory_interface.h"
#include "mpact/sim/util/memory/memory_use_profiler.h"
#include "mpact/sim/util/memory/single_initiator_router.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"
#include "mpact/sim/util/memory/tagged_memory_interface.h"
#include "mpact/sim/util/other/instruction_profiler.h"
#include "mpact/sim/util/program_loader/elf_program_loader.h"
#include "mpact/sim/util/renode/renode_debug_interface.h"
#include "mpact/sim/util/renode/socket_cli.h"
#include "riscv//riscv_arm_semihost.h"
#include "riscv//riscv_clint.h"

// This file defines a wrapper class for the CheriotTop that adds Arm
// semihosting. In addition, the .cc class defines a global namespace
// function that is used by the renode wrapper to create a top simulator
// instance.
//
// In addition, when the configuration specifies a command line interface port,
// an object of the SocketCLI class is instantiated to provide a command line
// interface accessible over a socket. In this case the wrapper no longer
// directly calls the top simulator control class, but routes the calls through
// a combined ReNode/CLI interface that manages the priorities and access of
// ReNode and command line commands to the simulator control class.

extern ::mpact::sim::util::renode::RenodeDebugInterface *CreateMpactSim(
    std::string name, ::mpact::sim::util::MemoryInterface *renode_sysbus);

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::DataBufferFactory;
using ::mpact::sim::riscv::RiscVArmSemihost;
using ::mpact::sim::riscv::RiscVClint;
using ::mpact::sim::util::AtomicMemory;
using ::mpact::sim::util::ElfProgramLoader;
using ::mpact::sim::util::InstructionProfiler;
using ::mpact::sim::util::MemoryInterface;
using ::mpact::sim::util::SingleInitiatorRouter;
using ::mpact::sim::util::TaggedFlatDemandMemory;
using ::mpact::sim::util::TaggedMemoryInterface;
using ::mpact::sim::util::TaggedMemoryUseProfiler;
using ::mpact::sim::util::renode::SocketCLI;

class CheriotRenode : public util::renode::RenodeDebugInterface {
 public:
  // Supported IRQ request types.
  enum class IrqType {
    kMachineSoftwareInterrupt = 0x3,
    kMachineExternalInterrupt = 0xb,
  };

  enum RenodeState {
    kIdle = 0,
    kStepping = 1,
    kRunning = 2,
  };

  enum CLIState {
    kDisconnected = 0,
    kConnected = 1,
  };

  enum class CheriotCpuType {
    kBase = 0,
    kRvv = 1,
    kRvvFp = 2,
  };

  using ::mpact::sim::generic::CoreDebugInterface::HaltReason;
  using ::mpact::sim::generic::CoreDebugInterface::RunStatus;
  using RenodeCpuRegister = ::mpact::sim::util::renode::RenodeCpuRegister;

  // Constructor takes a name and a memory interface that is used for memory
  // transactions routed to the system bus.
  CheriotRenode(std::string name, MemoryInterface *renode_sysbus);
  ~CheriotRenode() override;

  absl::StatusOr<uint64_t> LoadExecutable(const char *elf_file_name,
                                          bool for_symbols_only) override;
  // Step the core by num instructions.
  absl::StatusOr<int> Step(int num) override;
  // Returns the reason for the most recent halt.
  absl::StatusOr<HaltReasonValueType> GetLastHaltReason() override;
  // Read/write the numeric id registers.
  absl::StatusOr<uint64_t> ReadRegister(uint32_t reg_id) override;
  absl::Status WriteRegister(uint32_t reg_id, uint64_t value) override;
  // Get register data buffer call. Not implemented, stubbed out to return null.
  // Read/write the buffers to memory.
  absl::StatusOr<size_t> ReadMemory(uint64_t address, void *buf,
                                    size_t length) override;
  absl::StatusOr<size_t> WriteMemory(uint64_t address, const void *buf,
                                     size_t length) override;
  // Return register information.
  int32_t GetRenodeRegisterInfoSize() const override;
  absl::Status GetRenodeRegisterInfo(int32_t index, int32_t max_len, char *name,
                                     RenodeCpuRegister &info) override;

  // Set configuration value.
  absl::Status SetConfig(const char *config_names[],
                         const char *config_values[], int size) override;

  // Set IRQ value for supported IRQs. Supported irq_nums are:
  //          MachineSoftwareInterrupt = 0x3
  //          handled by the clint for now: MachineTimerInterrupt = 0x7
  //          MachineExternalInterrupt = 0xb
  // These correspond to the msip and meip bits of the mip register.
  absl::Status SetIrqValue(int32_t irq_num, bool irq_value) override;

  absl::Status InitializeSimulator(const std::string &cpu_type);

 private:
  std::string name_;
  MemoryInterface *renode_sysbus_ = nullptr;
  TaggedMemoryInterface *data_memory_ = nullptr;
  TaggedMemoryInterface *tagged_sysbus_ = nullptr;
  CheriotState *cheriot_state_ = nullptr;
  DecoderInterface *cheriot_decoder_ = nullptr;
  CheriotTop *cheriot_top_ = nullptr;
  RiscVArmSemihost *semihost_ = nullptr;
  SingleInitiatorRouter *router_ = nullptr;
  SingleInitiatorRouter *renode_router_ = nullptr;
  DataBufferFactory db_factory_;
  AtomicMemory *atomic_memory_ = nullptr;
  TaggedFlatDemandMemory *tagged_memory_ = nullptr;
  RiscVClint *clint_ = nullptr;
  SocketCLI *socket_cli_ = nullptr;
  CheriotRenodeCLITop *cheriot_renode_cli_top_ = nullptr;
  CheriotCLIForwarder *cheriot_cli_forwarder_ = nullptr;
  ElfProgramLoader *program_loader_ = nullptr;
  DebugCommandShell *cmd_shell_ = nullptr;
  InstructionProfiler *inst_profiler_ = nullptr;
  TaggedMemoryUseProfiler *mem_profiler_ = nullptr;
  CheriotInstrumentationControl *instrumentation_control_ = nullptr;
  CheriotCpuType cpu_type_ = CheriotCpuType::kBase;
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__CHERIOT_RENODE_H_
