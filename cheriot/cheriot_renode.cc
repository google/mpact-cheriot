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

#include "cheriot/cheriot_renode.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <string_view>

#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "cheriot/cheriot_cli_forwarder.h"
#include "cheriot/cheriot_debug_info.h"
#include "cheriot/cheriot_debug_interface.h"
#include "cheriot/cheriot_decoder.h"
#include "cheriot/cheriot_instrumentation_control.h"
#include "cheriot/cheriot_renode_cli_top.h"
#include "cheriot/cheriot_renode_register_info.h"
#include "cheriot/cheriot_rvv_decoder.h"
#include "cheriot/cheriot_rvv_fp_decoder.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/cheriot_top.h"
#include "cheriot/debug_command_shell.h"
#include "mpact/sim/generic/core_debug_interface.h"
#include "mpact/sim/generic/type_helpers.h"
#include "mpact/sim/proto/component_data.pb.h"
#include "mpact/sim/util/memory/atomic_memory.h"
#include "mpact/sim/util/memory/memory_interface.h"
#include "mpact/sim/util/memory/single_initiator_router.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"
#include "mpact/sim/util/memory/tagged_memory_watcher.h"
#include "mpact/sim/util/memory/tagged_to_untagged_memory_transactor.h"
#include "mpact/sim/util/renode/renode_debug_interface.h"
#include "riscv//riscv_arm_semihost.h"
#include "riscv//riscv_clint.h"
#include "riscv//riscv_counter_csr.h"
#include "riscv//riscv_state.h"
#include "riscv//stoull_wrapper.h"
#include "src/google/protobuf/text_format.h"

::mpact::sim::util::renode::RenodeDebugInterface *CreateMpactSim(
    std::string name, std::string cpu_type,
    ::mpact::sim::util::MemoryInterface *renode_sysbus) {
  auto *top = new ::mpact::sim::cheriot::CheriotRenode(name, renode_sysbus);
  auto status = top->InitializeSimulator(cpu_type);
  if (!status.ok()) {
    delete top;
    return nullptr;
  }
  return top;
}

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::proto::ComponentData;
using ::mpact::sim::riscv::RiscVClint;
using ::mpact::sim::riscv::RiscVCounterCsr;
using ::mpact::sim::riscv::RiscVCounterCsrHigh;
using ::mpact::sim::util::AtomicMemoryOpInterface;
using ::mpact::sim::util::TaggedMemoryWatcher;
using ::mpact::sim::util::TaggedToUntaggedMemoryTransactor;

using HaltReasonValueType =
    ::mpact::sim::generic::CoreDebugInterface::HaltReasonValueType;
using HaltReason = ::mpact::sim::generic::CoreDebugInterface::HaltReason;
using RunStatus = ::mpact::sim::generic::CoreDebugInterface::RunStatus;

constexpr int kCapabilityGranule = 8;

// Configuration names.
constexpr std::string_view kTaggedMemoryBase = "memoryBase";
constexpr std::string_view kTaggedMemorySize = "memorySize";
constexpr std::string_view kRevocationMemoryBase = "revocationMemoryBase";
constexpr std::string_view kClintMMRBase = "clintMMRBase";
constexpr std::string_view kClintPeriod = "clintPeriod";
constexpr std::string_view kCLIPort = "cliPort";
constexpr std::string_view kWaitForCLI = "waitForCLI";
constexpr std::string_view kInstProfile = "instProfile";
constexpr std::string_view kMemProfile = "memProfile";
// Cpu names
constexpr std::string_view kBaseName = "Mpact.Cheriot";
constexpr std::string_view kRvvName = "Mpact.CheriotRvv";
constexpr std::string_view kRvvFpName = "Mpact.CheriotRvvFp";

CheriotRenode::CheriotRenode(std::string name, MemoryInterface *renode_sysbus)
    : name_(name), renode_sysbus_(renode_sysbus) {}

CheriotRenode::~CheriotRenode() {
  // Halt the core just to be safe.
  if (cheriot_top_ != nullptr) (void)cheriot_top_->Halt();
  // Write out instruction profile.
  if (inst_profiler_ != nullptr) {
    std::string inst_profile_file_name =
        absl::StrCat("./mpact_cheriot_", name_, "_inst_profile.csv");
    std::fstream inst_profile_file(inst_profile_file_name.c_str(),
                                   std::ios_base::out);
    if (!inst_profile_file.good()) {
      LOG(ERROR) << "Failed to write profile to file";
    } else {
      inst_profiler_->WriteProfile(inst_profile_file);
    }
    inst_profile_file.close();
  }
  if (mem_profiler_ != nullptr) {
    std::string mem_profile_file_name =
        absl::StrCat("./mpact_cheriot_", name_, "_mem_profile.csv");
    std::fstream mem_profile_file(mem_profile_file_name.c_str(),
                                  std::ios_base::out);
    if (!mem_profile_file.good()) {
      LOG(ERROR) << "Failed to write profile to file";
    } else {
      mem_profiler_->WriteProfile(mem_profile_file);
    }
    mem_profile_file.close();
  }
  // Export counters.
  if (cheriot_top_ != nullptr) {
    auto component_proto = std::make_unique<ComponentData>();
    CHECK_OK(cheriot_top_->Export(component_proto.get()))
        << "Failed to export proto";
    std::string proto_file_name;
    proto_file_name = absl::StrCat("./mpact_cheriot_", name_, ".proto");
    std::fstream proto_file(proto_file_name.c_str(), std::ios_base::out);
    std::string serialized;
    if (!proto_file.good() || !google::protobuf::TextFormat::PrintToString(
                                  *component_proto.get(), &serialized)) {
      LOG(ERROR) << "Failed to write proto to file";
    } else {
      proto_file << serialized;
      proto_file.close();
    }
  }
  // Clean up.
  delete mem_profiler_;
  delete inst_profiler_;
  delete instrumentation_control_;
  delete program_loader_;
  delete cmd_shell_;
  delete socket_cli_;
  delete cheriot_renode_cli_top_;
  delete cheriot_cli_forwarder_;
  delete cheriot_decoder_;
  delete cheriot_state_;
  delete cheriot_top_;
  delete semihost_;
  delete router_;
  delete atomic_memory_;
  delete tagged_memory_;
  delete clint_;
  delete tagged_sysbus_;
}

absl::StatusOr<uint64_t> CheriotRenode::LoadExecutable(
    const char *elf_file_name, bool for_symbols_only) {
  program_loader_ = new ElfProgramLoader(this);
  uint64_t entry_pt = 0;
  if (for_symbols_only) {
    auto res = program_loader_->LoadSymbols(elf_file_name);
    if (!res.ok()) {
      return res.status();
    }
    entry_pt = res.value();
  } else {
    auto res = program_loader_->LoadProgram(elf_file_name);
    if (!res.ok()) {
      return res.status();
    }
    entry_pt = res.value();
  }
  auto res = program_loader_->GetSymbol("tohost");
  // Add watchpoint for tohost if the symbol exists.
  if (res.ok()) {
    // If there is a 'tohost' symbol, set up a write watchpoint on that address
    // to catch writes that mark program exit.
    uint64_t tohost_addr = res.value().first;
    // Add to_host watchpoint that halts the execution when program exit is
    // signaled.
    auto *db = cheriot_top_->state()->db_factory()->Allocate<uint32_t>(2);
    auto status = cheriot_top_->tagged_watcher()->SetStoreWatchCallback(
        TaggedMemoryWatcher::AddressRange{
            tohost_addr, tohost_addr + 2 * sizeof(uint32_t) - 1},
        [this, tohost_addr, db](uint64_t addr, int sz) {
          static DataBuffer *load_db = db;
          if (load_db == nullptr) return;
          tagged_memory_->Load(tohost_addr, load_db, nullptr, nullptr);
          uint32_t code = load_db->Get<uint32_t>(0);
          if (code & 0x1) {
            // The return code is in the upper 31 bits.
            code >>= 1;
            LOG(INFO) << absl::StrCat(
                "Simulation halting due to tohost write: exit ",
                absl::Hex(code));
            (void)cheriot_top_->RequestHalt(HaltReason::kProgramDone, nullptr);
            load_db->DecRef();
          }
        });
  }
  // Add instruction profiler it hasn't already been added.
  if (inst_profiler_ == nullptr) {
    inst_profiler_ = new InstructionProfiler(*program_loader_, 2);
    cheriot_top_->counter_pc()->AddListener(inst_profiler_);
    cheriot_top_->counter_pc()->SetIsEnabled(false);
  } else {
    // If it has been added already, set the elf loader, and make sure the pc
    // counter is enabled.
    inst_profiler_->SetElfLoader(program_loader_);
    cheriot_top_->counter_pc()->SetIsEnabled(true);
  }
  return entry_pt;
}

// Each of the following methods checks to see if the command line enabled
// "top" interface is null. If it is not, it uses that to control the simulator,
// as it provides proper prioritization and handling of both ReNode and command
// line commands. Otherwise it uses the cheriot_top interface directly.

absl::StatusOr<int> CheriotRenode::Step(int num) {
  if (cheriot_renode_cli_top_ != nullptr)
    return cheriot_renode_cli_top_->RenodeStep(num);
  return cheriot_top_->Step(num);
}

absl::StatusOr<HaltReasonValueType> CheriotRenode::GetLastHaltReason() {
  if (cheriot_renode_cli_top_ != nullptr)
    return cheriot_renode_cli_top_->RenodeGetLastHaltReason();
  return cheriot_top_->GetLastHaltReason();
}

// Perform direct read of the memory through the renode router. The renode
// router avoids routing the request back out to the sysbus.
absl::StatusOr<size_t> CheriotRenode::ReadMemory(uint64_t address, void *buf,
                                                 size_t length) {
  auto *db = db_factory_.Allocate<uint8_t>(length);
  renode_router_->Load(address, db, nullptr, nullptr);
  std::memcpy(buf, db->raw_ptr(), length);
  db->DecRef();
  return length;
}

// Perform direct write of the memory through the renode router. The renode
// router avoids routing the request back out to the sysbus.
absl::StatusOr<size_t> CheriotRenode::WriteMemory(uint64_t address,
                                                  const void *buf,
                                                  size_t length) {
  auto *db = db_factory_.Allocate<uint8_t>(length);
  std::memcpy(db->raw_ptr(), buf, length);
  renode_router_->Store(address, db);
  db->DecRef();
  return length;
}

absl::StatusOr<uint64_t> CheriotRenode::ReadRegister(uint32_t reg_id) {
  auto ptr = CheriotDebugInfo::Instance()->debug_register_map().find(reg_id);
  if (ptr == CheriotDebugInfo::Instance()->debug_register_map().end()) {
    return absl::NotFoundError(
        absl::StrCat("Not found reg id: ", absl::Hex(reg_id)));
  }
  if (cheriot_renode_cli_top_ != nullptr)
    return cheriot_renode_cli_top_->RenodeReadRegister(ptr->second);
  return cheriot_top_->ReadRegister(ptr->second);
}

absl::Status CheriotRenode::WriteRegister(uint32_t reg_id, uint64_t value) {
  auto ptr = CheriotDebugInfo::Instance()->debug_register_map().find(reg_id);
  if (ptr == CheriotDebugInfo::Instance()->debug_register_map().end()) {
    return absl::NotFoundError(
        absl::StrCat("Not found reg id: ", absl::Hex(reg_id)));
  }
  if (cheriot_renode_cli_top_ != nullptr)
    return cheriot_renode_cli_top_->RenodeWriteRegister(ptr->second, value);
  return cheriot_top_->WriteRegister(ptr->second, value);
}

int32_t CheriotRenode::GetRenodeRegisterInfoSize() const {
  return CheriotRenodeRegisterInfo::GetRenodeRegisterInfo().size();
}

absl::Status CheriotRenode::GetRenodeRegisterInfo(int32_t index,
                                                  int32_t max_len, char *name,
                                                  RenodeCpuRegister &info) {
  auto const &register_info =
      CheriotRenodeRegisterInfo::GetRenodeRegisterInfo();
  if ((index < 0) || (index >= register_info.size())) {
    return absl::OutOfRangeError(
        absl::StrCat("Register info index (", index, ") out of range"));
  }
  info = register_info[index];
  auto const &reg_map = CheriotDebugInfo::Instance()->debug_register_map();
  auto ptr = reg_map.find(info.index);
  if (ptr == reg_map.end()) {
    name[0] = '\0';
  } else {
    strncpy(name, ptr->second.c_str(), max_len);
  }
  return absl::OkStatus();
}

static absl::StatusOr<uint64_t> ParseNumber(const std::string &number) {
  if (number.empty()) {
    return absl::InvalidArgumentError("Empty number");
  }
  absl::StatusOr<uint64_t> res;
  if ((number.size() > 2) && (number.substr(0, 2) == "0x")) {
    res = riscv::internal::stoull(number.substr(2), nullptr, 16);
  } else if (number[0] == '0') {
    res = riscv::internal::stoull(number.substr(1), nullptr, 8);
  } else {
    res = riscv::internal::stoull(number, nullptr, 16);
  }
  if (!res.ok()) {
    LOG(ERROR) << "Invalid number: " << number;
    return absl::InvalidArgumentError(absl::StrCat("Invalid number: ", number));
  }
  return res.value();
}

absl::Status CheriotRenode::SetConfig(const char *config_names[],
                                      const char *config_values[], int size) {
  uint64_t tagged_memory_base = 0;
  uint64_t tagged_memory_size = 0;
  uint64_t revocation_memory_base = 0;
  uint64_t clint_mmr_base = 0;
  uint64_t clint_period = 100;  // 100 by default.
  bool do_inst_profile = false;
  int cli_port = 0;
  int wait_for_cli = 0;
  for (int i = 0; i < size; ++i) {
    std::string name(config_names[i]);
    std::string str_value = config_values[i];
    auto res = ParseNumber(str_value);
    uint64_t value = 0;
    if (!res.ok()) {
      return res.status();
    }
    value = res.value();
    if (name == kTaggedMemoryBase) {
      tagged_memory_base = value;
    } else if (name == kTaggedMemorySize) {
      tagged_memory_size = value;
    } else if (name == kRevocationMemoryBase) {
      revocation_memory_base = value;
    } else if (name == kClintMMRBase) {
      clint_mmr_base = value;
    } else if (name == kClintPeriod) {
      clint_period = value;
    } else if (name == kCLIPort) {
      cli_port = value;
    } else if (name == kWaitForCLI) {
      wait_for_cli = value;
    } else if (name == kInstProfile) {
      do_inst_profile = value != 0;
    } else if (name == kMemProfile) {
      mem_profiler_->set_is_enabled(value != 0);
    } else {
      LOG(ERROR) << "Unknown config name: " << name << " " << config_values[i];
    }
  }
  if (tagged_memory_size == 0) {
    return absl::InvalidArgumentError("tagged_memory_size is 0");
  }
  // Add the memory targets.
  CHECK_OK(router_->AddTarget<AtomicMemoryOpInterface>(
      atomic_memory_, tagged_memory_base,
      tagged_memory_base + tagged_memory_size - 1));
  CHECK_OK(router_->AddTarget<TaggedMemoryInterface>(
      tagged_memory_, tagged_memory_base,
      tagged_memory_base + tagged_memory_size - 1));
  CHECK_OK(router_->AddTarget<MemoryInterface>(
      tagged_memory_, tagged_memory_base,
      tagged_memory_base + tagged_memory_size - 1));
  // Memory mapped devices.
  if (clint_mmr_base != 0) {
    clint_ = new RiscVClint(clint_period, cheriot_top_->state()->mip());
    cheriot_top_->counter_num_cycles()->AddListener(clint_);
    // Core local interrupt controller - clint.
    CHECK_OK(router_->AddTarget<MemoryInterface>(clint_, clint_mmr_base,
                                                 clint_mmr_base + 0xffffULL));
  }
  // Instruction profiler.
  if (do_inst_profile) {
    if (inst_profiler_ == nullptr) {
      if (program_loader_ == nullptr) {
        // If the program loader is null, assume that it will be added later,
        // but don't enable the trace until it is.
        inst_profiler_ = new InstructionProfiler(2);
        cheriot_top_->counter_pc()->SetIsEnabled(false);
      } else {
        inst_profiler_ = new InstructionProfiler(*program_loader_, 2);
        cheriot_top_->counter_pc()->SetIsEnabled(true);
      }
      cheriot_top_->counter_pc()->AddListener(inst_profiler_);
    }
  }
  // If the cli port has been specified, then instantiate the requisite classes.
  if (cli_port != 0 && (cheriot_renode_cli_top_ == nullptr)) {
    cheriot_renode_cli_top_ =
        new CheriotRenodeCLITop(cheriot_top_, wait_for_cli != 0);
    cheriot_cli_forwarder_ = new CheriotCLIForwarder(cheriot_renode_cli_top_);
    cmd_shell_ = new DebugCommandShell();
    instrumentation_control_ = new CheriotInstrumentationControl(
        cmd_shell_, cheriot_top_, mem_profiler_);
    cmd_shell_->AddCore(
        {static_cast<CheriotDebugInterface *>(cheriot_cli_forwarder_),
         [this]() { return program_loader_; }, cheriot_state_});
    cmd_shell_->AddCommand(
        instrumentation_control_->Usage(),
        absl::bind_front(&CheriotInstrumentationControl::PerformShellCommand,
                         instrumentation_control_));
    socket_cli_ =
        new SocketCLI(cli_port, *cmd_shell_,
                      absl::bind_front(&CheriotRenodeCLITop::SetConnected,
                                       cheriot_renode_cli_top_));
    if (!socket_cli_->good()) {
      return absl::InternalError(
          absl::StrCat("Failed to create socket CLI (", errno, ")"));
    }
  }
  return absl::OkStatus();
}

absl::Status CheriotRenode::SetIrqValue(int32_t irq_num, bool irq_value) {
  switch (irq_num) {
    case *riscv::InterruptCode::kMachineExternalInterrupt:
      cheriot_top_->state()->mip()->set_meip(irq_value);
      return absl::OkStatus();
    case *riscv::InterruptCode::kMachineTimerInterrupt:
      cheriot_top_->state()->mip()->set_mtip(irq_value);
      return absl::OkStatus();
    case *riscv::InterruptCode::kMachineSoftwareInterrupt:
      cheriot_top_->state()->mip()->set_msip(irq_value);
      return absl::OkStatus();
    default:
      return absl::NotFoundError(
          absl::StrCat("Unsupported irq number: ", irq_num));
  }
}

absl::Status CheriotRenode::InitializeSimulator(const std::string &cpu_type) {
  router_ = new mpact::sim::util::SingleInitiatorRouter(name_ + "_router");
  renode_router_ =
      new mpact::sim::util::SingleInitiatorRouter(name_ + "_renode_router");
  auto *data_memory = static_cast<TaggedMemoryInterface *>(router_);
  // Instantiate memory profiler, but disable it until the config information
  // has been received.
  mem_profiler_ = new TaggedMemoryUseProfiler(data_memory);
  data_memory = mem_profiler_;
  mem_profiler_->set_is_enabled(false);
  cheriot_state_ = new CheriotState(
      "CherIoT", data_memory, static_cast<AtomicMemoryOpInterface *>(router_));
  // First create the decoder according to the cpu type.
  if (cpu_type == kBaseName) {
    cheriot_decoder_ = new CheriotDecoder(
        cheriot_state_, static_cast<MemoryInterface *>(router_));
    cpu_type_ = CheriotCpuType::kBase;
  } else if (cpu_type == kRvvName) {
    cheriot_decoder_ = new CheriotRVVDecoder(
        cheriot_state_, static_cast<MemoryInterface *>(router_));
    cpu_type_ = CheriotCpuType::kRvv;
  } else if (cpu_type == kRvvFpName) {
    cheriot_decoder_ = new CheriotRVVFPDecoder(
        cheriot_state_, static_cast<MemoryInterface *>(router_));
    cpu_type_ = CheriotCpuType::kRvvFp;
  } else {
    return absl::NotFoundError(
        absl::StrCat("Cpu type '", cpu_type, "' must be one of: '", kBaseName,
                     "', '", kRvvName, "', '", kRvvFpName, "'"));
  }
  // Instantiate cheriot_top.
  cheriot_top_ = new CheriotTop("Cheriot", cheriot_state_, cheriot_decoder_);
  // Initialize minstret/minstreth. Bind the instruction counter to those
  // registers.
  auto minstret_res = cheriot_top_->state()->csr_set()->GetCsr("minstret");
  auto minstreth_res = cheriot_top_->state()->csr_set()->GetCsr("minstreth");
  if (!minstret_res.ok() || !minstreth_res.ok()) {
    return absl::InternalError(
        absl::StrCat(name_, ": Error while initializing minstret/minstreth\n"));
  }
  auto *minstret = static_cast<RiscVCounterCsr<uint32_t, CheriotState> *>(
      minstret_res.value());
  auto *minstreth =
      static_cast<RiscVCounterCsrHigh<CheriotState> *>(minstreth_res.value());
  minstret->set_counter(cheriot_top_->counter_num_instructions());
  minstreth->set_counter(cheriot_top_->counter_num_instructions());
  // Initialize mcycle/mcycleh. Bind the instruction counter to those
  // registers.
  auto mcycle_res = cheriot_top_->state()->csr_set()->GetCsr("mcycle");
  auto mcycleh_res = cheriot_top_->state()->csr_set()->GetCsr("mcycleh");
  if (!mcycle_res.ok() || !mcycleh_res.ok()) {
    return absl::InternalError(
        absl::StrCat(name_, ": Error while initializing mcycle/mcycleh\n"));
  }
  auto *mcycle = static_cast<RiscVCounterCsr<uint32_t, CheriotState> *>(
      mcycle_res.value());
  auto *mcycleh =
      static_cast<RiscVCounterCsrHigh<CheriotState> *>(mcycleh_res.value());
  mcycle->set_counter(cheriot_top_->counter_num_cycles());
  mcycleh->set_counter(cheriot_top_->counter_num_cycles());
  // Set up the memory router with the system bus. Other devices are added once
  // config info has been received. Add a tagged default memory transactor, so
  // that any tagged loads/stores are forward to the sysbus without tags.
  tagged_sysbus_ = new TaggedToUntaggedMemoryTransactor(renode_sysbus_);
  auto status = router_->AddDefaultTarget<MemoryInterface>(renode_sysbus_);
  if (!status.ok()) return status;
  status = router_->AddDefaultTarget<TaggedMemoryInterface>(tagged_sysbus_);
  if (!status.ok()) return status;

  // Create memory. These memories will be added to the core router when there
  // is configuration data for the address space that belongs to the core. The
  // memories will be added to the renode router immediately as the default
  // target, since memory references from ReNode are only in the memory range
  // exposed on the sysbus.
  tagged_memory_ =
      new mpact::sim::util::TaggedFlatDemandMemory(kCapabilityGranule);
  atomic_memory_ = new mpact::sim::util::AtomicMemory(tagged_memory_);

  // Need to set up the renode router with the tagged_memory.
  status =
      renode_router_->AddDefaultTarget<TaggedMemoryInterface>(tagged_memory_);
  if (!status.ok()) return status;
  status = renode_router_->AddDefaultTarget<MemoryInterface>(tagged_memory_);
  if (!status.ok()) return status;

  // Set up semihosting.
  semihost_ =
      new RiscVArmSemihost(RiscVArmSemihost::BitWidth::kWord32,
                           static_cast<MemoryInterface *>(router_),
                           static_cast<MemoryInterface *>(renode_router_));
  // Set up special handlers (ebreak, wfi, ecall).
  cheriot_top_->state()->AddEbreakHandler([this](const Instruction *inst) {
    if (this->semihost_->IsSemihostingCall(inst)) {
      this->semihost_->OnEBreak(inst);
      return true;
    }
    if (this->cheriot_top_->HasBreakpoint(inst->address())) {
      this->cheriot_top_->RequestHalt(HaltReason::kSoftwareBreakpoint, nullptr);
      return true;
    }
    return false;
  });
  cheriot_top_->state()->set_on_wfi([](const Instruction *) { return true; });
  cheriot_top_->state()->set_on_ecall(
      [](const Instruction *) { return false; });
  semihost_->set_exit_callback([this]() {
    this->cheriot_top_->RequestHalt(HaltReason::kProgramDone, nullptr);
  });
  return absl::OkStatus();
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
