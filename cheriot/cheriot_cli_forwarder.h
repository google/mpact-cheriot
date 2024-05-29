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

#ifndef MPACT_CHERIOT__CHERIOT_CLI_FORWARDER_H_
#define MPACT_CHERIOT__CHERIOT_CLI_FORWARDER_H_

#include <cstddef>
#include <cstdint>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cheriot/cheriot_renode_cli_top.h"
#include "mpact/sim/generic/core_debug_interface.h"
#include "mpact/sim/util/renode/cli_forwarder.h"

// This file defines a class that forwards calls from the CLI to the class that
// merges requests from the CLI and ReNode.

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::AccessType;
using ::mpact::sim::util::renode::CLIForwarder;

class CheriotCLIForwarder : public CLIForwarder {
 public:
  explicit CheriotCLIForwarder(CheriotRenodeCLITop *top);
  CheriotCLIForwarder() = delete;
  CheriotCLIForwarder(const CLIForwarder &) = delete;
  CheriotCLIForwarder &operator=(const CLIForwarder &) = delete;

  absl::StatusOr<size_t> ReadTagMemory(uint64_t address, void *buf,
                                       size_t length);
  // Set a data watchpoint for the given memory range. Any access matching the
  // given access type (load/store) will halt execution following the completion
  // of that access.
  absl::Status SetDataWatchpoint(uint64_t address, size_t length,
                                 AccessType access_type);
  // Clear data watchpoint for the given memory address and access type.
  absl::Status ClearDataWatchpoint(uint64_t address, AccessType access_type);

  // Set an action point at the given address to execute the specified action.
  absl::StatusOr<int> SetActionPoint(
      uint64_t address, absl::AnyInvocable<void(uint64_t, int)> action);
  // Clear action point id at the given address.
  absl::Status ClearActionPoint(uint64_t address, int id);
  // Enable/disable action id at the given address.
  absl::Status EnableAction(uint64_t address, int id);
  absl::Status DisableAction(uint64_t address, int id);
  // Enable breaking on control flow change.
  void SetBreakOnControlFlowChange(bool value);

 private:
  CheriotRenodeCLITop *cheriot_cli_top_;
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__CHERIOT_CLI_FORWARDER_H_
