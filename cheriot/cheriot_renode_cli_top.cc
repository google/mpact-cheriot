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

#include "cheriot/cheriot_renode_cli_top.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cheriot/cheriot_debug_interface.h"
#include "cheriot/cheriot_top.h"
#include "third_party/mpact_renode/renode_cli_top.h"

namespace mpact {
namespace sim {
namespace cheriot {

CheriotRenodeCLITop::CheriotRenodeCLITop(CheriotTop *cheriot_top,
                                         bool wait_for_cli)
    : renode::RenodeCLITop(cheriot_top, wait_for_cli),
      cheriot_top_(cheriot_top) {}

absl::StatusOr<size_t> CheriotRenodeCLITop::CLIReadTagMemory(uint64_t address,
                                                             void *buf,
                                                             size_t length) {
  return DoWhenInControl<absl::StatusOr<size_t>>(
      [this, address, buf, length]() {
        return cheriot_top_->ReadTagMemory(address, buf, length);
      });
}

absl::Status CheriotRenodeCLITop::CLISetDataWatchpoint(uint64_t address,
                                                       size_t length,
                                                       AccessType access_type) {
  return DoWhenInControl<absl::Status>([this, address, length, access_type]() {
    return cheriot_top_->SetDataWatchpoint(address, length, access_type);
  });
}

absl::Status CheriotRenodeCLITop::CLIClearDataWatchpoint(
    uint64_t address, AccessType access_type) {
  return DoWhenInControl<absl::Status>([this, address, access_type]() {
    return cheriot_top_->ClearDataWatchpoint(address, access_type);
  });
}

void CheriotRenodeCLITop::CLISetBreakOnControlFlowChange(bool value) {
  DoWhenInControl<void>(
      [this, value]() { cheriot_top_->SetBreakOnControlFlowChange(value); });
}

absl::StatusOr<int> CheriotRenodeCLITop::CLISetActionPoint(
    uint64_t address, absl::AnyInvocable<void(uint64_t, int)> action) {
  return DoWhenInControl<absl::StatusOr<int>>([this, address, &action]() {
    return cheriot_top_->SetActionPoint(address, std::move(action));
  });
}

absl::Status CheriotRenodeCLITop::CLIClearActionPoint(uint64_t address,
                                                      int id) {
  return DoWhenInControl<absl::Status>([this, address, id]() {
    return cheriot_top_->ClearActionPoint(address, id);
  });
}

absl::Status CheriotRenodeCLITop::CLIEnableAction(uint64_t address, int id) {
  return DoWhenInControl<absl::Status>([this, address, id]() {
    return cheriot_top_->EnableAction(address, id);
  });
}

absl::Status CheriotRenodeCLITop::CLIDisableAction(uint64_t address, int id) {
  return DoWhenInControl<absl::Status>([this, address, id]() {
    return cheriot_top_->DisableAction(address, id);
  });
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
