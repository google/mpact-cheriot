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

#include <cstdint>

#include "cheriot/cheriot_state.h"
#include "cheriot/cheriot_vector_state.h"
#include "cheriot/cheriot_vector_true_operand.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"

namespace {

using ::mpact::sim::cheriot::CheriotState;
using ::mpact::sim::cheriot::CheriotVectorState;
using ::mpact::sim::cheriot::CheriotVectorTrueOperand;
using ::mpact::sim::util::TaggedFlatDemandMemory;

constexpr int kVLengthInBytes = 64;
// Test fixture.
class CheriotVectorTrueTest : public testing::Test {
 protected:
  CheriotVectorTrueTest() : memory_(8) {
    state_ = new CheriotState("test", &memory_);
    vstate_ = new CheriotVectorState(state_, kVLengthInBytes);
  }
  ~CheriotVectorTrueTest() override {
    delete state_;
    delete vstate_;
  }

  TaggedFlatDemandMemory memory_;
  CheriotState *state_;
  CheriotVectorState *vstate_;
};

TEST_F(CheriotVectorTrueTest, Initial) {
  auto *op = new CheriotVectorTrueOperand(state_);
  for (int i = 0; i < op->shape()[0]; ++i) {
    EXPECT_EQ(op->AsUint8(i), 0xff) << "element: " << i;
  }
  delete op;
}

TEST_F(CheriotVectorTrueTest, Register) {
  auto *op = new CheriotVectorTrueOperand(state_);
  auto *reg = op->GetRegister(0);
  auto span = reg->data_buffer()->Get<uint8_t>();
  for (int i = 0; i < op->shape()[0]; ++i) {
    EXPECT_EQ(span[i], 0xff) << "element: " << i;
  }
  delete op;
}

}  // namespace
