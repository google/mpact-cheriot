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

#include <signal.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "cheriot/cheriot_instrumentation_control.h"
#include "cheriot/cheriot_top.h"
#include "cheriot/debug_command_shell.h"
#include "cheriot/memory_use_profiler.h"
#include "cheriot/profiler.h"
#include "cheriot/riscv_cheriot_minstret.h"
#include "mpact/sim/generic/core_debug_interface.h"
#include "mpact/sim/generic/counters.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/proto/component_data.pb.h"
#include "mpact/sim/util/memory/atomic_memory.h"
#include "mpact/sim/util/memory/memory_interface.h"
#include "mpact/sim/util/memory/memory_watcher.h"
#include "mpact/sim/util/memory/single_initiator_router.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"
#include "mpact/sim/util/memory/tagged_memory_interface.h"
#include "mpact/sim/util/memory/tagged_memory_watcher.h"
#include "mpact/sim/util/other/simple_uart.h"
#include "mpact/sim/util/program_loader/elf_program_loader.h"
#include "re2/re2.h"
#include "riscv//riscv_arm_semihost.h"
#include "riscv//riscv_clint.h"
#include "src/google/protobuf/text_format.h"

using ::mpact::sim::proto::ComponentData;
using AddressRange = mpact::sim::util::MemoryWatcher::AddressRange;
using ::mpact::sim::cheriot::CheriotInstrumentationControl;
using ::mpact::sim::cheriot::Profiler;
using ::mpact::sim::cheriot::TaggedMemoryUseProfiler;

// Flags for specifying interactive mode.
ABSL_FLAG(bool, i, false, "Interactive mode");
ABSL_FLAG(bool, interactive, false, "Interactive mode");
// Flag for destination directory of proto file.
ABSL_FLAG(std::string, output_dir, "", "Output directory");
// The following defines the optional flag for setting the stack size. If the
// stack size is not set using the flag, then the simulator will look in the
// executable to see if the GNU_STACK segment exists (assuming gcc RiscV
// compiler), and use that size. If not, it will use the value of the symbol
// __stack_size in the executable. If no such symbol exists, the stack size will
// be 32KB.
//
// A symbol may be defined in a C/C++ source file using asm, such as:
// asm(".global __stack_size\n"
//     ".equ __stack_size, 32 * 1024\n");
// The asm statement need not be inside a function body.
//
// The program header entry may be generated by adding the following to the
// gcc/g++ command line: -Wl,z,stack-size=N
//
ABSL_FLAG(std::optional<uint64_t>, stack_size, 32 * 1024,
          "Size of software stack");
// Optional flag for setting the location of the end of the stack (bottom). The
// beginning stack pointer is the value stack_end + stack_size. If this option
// is not set, it will use the value of the symbol __stack_end in the
// executable. If no such symbol exists, stack pointer initialization will not
// be performed by the simulator, and an appropriate crt0 library has to be
// used.
//
// A symbol may be defined in a C/C++ source file using asm, such as:
// asm(".global __stack_end\n"
//     ".equ __stack_end, 0x200000\n");
// The asm statement need not be inside a function body.
ABSL_FLAG(std::optional<uint64_t>, stack_end, 0,
          "Lowest valid address of software stack. "
          "Top of stack is stack_end + stack_size.");

// The following macro can be used in source code to define both the stack size
// and location:
//
// #define __STACK(addr, size) \
//  asm(".global __stack_size\n.equ __stack_size, " #size "\n"); \
//  asm(".global __stack_end\n.equ __stack_end, " #addr "\n");
//
// E.g.
//
// #include <stdio>
//
// __STACK(0x20000, 32 * 1024);
//
// int main(int, char **) {
//   printf("Hello World\n");
//   return 0;
// }
//

// Flag to exit on any exception and print the relevant exception information.
ABSL_FLAG(bool, exit_on_exception, false, "Exit on exception");

// Enable instruction profiling.
ABSL_FLAG(bool, inst_profile, false, "Enable instruction profiling");

// Enable memory use profiling.
ABSL_FLAG(bool, mem_profile, false, "Enable memory use profiling");

constexpr char kStackEndSymbolName[] = "__stack_end";
constexpr char kStackSizeSymbolName[] = "__stack_size";

constexpr int kCapabilityGranule = 8;

using HaltReason = ::mpact::sim::generic::CoreDebugInterface::HaltReason;
using ::mpact::sim::cheriot::CheriotTop;
using ::mpact::sim::cheriot::RiscVCheriotMInstret;
using ::mpact::sim::cheriot::RiscVCheriotMInstreth;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::riscv::RiscVArmSemihost;
using ::mpact::sim::riscv::RiscVClint;
using ::mpact::sim::util::AtomicMemoryOpInterface;
using ::mpact::sim::util::MemoryInterface;
using ::mpact::sim::util::SimpleUart;
using ::mpact::sim::util::TaggedMemoryInterface;
using ::mpact::sim::util::TaggedMemoryWatcher;

// Static pointer to the top instance. Used by the control-C handler.
static CheriotTop *top = nullptr;

// Control-c handler to interrupt any running simulation.
static void sim_sigint_handler(int arg) {
  if (top != nullptr) {
    (void)top->Halt();
    return;
  } else {
    exit(-1);
  }
}

// This is an example custom command that is added to the interactive
// debug command shell.
static bool PrintRegisters(
    absl::string_view input,
    const mpact::sim::cheriot::DebugCommandShell::CoreAccess &core_access,
    std::string &output) {
  static const LazyRE2 reg_info_re{R"(\s*xyzreg\s+info\s*)"};
  if (!RE2::FullMatch(input, *reg_info_re)) return false;

  std::string output_str;
  for (int i = 0; i < 32; i++) {
    std::string reg_name = absl::StrCat("x", i);
    auto result = core_access.debug_interface->ReadRegister(reg_name);
    if (!result.ok()) {
      output = absl::StrCat("Failed to read register '", reg_name, "'");
      return true;
    }
    output_str +=
        absl::StrCat("x", absl::Dec(i, absl::kZeroPad2), " = [",
                     absl::Hex(result.value(), absl::kZeroPad8), "]\n");
  }
  output = output_str;
  return true;
}

// Trap handler.
bool HandleSimulatorTrap(bool is_interrupt, uint64_t trap_value, uint64_t ec,
                         uint64_t epc, const Instruction *instruction) {
  if (is_interrupt) return false;
  std::cerr << absl::StrCat(
      "Exception\n"
      " trapvalue: ",
      absl::Hex(trap_value, absl::kZeroPad8),
      "\n"
      " code: ",
      absl::Hex(ec, absl::kZeroPad8),
      "\n"
      " epc: ",
      absl::Hex(epc, absl::kZeroPad8),
      "\n"
      " inst: ",
      instruction == nullptr ? "nullptr" : instruction->AsString(), "\n");
  // Halt the simulation.
  if (top != nullptr) (void)top->Halt();
  return false;
}

// Main function for the simulator.
int main(int argc, char **argv) {
  absl::SetProgramUsageMessage(argv[0]);
  auto arg_vec = absl::ParseCommandLine(argc, argv);

  if (arg_vec.size() > 2) {
    std::cerr << "Only a single input file allowed" << std::endl;
    return -1;
  }
  if (arg_vec.size() < 2) {
    std::cerr << "Must specify input file" << std::endl;
    return -1;
  }
  std::string full_file_name = arg_vec[1];
  std::string file_name =
      full_file_name.substr(full_file_name.find_last_of('/') + 1);
  std::string file_basename = file_name.substr(0, file_name.find_first_of('.'));

  auto *tagged_memory =
      new mpact::sim::util::TaggedFlatDemandMemory(kCapabilityGranule);
  // Load the elf segments into memory.
  mpact::sim::util::ElfProgramLoader elf_loader(tagged_memory);
  auto load_result = elf_loader.LoadProgram(full_file_name);
  if (!load_result.ok()) {
    std::cerr << "Error while loading '" << full_file_name
              << "': " << load_result.status().message();
    return -1;
  }
  auto *router = new mpact::sim::util::SingleInitiatorRouter("router");
  TaggedMemoryInterface *data_memory =
      static_cast<TaggedMemoryInterface *>(router);
  TaggedMemoryUseProfiler *memory_use_profiler = nullptr;
  // Check to see if memory use profiling is enabled, and if so, set it up.
  if (absl::GetFlag(FLAGS_mem_profile)) {
    memory_use_profiler = new TaggedMemoryUseProfiler(data_memory);
    // Disable until program execution.
    memory_use_profiler->set_is_enabled(false);
    data_memory = memory_use_profiler;
  }
  CheriotTop cheriot_top("Cheriot", static_cast<MemoryInterface *>(router),
                         data_memory, static_cast<MemoryInterface *>(router));

  // Enable instruction profiling if the flag is set.
  Profiler *inst_profiler = nullptr;
  if (absl::GetFlag(FLAGS_inst_profile)) {
    inst_profiler = new Profiler(elf_loader, 2);
    cheriot_top.counter_pc()->AddListener(inst_profiler);
  } else {
    cheriot_top.counter_pc()->SetIsEnabled(false);
  }

  mpact::sim::generic::DataBuffer *db = nullptr;

  // If tohost exists, add a memory watcher to look for exit signal.
  auto tohost_res = elf_loader.GetSymbol("tohost");
  uint64_t tohost_addr = 0;
  if (tohost_res.ok()) {
    // tohost is declared as uint32_t tohost[2]. Writing an lsb of 1
    // terminates the simulation. The upper 31 bits can pass extra metadata.
    // Use all 0s to indicate success.
    tohost_addr = tohost_res.value().first;
    // Add to_host watchpoint.
    db = cheriot_top.state()->db_factory()->Allocate<uint32_t>(2);
    auto status = cheriot_top.tagged_watcher()->SetStoreWatchCallback(
        TaggedMemoryWatcher::AddressRange{
            tohost_addr, tohost_addr + 2 * sizeof(uint32_t) - 1},
        [tagged_memory, tohost_addr, &db, &cheriot_top](uint64_t, int) {
          if (db == nullptr) return;
          tagged_memory->Load(tohost_addr, db, nullptr, nullptr);
          uint32_t code = db->Get<uint32_t>(0);
          if (code & 0x1) {
            code >>= 1;
            std::cerr << absl::StrCat("Simulation halted: exit ",
                                      absl::Hex(code), "\n");
            (void)cheriot_top.Halt();
            db->DecRef();
            db = nullptr;
          }
        });
    if (!status.ok()) {
      LOG(ERROR) << "Failed to set 'tohost' watchpoint";
      exit(-1);
    }
  }

  // Initialize minstret/minstreth. Bind the instruction counter to those
  // registers.
  auto minstret_res = cheriot_top.state()->csr_set()->GetCsr("minstret");
  auto minstreth_res = cheriot_top.state()->csr_set()->GetCsr("minstreth");
  if (!minstret_res.ok() || !minstreth_res.ok()) {
    std::cerr << "Error while initializing minstret/minstreth";
    return -1;
  }
  auto *minstret = static_cast<RiscVCheriotMInstret *>(minstret_res.value());
  auto *minstreth = static_cast<RiscVCheriotMInstreth *>(minstreth_res.value());
  minstret->set_counter(cheriot_top.counter_num_instructions());
  minstreth->set_counter(cheriot_top.counter_num_instructions());

  // Set up the memory router with the appropriate targets.
  ::mpact::sim::util::AtomicMemory *atomic_memory = nullptr;
  atomic_memory = new mpact::sim::util::AtomicMemory(tagged_memory);

  auto *uart = new SimpleUart(cheriot_top.state());

  CHECK_OK(
      router->AddTarget<MemoryInterface>(uart, 0x1000'0000ULL, 0x1000'00ffULL));
  auto *clint = new RiscVClint(/*period=*/100, cheriot_top.state()->mip());
  cheriot_top.counter_num_cycles()->AddListener(clint);
  CHECK_OK(router->AddTarget<AtomicMemoryOpInterface>(
      atomic_memory, 0x0000'0000ULL, 0x01ff'ffffULL));
  CHECK_OK(router->AddTarget<TaggedMemoryInterface>(
      tagged_memory, 0x0000'0000ULL, 0x01ff'ffffULL));
  CHECK_OK(router->AddTarget<MemoryInterface>(clint, 0x0200'0000ULL,
                                              0x0200'ffffULL));
  CHECK_OK(router->AddTarget<AtomicMemoryOpInterface>(
      atomic_memory, 0x02001'0000ULL, 0xffff'ffffULL));
  CHECK_OK(router->AddTarget<TaggedMemoryInterface>(
      tagged_memory, 0x0201'0000ULL, 0xffff'ffffULL));

  // Set up a dummy WFI handler.
  cheriot_top.state()->set_on_wfi([](const Instruction *) { return true; });
  cheriot_top.state()->set_on_ecall([](const Instruction *) { return false; });
  // Initialize the PC to the entry point.
  uint32_t entry_point = load_result.value();
  auto pcc_write = cheriot_top.WriteRegister("pcc", entry_point);
  if (!pcc_write.ok()) {
    std::cerr << "Error writing to pcc: " << pcc_write.message();
    return -1;
  }

  // Set up semihosting.
  auto *semihost = new RiscVArmSemihost(RiscVArmSemihost::BitWidth::kWord32,
                                        cheriot_top.inst_memory(),
                                        cheriot_top.data_memory());
  cheriot_top.state()->AddEbreakHandler([semihost](const Instruction *inst) {
    if (semihost->IsSemihostingCall(inst)) {
      semihost->OnEBreak(inst);
      return true;
    }
    return false;
  });
  semihost->set_exit_callback([&cheriot_top]() {
    cheriot_top.RequestHalt(HaltReason::kSemihostHaltRequest, nullptr);
  });

  // Initializing the stack pointer.

  // First see if there is a stack location defined, if not, do not initialize
  // the stack pointer.
  bool initialize_stack = false;
  uint64_t stack_end = 0;

  // Is the __stack_end symbol defined?
  auto res = elf_loader.GetSymbol(kStackEndSymbolName);
  if (res.ok()) {
    stack_end = res.value().first;
    initialize_stack = true;
  }

  // The stack_end flag overrides the __stack_end symbol.
  if (absl::GetFlag(FLAGS_stack_end).has_value()) {
    stack_end = absl::GetFlag(FLAGS_stack_end).value();
    initialize_stack = true;
  }

  // If there is a stack location, get the stack size, and write the sp.
  if (initialize_stack) {
    // Default size is 32KB.
    uint64_t stack_size = 32 * 1024;

    // Does the executable have a valid GNU_STACK segment? If so, override the
    // default
    auto loader_res = elf_loader.GetStackSize();
    if (loader_res.ok()) {
      stack_end = loader_res.value();
    }

    // If the __stack_size symbol is defined then override.
    auto res = elf_loader.GetSymbol(kStackSizeSymbolName);
    if (res.ok()) {
      stack_size = res.value().first;
    }

    // If the flag is set, then override.
    if (absl::GetFlag(FLAGS_stack_size).has_value()) {
      stack_size = absl::GetFlag(FLAGS_stack_size).value();
    }

    auto sp_write = cheriot_top.WriteRegister("sp", stack_end + stack_size);
    if (!sp_write.ok()) {
      std::cerr << "Error writing to sp: " << sp_write.message();
      return -1;
    }
  }

  mpact::sim::generic::SimpleCounter<double> counter_sec("simulation_time_sec",
                                                         0.0);

  CHECK_OK(cheriot_top.AddCounter(&counter_sec));
  // Set up control-c handling.
  top = &cheriot_top;
  struct sigaction sa;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGINT);
  sa.sa_handler = &sim_sigint_handler;
  sigaction(SIGINT, &sa, nullptr);

  // If exit on exception is set, set up an exception handler that terminates
  // the simulation and prints exception information. In interactive mode a
  // message is printed and any run is stopped.
  if (absl::GetFlag(FLAGS_exit_on_exception)) {
    cheriot_top.state()->set_on_trap(&HandleSimulatorTrap);
  }

  if (memory_use_profiler) memory_use_profiler->set_is_enabled(true);

  // Determine if this is being run interactively or as a batch job.
  bool interactive = absl::GetFlag(FLAGS_i) || absl::GetFlag(FLAGS_interactive);
  CheriotInstrumentationControl *cheriot_instrumentation_control = nullptr;
  if (interactive) {
    mpact::sim::cheriot::DebugCommandShell cmd_shell;
    cmd_shell.AddCore({&cheriot_top, &elf_loader});
    cheriot_instrumentation_control = new CheriotInstrumentationControl(
        &cmd_shell, &cheriot_top, memory_use_profiler);
    // Add custom command to interactive debug command shell.
    cmd_shell.AddCommand(
        "    reg info                       - print all scalar regs",
        PrintRegisters);
    cmd_shell.AddCommand(
        cheriot_instrumentation_control->Usage(),
        absl::bind_front(&CheriotInstrumentationControl::PerformShellCommand,
                         cheriot_instrumentation_control));
    cmd_shell.Run(std::cin, std::cout);
  } else {
    std::cerr << "Starting simulation\n";

    auto t0 = absl::Now();

    auto run_status = cheriot_top.Run();
    if (!run_status.ok()) {
      std::cerr << run_status.message() << std::endl;
    }

    auto wait_status = cheriot_top.Wait();
    if (!wait_status.ok()) {
      std::cerr << wait_status.message() << std::endl;
    }

    auto t1 = absl::Now();
    absl::Duration duration = t1 - t0;
    double sec = static_cast<double>(duration / absl::Milliseconds(100)) / 10;
    counter_sec.SetValue(sec);

    std::cerr << absl::StrFormat("Simulation done: %0.1f sec\n", sec);
  }

  // Write out memory use profile.
  if (memory_use_profiler != nullptr) {
    std::cerr << "Writing out memory use profile\n";
    std::string memory_use_profile_file_name;
    if (FLAGS_output_dir.CurrentValue().empty()) {
      memory_use_profile_file_name =
          "./" + file_basename + "_memory_use_profile.csv";
    } else {
      memory_use_profile_file_name = FLAGS_output_dir.CurrentValue() + "/" +
                                     file_basename + "_memory_use_profile.csv";
    }
    std::fstream memory_use_profile_file(memory_use_profile_file_name.c_str(),
                                         std::ios_base::out);
    if (!memory_use_profile_file.good()) {
      LOG(ERROR) << "Failed to write memory use profile to file";
    } else {
      memory_use_profiler->WriteProfile(memory_use_profile_file);
    }
  }

  // Write out instruction profile.
  if (inst_profiler != nullptr) {
    std::cerr << "Writing out instruction profile\n";
    std::string inst_profile_file_name;
    if (FLAGS_output_dir.CurrentValue().empty()) {
      inst_profile_file_name = "./" + file_basename + "_inst_profile.csv";
    } else {
      inst_profile_file_name = FLAGS_output_dir.CurrentValue() + "/" +
                               file_basename + "_inst_profile.csv";
    }
    std::fstream inst_profile_file(inst_profile_file_name.c_str(),
                                   std::ios_base::out);
    if (!inst_profile_file.good()) {
      LOG(ERROR) << "Failed to write profile to file";
    } else {
      inst_profiler->WriteProfile(inst_profile_file);
    }
  }

  // Export counters.
  std::cerr << "Exporting counters\n";
  auto component_proto = std::make_unique<ComponentData>();
  CHECK_OK(cheriot_top.Export(component_proto.get()))
      << "Failed to export proto";
  std::string proto_file_name;
  if (FLAGS_output_dir.CurrentValue().empty()) {
    proto_file_name = "./" + file_basename + "_counters.proto";
  } else {
    proto_file_name = FLAGS_output_dir.CurrentValue() + "/" + file_basename +
                      "_counters.proto";
  }
  std::fstream proto_file(proto_file_name.c_str(), std::ios_base::out);
  std::string serialized;
  if (!proto_file.good() || !google::protobuf::TextFormat::PrintToString(
                                *component_proto.get(), &serialized)) {
    LOG(ERROR) << "Failed to write proto to file";
  } else {
    proto_file << serialized;
    proto_file.close();
  }

  // Cleanup.
  auto bp_status = cheriot_top.ClearAllSwBreakpoints();
  if (!bp_status.ok()) {
    LOG(ERROR) << "Error in ClearAllSwBreakpoints: " << bp_status.message();
  }
  delete cheriot_instrumentation_control;
  delete inst_profiler;
  delete atomic_memory;
  delete tagged_memory;
  delete memory_use_profiler;
  delete semihost;
  if (db != nullptr) db->DecRef();
}
