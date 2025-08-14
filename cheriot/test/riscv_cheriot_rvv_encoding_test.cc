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

#include "cheriot/riscv_cheriot_rvv_encoding.h"

#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_rvv_enums.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/type_helpers.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"

namespace {

using ::mpact::sim::cheriot::CheriotState;
using ::mpact::sim::cheriot::isa32_rvv::RiscVCheriotRVVEncoding;
using ::mpact::sim::generic::operator*;  // NOLINT: is used below (clang error).
using ::mpact::sim::cheriot::isa32_rvv::DestOpEnum;
using ::mpact::sim::cheriot::isa32_rvv::kDestOpNames;
using ::mpact::sim::cheriot::isa32_rvv::kSourceOpNames;
using ::mpact::sim::cheriot::isa32_rvv::SourceOpEnum;
using ::mpact::sim::util::TaggedFlatDemandMemory;
using SlotEnum = mpact::sim::cheriot::isa32_rvv::SlotEnum;
using OpcodeEnum = mpact::sim::cheriot::isa32_rvv::OpcodeEnum;

class RiscVCheriotRVVEncodingTest : public testing::Test {
 protected:
  RiscVCheriotRVVEncodingTest() {
    mem_ = new TaggedFlatDemandMemory(8);
    state_ = new CheriotState("test", mem_, nullptr);
    enc_ = new RiscVCheriotRVVEncoding(state_);
  }
  ~RiscVCheriotRVVEncodingTest() override {
    delete enc_;
    delete mem_;
    delete state_;
  }

  TaggedFlatDemandMemory* mem_;
  CheriotState* state_;
  RiscVCheriotRVVEncoding* enc_;
};

TEST_F(RiscVCheriotRVVEncodingTest, SourceOperands) {
  auto& getters = enc_->source_op_getters();
  for (int i = *SourceOpEnum::kNone; i < *SourceOpEnum::kPastMaxValue; ++i) {
    EXPECT_TRUE(getters.contains(i)) << "No source operand for enum value " << i
                                     << " (" << kSourceOpNames[i] << ")";
  }
}

TEST_F(RiscVCheriotRVVEncodingTest, DestOperands) {
  auto& getters = enc_->dest_op_getters();
  for (int i = *DestOpEnum::kNone; i < *DestOpEnum::kPastMaxValue; ++i) {
    EXPECT_TRUE(getters.contains(i)) << "No dest operand for enum value " << i
                                     << " (" << kDestOpNames[i] << ")";
  }
}

}  // namespace
