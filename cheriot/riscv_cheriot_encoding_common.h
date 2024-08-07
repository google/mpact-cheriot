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

#ifndef MPACT_CHERIOT_RISCV_CHERIOT_ENCODING_COMMON_H_
#define MPACT_CHERIOT_RISCV_CHERIOT_ENCODING_COMMON_H_

#include <cstdint>

namespace mpact {
namespace sim {
namespace cheriot {

class CheriotState;

// This class provides a common interface for accessing the state and
// instruction word for the RiscVCheriotEncoding classes (scalar, vector, vector
// + fp).

class RiscVCheriotEncodingCommon {
 public:
  explicit RiscVCheriotEncodingCommon(CheriotState *state) : state_(state) {}

  // Accessors.
  CheriotState *state() const { return state_; }
  uint32_t inst_word() const { return inst_word_; }

 protected:
  CheriotState *state_;
  uint32_t inst_word_;
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT_RISCV_CHERIOT_ENCODING_COMMON_H_
