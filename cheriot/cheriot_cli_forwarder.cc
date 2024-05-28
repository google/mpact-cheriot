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
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cheriot/cheriot_renode_cli_top.h"
#include "mpact/sim/generic/core_debug_interface.h"
#include "third_party/mpact_renode/cli_forwarder.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::AccessType;
using ::mpact::sim::renode::CLIForwarder;
using RunStatus = ::mpact::sim::generic::CoreDebugInterface::RunStatus;
using HaltReasonValueType =
    ::mpact::sim::generic::CoreDebugInterface::HaltReasonValueType;

CheriotCLIForwarder::CheriotCLIForwarder(CheriotRenodeCLITop *cheriot_cli_top)
    : CLIForwarder(cheriot_cli_top), cheriot_cli_top_(cheriot_cli_top) {}

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

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
