// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cheriot/cheriot_vector_true_operand.h"

#include <cstdint>
#include <limits>

#include "cheriot/cheriot_state.h"
#include "riscv//riscv_register.h"

namespace mpact {
namespace sim {
namespace cheriot {

CheriotVectorTrueOperand::CheriotVectorTrueOperand(CheriotState* state)
    : RV32VectorSourceOperand(
          state->GetRegister<RVVectorRegister>(kName).first) {
  // Ensure the value is all ones.
  auto* reg = state->GetRegister<RVVectorRegister>(kName).first;
  auto data = reg->data_buffer()->Get<uint64_t>();
  for (int i = 0; i < data.size(); i++) {
    data[i] = std::numeric_limits<uint64_t>::max();
  }
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
