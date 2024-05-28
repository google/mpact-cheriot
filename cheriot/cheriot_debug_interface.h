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

#ifndef MPACT_CHERIOT__CHERIOT_DEBUG_INTERFACE_H_
#define MPACT_CHERIOT__CHERIOT_DEBUG_INTERFACE_H_

#include <cstddef>
#include <cstdint>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "mpact/sim/generic/core_debug_interface.h"

namespace mpact::sim::cheriot {

using ::mpact::sim::generic::AccessType;

class CheriotDebugInterface : public generic::CoreDebugInterface {
 public:
  ~CheriotDebugInterface() override = default;

  // Read tags from memory from the byte address (not tag address) given. The
  // tag address is computed by shifting the byte address right by three. The
  // length specifies the number of bytes (tags) to read.
  virtual absl::StatusOr<size_t> ReadTagMemory(uint64_t address, void *buf,
                                               size_t length) = 0;
  // Set a data watchpoint for the given memory range. Any access matching the
  // given access type (load/store) will halt execution following the completion
  // of that access.
  virtual absl::Status SetDataWatchpoint(uint64_t address, size_t length,
                                         AccessType access_type) = 0;
  // Clear data watchpoint for the given memory address and access type.
  virtual absl::Status ClearDataWatchpoint(uint64_t address,
                                           AccessType access_type) = 0;
  // Set an action point at the given address to execute the specified action.
  virtual absl::StatusOr<int> SetActionPoint(
      uint64_t address, absl::AnyInvocable<void(uint64_t, int)> action) = 0;
  // Clear action point id at the given address.
  virtual absl::Status ClearActionPoint(uint64_t address, int id) = 0;
  // Enable/disable action id at the given address.
  virtual absl::Status EnableAction(uint64_t address, int id) = 0;
  virtual absl::Status DisableAction(uint64_t address, int id) = 0;

  // Enable/disable breaking on control flow change.
  virtual void SetBreakOnControlFlowChange(bool enabled) = 0;
};

}  // namespace mpact::sim::cheriot

#endif  // MPACT_CHERIOT__CHERIOT_DEBUG_INTERFACE_H_
