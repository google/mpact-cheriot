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

#include "cheriot/riscv_cheriot_m_instructions.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "absl/random/random.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/immediate_operand.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/type_helpers.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"

// This file contains tests for individual CHERIoT RiscV32M instructions.

namespace {

using ::mpact::sim::generic::operator*;  // NOLINT: used below (clang error).
using ::mpact::sim::cheriot::CheriotRegister;
using ::mpact::sim::cheriot::CheriotState;
using ::mpact::sim::generic::ImmediateOperand;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::util::TaggedFlatDemandMemory;
using CH_EC = ::mpact::sim::cheriot::ExceptionCode;
using PB = CheriotRegister::PermissionBits;

using ::mpact::sim::cheriot::MDiv;
using ::mpact::sim::cheriot::MDivu;
using ::mpact::sim::cheriot::MMul;
using ::mpact::sim::cheriot::MMulh;
using ::mpact::sim::cheriot::MMulhsu;
using ::mpact::sim::cheriot::MMulhu;
using ::mpact::sim::cheriot::MRem;
using ::mpact::sim::cheriot::MRemu;

constexpr char kC1[] = "c1";
constexpr char kC2[] = "c2";
constexpr char kC3[] = "c3";

constexpr int kNumTests = 100;

constexpr uint32_t kInstAddress = 0x2468;

// The test fixture allocates a machine state object and an instruction object.
// It also contains convenience methods for interacting with the instruction
// object in a more short hand form.
class RVCheriotMInstructionTest : public testing::Test {
 public:
  RVCheriotMInstructionTest() {
    mem_ = new TaggedFlatDemandMemory(8);
    state_ = new CheriotState("test", mem_, nullptr);
    instruction_ = new Instruction(kInstAddress, state_);
    instruction_->set_size(4);
    creg_1_ = state_->GetRegister<CheriotRegister>(kC1).first;
    creg_2_ = state_->GetRegister<CheriotRegister>(kC2).first;
    creg_3_ = state_->GetRegister<CheriotRegister>(kC3).first;
  }

  ~RVCheriotMInstructionTest() override {
    delete mem_;
    delete state_;
    delete instruction_;
  }

  // Appends the source and destination operands for the register names
  // given in the two vectors.
  void AppendRegisterOperands(Instruction *inst,
                              const std::vector<std::string> &sources,
                              const std::vector<std::string> &destinations) {
    for (auto &reg_name : sources) {
      auto *reg = state_->GetRegister<CheriotRegister>(reg_name).first;
      inst->AppendSource(reg->CreateSourceOperand());
    }
    for (auto &reg_name : destinations) {
      auto *reg = state_->GetRegister<CheriotRegister>(reg_name).first;
      inst->AppendDestination(reg->CreateDestinationOperand(0));
    }
  }

  void AppendRegisterOperands(const std::vector<std::string> &sources,
                              const std::vector<std::string> &destinations) {
    AppendRegisterOperands(instruction_, sources, destinations);
  }

  // Appends immediate source operands with the given values.
  template <typename T>
  void AppendImmediateOperands(const std::vector<T> &values) {
    for (auto value : values) {
      auto *src = new ImmediateOperand<T>(value);
      instruction_->AppendSource(src);
    }
  }

  // Takes a vector of tuples of register names and values. Fetches each
  // named register and sets it to the corresponding value.
  template <typename T>
  void SetRegisterValues(const std::vector<std::tuple<std::string, T>> values) {
    for (auto &[reg_name, value] : values) {
      auto *reg = state_->GetRegister<CheriotRegister>(reg_name).first;
      reg->set_address(value);
    }
  }

  // Initializes the semantic function of the instruction object.
  void SetSemanticFunction(Instruction::SemanticFunction fcn) {
    instruction_->set_semantic_function(fcn);
  }

  // Returns the value of the named register.
  template <typename T>
  T GetRegisterValue(std::string_view reg_name) {
    auto *reg = state_->GetRegister<CheriotRegister>(reg_name).first;
    return reg->address();
  }

  TaggedFlatDemandMemory *mem_;
  CheriotState *state_;
  Instruction *instruction_;
  CheriotRegister *creg_1_;
  CheriotRegister *creg_2_;
  CheriotRegister *creg_3_;
  absl::BitGen bitgen_;
};

TEST_F(RVCheriotMInstructionTest, MMul) {
  instruction_->set_semantic_function(&MMul);
  AppendRegisterOperands({kC1, kC2}, {kC3});
  for (int i = 0; i < kNumTests; i++) {
    int32_t a = absl::Uniform(absl::IntervalClosed, bitgen_,
                              std::numeric_limits<int32_t>::min(),
                              std::numeric_limits<int32_t>::max());
    int32_t b = absl::Uniform(absl::IntervalClosed, bitgen_,
                              std::numeric_limits<int32_t>::min(),
                              std::numeric_limits<int32_t>::max());
    SetRegisterValues<int32_t>({{kC1, a}, {kC2, b}});
    creg_3_->ResetMemoryRoot();
    instruction_->Execute(nullptr);
    int32_t result =
        static_cast<int32_t>(static_cast<int64_t>(a) * static_cast<int64_t>(b));
    EXPECT_EQ(creg_3_->address(), result);
    EXPECT_FALSE(creg_3_->tag());
    EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->permissions(), 0);
    EXPECT_EQ(creg_3_->object_type(), 0);
  }
}

TEST_F(RVCheriotMInstructionTest, MMulh) {
  instruction_->set_semantic_function(&MMulh);
  AppendRegisterOperands({kC1, kC2}, {kC3});
  for (int i = 0; i < kNumTests; i++) {
    int32_t a = absl::Uniform(absl::IntervalClosed, bitgen_,
                              std::numeric_limits<int32_t>::min(),
                              std::numeric_limits<int32_t>::max());
    int32_t b = absl::Uniform(absl::IntervalClosed, bitgen_,
                              std::numeric_limits<int32_t>::min(),
                              std::numeric_limits<int32_t>::max());
    SetRegisterValues<int32_t>({{kC1, a}, {kC2, b}});
    creg_3_->ResetMemoryRoot();
    instruction_->Execute(nullptr);
    int32_t result = static_cast<int32_t>(
        (static_cast<int64_t>(a) * static_cast<int64_t>(b)) >> 32);
    EXPECT_EQ(creg_3_->address(), result);
    EXPECT_FALSE(creg_3_->tag());
    EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->permissions(), 0);
    EXPECT_EQ(creg_3_->object_type(), 0);
  }
}

TEST_F(RVCheriotMInstructionTest, MMulhu) {
  instruction_->set_semantic_function(&MMulhu);
  AppendRegisterOperands({kC1, kC2}, {kC3});
  for (int i = 0; i < kNumTests; i++) {
    uint32_t a = absl::Uniform(absl::IntervalClosed, bitgen_,
                               std::numeric_limits<uint32_t>::min(),
                               std::numeric_limits<uint32_t>::max());
    uint32_t b = absl::Uniform(absl::IntervalClosed, bitgen_,
                               std::numeric_limits<uint32_t>::min(),
                               std::numeric_limits<uint32_t>::max());
    SetRegisterValues<int32_t>({{kC1, a}, {kC2, b}});
    creg_3_->ResetMemoryRoot();
    instruction_->Execute(nullptr);
    uint32_t result = static_cast<uint32_t>(
        (static_cast<uint64_t>(a) * static_cast<uint64_t>(b)) >> 32);
    EXPECT_EQ(creg_3_->address(), result);
    EXPECT_FALSE(creg_3_->tag());
    EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->permissions(), 0);
    EXPECT_EQ(creg_3_->object_type(), 0);
  }
}

TEST_F(RVCheriotMInstructionTest, MMulhsu) {
  instruction_->set_semantic_function(&MMulhsu);
  AppendRegisterOperands({kC1, kC2}, {kC3});
  for (int i = 0; i < kNumTests; i++) {
    int32_t a = absl::Uniform(absl::IntervalClosed, bitgen_,
                              std::numeric_limits<int32_t>::min(),
                              std::numeric_limits<int32_t>::max());
    uint32_t b = absl::Uniform(absl::IntervalClosed, bitgen_,
                               std::numeric_limits<uint32_t>::min(),
                               std::numeric_limits<uint32_t>::max());
    SetRegisterValues<int32_t>({{kC1, a}, {kC2, b}});
    creg_3_->ResetMemoryRoot();
    instruction_->Execute(nullptr);
    int32_t result = static_cast<int32_t>(
        (static_cast<int64_t>(a) * static_cast<uint64_t>(b)) >> 32);
    EXPECT_EQ(creg_3_->address(), result);
    EXPECT_FALSE(creg_3_->tag());
    EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->permissions(), 0);
    EXPECT_EQ(creg_3_->object_type(), 0);
  }
}

TEST_F(RVCheriotMInstructionTest, MDiv) {
  instruction_->set_semantic_function(&MDiv);
  AppendRegisterOperands({kC1, kC2}, {kC3});
  for (int i = 0; i < kNumTests; i++) {
    int32_t a = absl::Uniform(absl::IntervalClosed, bitgen_,
                              std::numeric_limits<int32_t>::min(),
                              std::numeric_limits<int32_t>::max());
    int32_t b = absl::Uniform(absl::IntervalClosed, bitgen_,
                              std::numeric_limits<int32_t>::min(),
                              std::numeric_limits<int32_t>::max());
    SetRegisterValues<int32_t>({{kC1, a}, {kC2, b}});
    creg_3_->ResetMemoryRoot();
    instruction_->Execute(nullptr);

    int32_t result;
    if (b == 0) {
      result = -1;
    } else if ((b == -1) && (a == std::numeric_limits<int32_t>::min())) {
      result = std::numeric_limits<int32_t>::min();
    } else {
      result = a / b;
    }
    EXPECT_EQ(creg_3_->address(), result);
    EXPECT_FALSE(creg_3_->tag());
    EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->permissions(), 0);
    EXPECT_EQ(creg_3_->object_type(), 0);
  }
}

TEST_F(RVCheriotMInstructionTest, MDivu) {
  instruction_->set_semantic_function(&MDivu);
  AppendRegisterOperands({kC1, kC2}, {kC3});
  for (int i = 0; i < kNumTests; i++) {
    uint32_t a = absl::Uniform(absl::IntervalClosed, bitgen_,
                               std::numeric_limits<uint32_t>::min(),
                               std::numeric_limits<uint32_t>::max());
    uint32_t b = absl::Uniform(absl::IntervalClosed, bitgen_,
                               std::numeric_limits<uint32_t>::min(),
                               std::numeric_limits<uint32_t>::max());
    SetRegisterValues<int32_t>({{kC1, a}, {kC2, b}});
    creg_3_->ResetMemoryRoot();
    instruction_->Execute(nullptr);

    uint32_t result;
    if (b == 0) {
      result = std::numeric_limits<uint32_t>::max();
    } else {
      result = a / b;
    }
    EXPECT_EQ(creg_3_->address(), result);
    EXPECT_FALSE(creg_3_->tag());
    EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->permissions(), 0);
    EXPECT_EQ(creg_3_->object_type(), 0);
  }
}

TEST_F(RVCheriotMInstructionTest, MRem) {
  instruction_->set_semantic_function(&MRem);
  AppendRegisterOperands({kC1, kC2}, {kC3});
  for (int i = 0; i < kNumTests; i++) {
    int32_t a = absl::Uniform(absl::IntervalClosed, bitgen_,
                              std::numeric_limits<int32_t>::min(),
                              std::numeric_limits<int32_t>::max());
    int32_t b = absl::Uniform(absl::IntervalClosed, bitgen_,
                              std::numeric_limits<int32_t>::min(),
                              std::numeric_limits<int32_t>::max());
    SetRegisterValues<int32_t>({{kC1, a}, {kC2, b}});
    creg_3_->ResetMemoryRoot();
    instruction_->Execute(nullptr);

    int32_t result;
    if (b == 0) {
      result = a;
    } else if ((b == -1) && (a == std::numeric_limits<int32_t>::min())) {
      result = 0;
    } else {
      result = a % b;
    }
    EXPECT_EQ(creg_3_->address(), result);
    EXPECT_FALSE(creg_3_->tag());
    EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->permissions(), 0);
    EXPECT_EQ(creg_3_->object_type(), 0);
  }
}

TEST_F(RVCheriotMInstructionTest, MRemu) {
  instruction_->set_semantic_function(&MRemu);
  AppendRegisterOperands({kC1, kC2}, {kC3});
  for (int i = 0; i < kNumTests; i++) {
    uint32_t a = absl::Uniform(absl::IntervalClosed, bitgen_,
                               std::numeric_limits<uint32_t>::min(),
                               std::numeric_limits<uint32_t>::max());
    uint32_t b = absl::Uniform(absl::IntervalClosed, bitgen_,
                               std::numeric_limits<uint32_t>::min(),
                               std::numeric_limits<uint32_t>::max());
    SetRegisterValues<int32_t>({{kC1, a}, {kC2, b}});
    creg_3_->ResetMemoryRoot();
    instruction_->Execute(nullptr);

    uint32_t result;
    if (b == 0) {
      result = a;
    } else {
      result = a % b;
    }
    EXPECT_EQ(creg_3_->address(), result);
    EXPECT_FALSE(creg_3_->tag());
    EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
    EXPECT_EQ(creg_3_->permissions(), 0);
    EXPECT_EQ(creg_3_->object_type(), 0);
  }
}

}  // namespace
