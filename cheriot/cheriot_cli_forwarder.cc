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

#include "cheriot/cheriot_cli_forwarder.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cheriot/cheriot_renode_cli_top.h"
#include "cheriot/cheriot_state.h"
#include "mpact/sim/generic/core_debug_interface.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::AccessType;
using RunStatus = ::mpact::sim::generic::CoreDebugInterface::RunStatus;
using HaltReasonValueType =
    ::mpact::sim::generic::CoreDebugInterface::HaltReasonValueType;

CheriotCLIForwarder::CheriotCLIForwarder(CheriotRenodeCLITop *cheriot_cli_top)
    : cheriot_cli_top_(cheriot_cli_top) {}

// Forward the calls to the CheriotRenodeCLITop class - CLI methods.

absl::StatusOr<size_t> CheriotCLIForwarder::ReadTagMemory(uint64_t address,
                                                          void *buf,
                                                          size_t length) {
  return cheriot_cli_top_->CLIReadTagMemory(address, buf, length);
}

absl::Status CheriotCLIForwarder::SetDataWatchpoint(uint64_t address,
                                                    size_t length,
                                                    AccessType access_type) {
  return cheriot_cli_top_->CLISetDataWatchpoint(address, length, access_type);
}

absl::Status CheriotCLIForwarder::ClearDataWatchpoint(uint64_t address,
                                                      AccessType access_type) {
  return cheriot_cli_top_->CLIClearDataWatchpoint(address, access_type);
}

absl::StatusOr<int> CheriotCLIForwarder::SetActionPoint(
    uint64_t address, absl::AnyInvocable<void(uint64_t, int)> action) {
  return cheriot_cli_top_->CLISetActionPoint(address, std::move(action));
}

absl::Status CheriotCLIForwarder::ClearActionPoint(uint64_t address, int id) {
  return cheriot_cli_top_->CLIClearActionPoint(address, id);
}

absl::Status CheriotCLIForwarder::EnableAction(uint64_t address, int id) {
  return cheriot_cli_top_->CLIEnableAction(address, id);
}

absl::Status CheriotCLIForwarder::DisableAction(uint64_t address, int id) {
  return cheriot_cli_top_->CLIDisableAction(address, id);
}

void CheriotCLIForwarder::SetBreakOnControlFlowChange(bool value) {
  cheriot_cli_top_->CLISetBreakOnControlFlowChange(value);
}

bool CheriotCLIForwarder::BreakOnControlFlowChange() {
  return cheriot_cli_top_->CLIBreakOnControlFlowChange();
}

absl::Status CheriotCLIForwarder::Halt() { return cheriot_cli_top_->CLIHalt(); }

absl::Status CheriotCLIForwarder::Halt(HaltReason halt_reason) {
  cheriot_cli_top_->CLIRequestHalt(halt_reason, nullptr);
  return absl::OkStatus();
}

absl::Status CheriotCLIForwarder::Halt(HaltReasonValueType halt_reason) {
  cheriot_cli_top_->CLIRequestHalt(halt_reason, nullptr);
  return absl::OkStatus();
}
absl::StatusOr<int> CheriotCLIForwarder::Step(int num) {
  return cheriot_cli_top_->CLIStep(num);
}

absl::Status CheriotCLIForwarder::Run() { return cheriot_cli_top_->CLIRun(); }

absl::Status CheriotCLIForwarder::Wait() { return cheriot_cli_top_->CLIWait(); }

// Returns the current run status.
absl::StatusOr<RunStatus> CheriotCLIForwarder::GetRunStatus() {
  return cheriot_cli_top_->CLIGetRunStatus();
}

// Returns the reason for the most recent halt.
absl::StatusOr<HaltReasonValueType> CheriotCLIForwarder::GetLastHaltReason() {
  return cheriot_cli_top_->CLIGetLastHaltReason();
}

// Read/write the named registers.
absl::StatusOr<uint64_t> CheriotCLIForwarder::ReadRegister(
    const std::string &name) {
  return cheriot_cli_top_->CLIReadRegister(name);
}

absl::Status CheriotCLIForwarder::WriteRegister(const std::string &name,
                                                uint64_t value) {
  return cheriot_cli_top_->CLIWriteRegister(name, value);
}

absl::StatusOr<DataBuffer *> CheriotCLIForwarder::GetRegisterDataBuffer(
    const std::string &name) {
  return cheriot_cli_top_->CLIGetRegisterDataBuffer(name);
}

// Read/write the buffers to memory.
absl::StatusOr<size_t> CheriotCLIForwarder::ReadMemory(uint64_t address,
                                                       void *buf,
                                                       size_t length) {
  return cheriot_cli_top_->CLIReadMemory(address, buf, length);
}

absl::StatusOr<size_t> CheriotCLIForwarder::WriteMemory(uint64_t address,
                                                        const void *buf,
                                                        size_t length) {
  return cheriot_cli_top_->CLIWriteMemory(address, buf, length);
}

bool CheriotCLIForwarder::HasBreakpoint(uint64_t address) {
  return cheriot_cli_top_->CLIHasBreakpoint(address);
}

absl::Status CheriotCLIForwarder::SetSwBreakpoint(uint64_t address) {
  return cheriot_cli_top_->CLISetSwBreakpoint(address);
}

absl::Status CheriotCLIForwarder::ClearSwBreakpoint(uint64_t address) {
  return cheriot_cli_top_->CLIClearSwBreakpoint(address);
}

absl::Status CheriotCLIForwarder::ClearAllSwBreakpoints() {
  return cheriot_cli_top_->CLIClearAllSwBreakpoints();
}

absl::StatusOr<Instruction *> CheriotCLIForwarder::GetInstruction(
    uint64_t address) {
  return cheriot_cli_top_->CLIGetInstruction(address);
}

absl::StatusOr<std::string> CheriotCLIForwarder::GetDisassembly(
    uint64_t address) {
  return cheriot_cli_top_->CLIGetDisassembly(address);
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
