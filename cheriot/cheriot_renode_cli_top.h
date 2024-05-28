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

#ifndef MPACT_CHERIOT__CHERIOT_RENODE_CLI_TOP_H_
#define MPACT_CHERIOT__CHERIOT_RENODE_CLI_TOP_H_

#include <cstddef>
#include <cstdint>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cheriot/cheriot_debug_interface.h"
#include "cheriot/cheriot_top.h"
#include "third_party/mpact_renode/renode_cli_top.h"

namespace mpact {
namespace sim {
namespace cheriot {

// This class extends the RenodeCLITop with a few features specific to the
// CherIoT CLI.
class CheriotRenodeCLITop : public renode::RenodeCLITop {
 public:
  CheriotRenodeCLITop(CheriotTop *cheriot_top, bool wait_for_cli);

  absl::StatusOr<size_t> CLIReadTagMemory(uint64_t address, void *buf,
                                          size_t length);

  absl::Status CLISetDataWatchpoint(uint64_t address, size_t length,
                                    AccessType access_type);
  absl::Status CLIClearDataWatchpoint(uint64_t address, AccessType access_type);
  void CLISetBreakOnControlFlowChange(bool value);

  absl::StatusOr<int> CLISetActionPoint(
      uint64_t address, absl::AnyInvocable<void(uint64_t, int)> action);
  absl::Status CLIClearActionPoint(uint64_t address, int id);
  absl::Status CLIEnableAction(uint64_t address, int id);
  absl::Status CLIDisableAction(uint64_t address, int id);

 private:
  CheriotTop *cheriot_top_ = nullptr;
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__CHERIOT_RENODE_CLI_TOP_H_
