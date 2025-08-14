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

#include "cheriot/riscv_cheriot_i_instructions.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "absl/types/span.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/immediate_operand.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/type_helpers.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"

// This file contains tests for individual RiscV32I instructions.

namespace {

using ::mpact::sim::generic::operator*;  // NOLINT: used below (clang error).
using ::mpact::sim::cheriot::CheriotRegister;
using ::mpact::sim::cheriot::CheriotState;
using ::mpact::sim::generic::ImmediateOperand;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::util::TaggedFlatDemandMemory;
using CH_EC = ::mpact::sim::cheriot::ExceptionCode;
using PB = CheriotRegister::PermissionBits;

constexpr char kC1[] = "c1";
constexpr char kC2[] = "c2";
constexpr char kC3[] = "c3";

constexpr int kC1Num = 1;

constexpr uint32_t kInstAddress = 0x2468;
constexpr int32_t kVal1 = 0x1234;
constexpr int32_t kVal2 = -0x5678;
constexpr uint32_t kOffset = 0x248;
constexpr uint32_t kBranchTarget = kInstAddress + kOffset;
constexpr uint32_t kMemAddress = 0x1000;
constexpr uint32_t kMemValue = 0x81'92'a3'b4;
constexpr uint32_t kShift = 6;

// The test fixture allocates a machine state object and an instruction object.
// It also contains convenience methods for interacting with the instruction
// object in a more short hand form.
class RVCheriotIInstructionTest : public testing::Test {
 public:
  RVCheriotIInstructionTest() {
    mem_ = new TaggedFlatDemandMemory(8);
    state_ = new CheriotState("test", mem_, nullptr);
    instruction_ = new Instruction(kInstAddress, state_);
    instruction_->set_size(4);
    creg_1_ = state_->GetRegister<CheriotRegister>(kC1).first;
    creg_2_ = state_->GetRegister<CheriotRegister>(kC2).first;
    creg_3_ = state_->GetRegister<CheriotRegister>(kC3).first;
    state_->set_on_trap([this](bool is_interrupt, uint64_t trap_value,
                               uint64_t exception_code, uint64_t epc,
                               const Instruction* inst) {
      return TrapHandler(is_interrupt, trap_value, exception_code, epc, inst);
    });
  }

  ~RVCheriotIInstructionTest() override {
    delete mem_;
    delete state_;
    delete instruction_;
  }

  // Appends the source and destination operands for the register names
  // given in the two vectors.
  void AppendRegisterOperands(Instruction* inst,
                              absl::Span<const std::string> sources,
                              absl::Span<const std::string> destinations) {
    for (auto& reg_name : sources) {
      auto* reg = state_->GetRegister<CheriotRegister>(reg_name).first;
      inst->AppendSource(reg->CreateSourceOperand());
    }
    for (auto& reg_name : destinations) {
      auto* reg = state_->GetRegister<CheriotRegister>(reg_name).first;
      inst->AppendDestination(reg->CreateDestinationOperand(0));
    }
  }

  void AppendRegisterOperands(const std::vector<std::string>& sources,
                              const std::vector<std::string>& destinations) {
    AppendRegisterOperands(instruction_, sources, destinations);
  }

  // Appends immediate source operands with the given values.
  template <typename T>
  void AppendImmediateOperands(const std::vector<T>& values) {
    for (auto value : values) {
      auto* src = new ImmediateOperand<T>(value);
      instruction_->AppendSource(src);
    }
  }

  // Takes a vector of tuples of register names and values. Fetches each
  // named register and sets it to the corresponding value.
  template <typename T>
  void SetRegisterValues(const std::vector<std::tuple<std::string, T>> values) {
    for (auto& [reg_name, value] : values) {
      auto* reg = state_->GetRegister<CheriotRegister>(reg_name).first;
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
    auto* reg = state_->GetRegister<CheriotRegister>(reg_name).first;
    return reg->address();
  }
  // Gets called on a trap.
  bool TrapHandler(bool is_interrupt, uint64_t trap_value,
                   uint64_t exception_code, uint64_t epc,
                   const Instruction* inst);

  TaggedFlatDemandMemory* mem_;
  CheriotState* state_;
  Instruction* instruction_;
  CheriotRegister* creg_1_;
  CheriotRegister* creg_2_;
  CheriotRegister* creg_3_;
  bool trap_taken_ = false;
  bool trap_is_interrupt_ = false;
  uint64_t trap_value_ = 0;
  uint64_t trap_exception_code_ = 0;
  uint64_t trap_epc_ = 0;
  const Instruction* trap_inst_ = nullptr;
};

bool RVCheriotIInstructionTest::TrapHandler(bool is_interrupt,
                                            uint64_t trap_value,
                                            uint64_t exception_code,
                                            uint64_t epc,
                                            const Instruction* inst) {
  trap_taken_ = true;
  trap_is_interrupt_ = is_interrupt;
  trap_value_ = trap_value;
  trap_exception_code_ = exception_code;
  trap_epc_ = epc;
  trap_inst_ = inst;
  return true;
}
// Almost all the tests below follow the same pattern. There are two phases.
// In the first register and or immediate operands are added to the instruction,
// and the instruction semantic function under test is bound to the instruction.
// In the second phase, the values of register operands are assigned, the
// instruction is executed, and the value(s) of the output register(s) is (are)
// compared against the expected value. The second phase may be repeated for
// different combinations of register operand values.

TEST_F(RVCheriotIInstructionTest, RV32IAdd) {
  AppendRegisterOperands({kC1, kC2}, {kC3});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVIAdd);

  SetRegisterValues<int32_t>({{kC1, kVal1}, {kC2, kVal2}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3), kVal1 + kVal2);
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32ISub) {
  AppendRegisterOperands({kC1, kC2}, {kC3});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISub);

  SetRegisterValues<int32_t>({{kC1, kVal1}, {kC2, kVal2}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3), kVal1 - kVal2);
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32ISlt) {
  AppendRegisterOperands({kC1, kC2}, {kC3});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISlt);

  SetRegisterValues<int32_t>({{kC1, kVal1}, {kC2, kVal2}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3), kVal1 < kVal2);
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), 0);
  EXPECT_EQ(creg_3_->base(), 0);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);

  SetRegisterValues<int32_t>({{kC1, kVal2}, {kC2, kVal1}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3), kVal2 < kVal1);
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), 0);
  EXPECT_EQ(creg_3_->base(), 0);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32ISltu) {
  AppendRegisterOperands({kC1, kC2}, {kC3});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISltu);
  SetRegisterValues<int32_t>({{kC1, kVal1}, {kC2, kVal2}});

  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3),
            static_cast<uint32_t>(kVal1) < static_cast<uint32_t>(kVal2));
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), 0);
  EXPECT_EQ(creg_3_->base(), 0);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);

  SetRegisterValues<int32_t>({{kC1, kVal2}, {kC2, kVal1}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3),
            static_cast<uint32_t>(kVal2) < static_cast<uint32_t>(kVal1));
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), 0);
  EXPECT_EQ(creg_3_->base(), 0);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32IAnd) {
  AppendRegisterOperands({kC1, kC2}, {kC3});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVIAnd);
  SetRegisterValues<int32_t>({{kC1, kVal1}, {kC2, kVal2}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3), kVal1 & kVal2);
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), 0);
  EXPECT_EQ(creg_3_->base(), 0);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32IOr) {
  AppendRegisterOperands({kC1, kC2}, {kC3});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVIOr);

  SetRegisterValues<int32_t>({{kC1, kVal1}, {kC2, kVal2}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3), kVal1 | kVal2);
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32IXor) {
  AppendRegisterOperands({kC1, kC2}, {kC3});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVIXor);
  SetRegisterValues<int32_t>({{kC1, kVal1}, {kC2, kVal2}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3), kVal1 ^ kVal2);
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32ISll) {
  AppendRegisterOperands({kC1}, {kC3});
  AppendImmediateOperands<uint32_t>({kShift});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISll);

  SetRegisterValues<uint32_t>({{kC1, kMemValue}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3), kMemValue << kShift);
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32ISrl) {
  AppendRegisterOperands({kC1}, {kC3});
  AppendImmediateOperands<uint32_t>({kShift});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISrl);

  SetRegisterValues<uint32_t>({{kC1, kMemValue}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3), kMemValue >> kShift);
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32ISra) {
  AppendRegisterOperands({kC1}, {kC3});
  AppendImmediateOperands<uint32_t>({kShift});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISra);
  SetRegisterValues<uint32_t>({{kC1, kMemValue}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3),
            static_cast<int32_t>(kMemValue) >> kShift);
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32ILui) {
  AppendRegisterOperands({}, {kC3});
  AppendImmediateOperands<uint32_t>({kMemValue});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVILui);

  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3), kMemValue & ~0xfff);
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32INop) {
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVINop);
  // Verify that the semantic functions executes without any operands.
  instruction_->Execute(nullptr);
}

TEST_F(RVCheriotIInstructionTest, RV32IBeq) {
  AppendRegisterOperands({kC1, kC2}, {});
  AppendImmediateOperands<int32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVIBeq);

  SetRegisterValues<int32_t>(
      {{kC1, kVal1}, {kC2, kVal1}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(state_->pcc()->address(), kBranchTarget);

  SetRegisterValues<int32_t>(
      {{kC1, kVal1}, {kC2, kVal2}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(state_->pcc()->address(), kInstAddress);
}

TEST_F(RVCheriotIInstructionTest, RV32IBne) {
  AppendRegisterOperands({kC1, kC2}, {});
  AppendImmediateOperands<int32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVIBne);

  SetRegisterValues<int32_t>(
      {{kC1, kVal1}, {kC2, kVal2}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(state_->pcc()->address(), kBranchTarget);

  SetRegisterValues<int32_t>(
      {{kC1, kVal1}, {kC2, kVal1}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(state_->pcc()->address(), kInstAddress);
}

TEST_F(RVCheriotIInstructionTest, RV32IBlt) {
  AppendRegisterOperands({kC1, kC2}, {});
  AppendImmediateOperands<int32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVIBlt);

  SetRegisterValues<int32_t>(
      {{kC1, kVal1}, {kC2, kVal1}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<uint32_t>(CheriotState::kPcName), kInstAddress);

  SetRegisterValues<int32_t>(
      {{kC1, kVal1}, {kC2, kVal2}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<uint32_t>(CheriotState::kPcName), kInstAddress);

  SetRegisterValues<int32_t>(
      {{kC1, kVal2}, {kC2, kVal1}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<uint32_t>(CheriotState::kPcName), kBranchTarget);
}

TEST_F(RVCheriotIInstructionTest, RV32IBltu) {
  AppendRegisterOperands({kC1, kC2}, {});
  AppendImmediateOperands<int32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVIBltu);

  SetRegisterValues<int32_t>(
      {{kC1, kVal1}, {kC2, kVal1}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<uint32_t>(CheriotState::kPcName), kInstAddress);

  SetRegisterValues<int32_t>(
      {{kC1, kVal1}, {kC2, kVal2}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<uint32_t>(CheriotState::kPcName), kBranchTarget);

  SetRegisterValues<int32_t>(
      {{kC1, kVal2}, {kC2, kVal1}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<uint32_t>(CheriotState::kPcName), kInstAddress);
}

TEST_F(RVCheriotIInstructionTest, RV32IBge) {
  AppendRegisterOperands({kC1, kC2}, {});
  AppendImmediateOperands<int32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVIBge);

  SetRegisterValues<int32_t>(
      {{kC1, kVal1}, {kC2, kVal1}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<uint32_t>(CheriotState::kPcName), kBranchTarget);

  SetRegisterValues<int32_t>(
      {{kC1, kVal1}, {kC2, kVal2}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<uint32_t>(CheriotState::kPcName), kBranchTarget);

  SetRegisterValues<int32_t>(
      {{kC1, kVal2}, {kC2, kVal1}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<uint32_t>(CheriotState::kPcName), kInstAddress);
}

TEST_F(RVCheriotIInstructionTest, RV32IBgeu) {
  AppendRegisterOperands({kC1, kC2}, {});
  AppendImmediateOperands<int32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVIBgeu);

  SetRegisterValues<int32_t>(
      {{kC1, kVal1}, {kC2, kVal1}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<uint32_t>(CheriotState::kPcName), kBranchTarget);

  SetRegisterValues<int32_t>(
      {{kC1, kVal1}, {kC2, kVal2}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<uint32_t>(CheriotState::kPcName), kInstAddress);

  SetRegisterValues<int32_t>(
      {{kC1, kVal2}, {kC2, kVal1}, {CheriotState::kPcName, kInstAddress}});
  instruction_->Execute(nullptr);
  EXPECT_EQ(GetRegisterValue<uint32_t>(CheriotState::kPcName), kBranchTarget);
}

// Load instructions require additional setup. First the memory locations have
// to be initialized. Second, all load instructions use a child instruction for
// the value writeback to the destination register.

TEST_F(RVCheriotIInstructionTest, RV32ILw) {
  // Initialize memory.
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, kMemValue);
  state_->StoreMemory(instruction_, kMemAddress + kOffset, db);
  db->DecRef();

  // Initialize instruction.
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVILw);
  auto* child = new Instruction(state_);
  child->set_semantic_function(&::mpact::sim::cheriot::RiscVILwChild);
  AppendRegisterOperands(child, {}, {kC3});
  instruction_->AppendChild(child);
  child->DecRef();

  creg_1_->ResetMemoryRoot();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, 0}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_FALSE(trap_taken_);
  EXPECT_EQ(GetRegisterValue<uint32_t>(kC3), kMemValue);
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32ILwTagViolation) {
  // Initialize memory.
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, kMemValue);
  state_->StoreMemory(instruction_, kMemAddress + kOffset, db);
  db->DecRef();

  // Initialize instruction.
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVILw);
  auto* child = new Instruction(state_);
  child->set_semantic_function(&::mpact::sim::cheriot::RiscVILwChild);
  AppendRegisterOperands(child, {}, {kC3});
  instruction_->AppendChild(child);
  child->DecRef();

  creg_1_->Invalidate();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, 0}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_NE(GetRegisterValue<uint32_t>(kC3), kMemValue);
  EXPECT_TRUE(trap_taken_);
  EXPECT_FALSE(trap_is_interrupt_);
  EXPECT_EQ(trap_epc_, instruction_->address());
  EXPECT_EQ(trap_value_, (kC1Num << 5) | *CH_EC::kCapExTagViolation);
  EXPECT_EQ(trap_exception_code_, CheriotState::kCheriExceptionCode);
  EXPECT_EQ(trap_inst_, instruction_);
}

TEST_F(RVCheriotIInstructionTest, RV32ILwSealViolation) {
  // Initialize memory.
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, kMemValue);
  state_->StoreMemory(instruction_, kMemAddress + kOffset, db);
  db->DecRef();

  // Initialize instruction.
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVILw);
  auto* child = new Instruction(state_);
  child->set_semantic_function(&::mpact::sim::cheriot::RiscVILwChild);
  AppendRegisterOperands(child, {}, {kC3});
  instruction_->AppendChild(child);
  child->DecRef();

  creg_1_->ResetMemoryRoot();
  (void)creg_1_->Seal(*state_->sealing_root(), 9);
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, 0}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_NE(GetRegisterValue<uint32_t>(kC3), kMemValue);
  EXPECT_TRUE(trap_taken_);
  EXPECT_FALSE(trap_is_interrupt_);
  EXPECT_EQ(trap_epc_, instruction_->address());
  EXPECT_EQ(trap_value_, (kC1Num << 5) | *CH_EC::kCapExSealViolation);
  EXPECT_EQ(trap_exception_code_, CheriotState::kCheriExceptionCode);
  EXPECT_EQ(trap_inst_, instruction_);
}

TEST_F(RVCheriotIInstructionTest, RV32ILwPermitLoadViolation) {
  // Initialize memory.
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, kMemValue);
  state_->StoreMemory(instruction_, kMemAddress + kOffset, db);
  db->DecRef();

  // Initialize instruction.
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVILw);
  auto* child = new Instruction(state_);
  child->set_semantic_function(&::mpact::sim::cheriot::RiscVILwChild);
  AppendRegisterOperands(child, {}, {kC3});
  instruction_->AppendChild(child);
  child->DecRef();

  creg_1_->ResetMemoryRoot();
  creg_1_->ClearPermissions(PB::kPermitLoad);
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, 0}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_NE(GetRegisterValue<uint32_t>(kC3), kMemValue);
  EXPECT_TRUE(trap_taken_);
  EXPECT_FALSE(trap_is_interrupt_);
  EXPECT_EQ(trap_epc_, instruction_->address());
  EXPECT_EQ(trap_value_, (kC1Num << 5) | *CH_EC::kCapExPermitLoadViolation);
  EXPECT_EQ(trap_exception_code_, CheriotState::kCheriExceptionCode);
  EXPECT_EQ(trap_inst_, instruction_);
}

TEST_F(RVCheriotIInstructionTest, RV32ILwBoundsViolation) {
  // Initialize memory.
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, kMemValue);
  state_->StoreMemory(instruction_, kMemAddress + kOffset, db);
  db->DecRef();

  // Initialize instruction.
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVILw);
  auto* child = new Instruction(state_);
  child->set_semantic_function(&::mpact::sim::cheriot::RiscVILwChild);
  AppendRegisterOperands(child, {}, {kC3});
  instruction_->AppendChild(child);
  child->DecRef();

  creg_1_->ResetMemoryRoot();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, 0}});
  creg_1_->SetBounds(kMemAddress, kOffset - 0x100);
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_NE(GetRegisterValue<uint32_t>(kC3), kMemValue);
  EXPECT_TRUE(trap_taken_);
  EXPECT_FALSE(trap_is_interrupt_);
  EXPECT_EQ(trap_epc_, instruction_->address());
  EXPECT_EQ(trap_value_, (kC1Num << 5) | *CH_EC::kCapExBoundsViolation);
  EXPECT_EQ(trap_exception_code_, CheriotState::kCheriExceptionCode);
  EXPECT_EQ(trap_inst_, instruction_);
}

TEST_F(RVCheriotIInstructionTest, RV32ILh) {
  // Initialize memory.
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, kMemValue);
  state_->StoreMemory(instruction_, kMemAddress + kOffset, db);
  db->DecRef();

  // Initialize instruction.
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVILh);
  auto* child = new Instruction(state_);
  child->set_semantic_function(&::mpact::sim::cheriot::RiscVILhChild);
  AppendRegisterOperands(child, {}, {kC3});
  instruction_->AppendChild(child);
  child->DecRef();

  creg_1_->ResetMemoryRoot();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, 0}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_FALSE(trap_taken_);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3), static_cast<int16_t>(kMemValue));
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32ILhu) {
  // Initialize memory.
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, kMemValue);
  state_->StoreMemory(instruction_, kMemAddress + kOffset, db);
  db->DecRef();

  // Initialize instruction.
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVILhu);
  auto* child = new Instruction(state_);
  child->set_semantic_function(&::mpact::sim::cheriot::RiscVILhuChild);
  AppendRegisterOperands(child, {}, {kC3});
  instruction_->AppendChild(child);
  child->DecRef();

  creg_1_->ResetMemoryRoot();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, 0}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_FALSE(trap_taken_);
  EXPECT_EQ(GetRegisterValue<uint32_t>(kC3), static_cast<uint16_t>(kMemValue));
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32ILb) {
  // Initialize memory.
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, kMemValue);
  state_->StoreMemory(instruction_, kMemAddress + kOffset, db);
  db->DecRef();

  // Initialize instruction.
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVILb);
  auto* child = new Instruction(state_);
  child->set_semantic_function(&::mpact::sim::cheriot::RiscVILbChild);
  AppendRegisterOperands(child, {}, {kC3});
  instruction_->AppendChild(child);
  child->DecRef();

  creg_1_->ResetMemoryRoot();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, 0}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_FALSE(trap_taken_);
  EXPECT_EQ(GetRegisterValue<int32_t>(kC3), static_cast<int8_t>(kMemValue));
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->base(), creg_3_->address() & ~0x1ff);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

TEST_F(RVCheriotIInstructionTest, RV32ILbu) {
  // Initialize memory.
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  db->Set<uint32_t>(0, kMemValue);
  state_->StoreMemory(instruction_, kMemAddress + kOffset, db);
  db->DecRef();

  // Initialize instruction.
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVILbu);
  auto* child = new Instruction(state_);
  child->set_semantic_function(&::mpact::sim::cheriot::RiscVILbuChild);
  AppendRegisterOperands(child, {}, {kC3});
  instruction_->AppendChild(child);
  child->DecRef();

  creg_1_->ResetMemoryRoot();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, 0}});
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  EXPECT_FALSE(trap_taken_);
  EXPECT_EQ(GetRegisterValue<uint32_t>(kC3), static_cast<uint8_t>(kMemValue));
  EXPECT_FALSE(creg_3_->tag());
  EXPECT_EQ(creg_3_->top(), 0);
  EXPECT_EQ(creg_3_->base(), 0);
  EXPECT_EQ(creg_3_->permissions(), 0);
  EXPECT_EQ(creg_3_->object_type(), 0);
}

// Store instructions are similar to the ALU instructions, except that
// additional code is added after executing the instruction to fetch the value
// stored to memory.
TEST_F(RVCheriotIInstructionTest, RV32ISw) {
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  AppendRegisterOperands({kC3}, {});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISw);
  creg_1_->ResetMemoryRoot();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, kMemValue}});
  instruction_->Execute(nullptr);
  EXPECT_FALSE(trap_taken_);
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  state_->LoadMemory(instruction_, kMemAddress + kOffset, db, nullptr, nullptr);
  EXPECT_EQ(db->Get<uint32_t>(0), kMemValue);
  db->DecRef();
}

TEST_F(RVCheriotIInstructionTest, RV32ISwTagViolation) {
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  AppendRegisterOperands({kC3}, {});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISw);
  creg_1_->ResetMemoryRoot();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, kMemValue}});
  creg_1_->Invalidate();
  instruction_->Execute(nullptr);
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  state_->LoadMemory(instruction_, kMemAddress + kOffset, db, nullptr, nullptr);
  EXPECT_NE(db->Get<uint32_t>(0), kMemValue);
  db->DecRef();

  EXPECT_TRUE(trap_taken_);
  EXPECT_FALSE(trap_is_interrupt_);
  EXPECT_EQ(trap_epc_, instruction_->address());
  EXPECT_EQ(trap_value_, (kC1Num << 5) | *CH_EC::kCapExTagViolation);
  EXPECT_EQ(trap_exception_code_, CheriotState::kCheriExceptionCode);
  EXPECT_EQ(trap_inst_, instruction_);
}

TEST_F(RVCheriotIInstructionTest, RV32ISwSealViolation) {
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  AppendRegisterOperands({kC3}, {});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISw);
  creg_1_->ResetMemoryRoot();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, kMemValue}});
  (void)creg_1_->Seal(*state_->sealing_root(), 9);
  instruction_->Execute(nullptr);
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  state_->LoadMemory(instruction_, kMemAddress + kOffset, db, nullptr, nullptr);
  EXPECT_NE(db->Get<uint32_t>(0), kMemValue);
  db->DecRef();

  EXPECT_TRUE(trap_taken_);
  EXPECT_FALSE(trap_is_interrupt_);
  EXPECT_EQ(trap_epc_, instruction_->address());
  EXPECT_EQ(trap_value_, (kC1Num << 5) | *CH_EC::kCapExSealViolation);
  EXPECT_EQ(trap_exception_code_, CheriotState::kCheriExceptionCode);
  EXPECT_EQ(trap_inst_, instruction_);
}

TEST_F(RVCheriotIInstructionTest, RV32ISwPermitLoadViolation) {
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  AppendRegisterOperands({kC3}, {});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISw);
  creg_1_->ResetMemoryRoot();
  creg_1_->ClearPermissions(PB::kPermitStore);
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, kMemValue}});
  instruction_->Execute(nullptr);
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  state_->LoadMemory(instruction_, kMemAddress + kOffset, db, nullptr, nullptr);
  EXPECT_NE(db->Get<uint32_t>(0), kMemValue);
  db->DecRef();

  EXPECT_TRUE(trap_taken_);
  EXPECT_FALSE(trap_is_interrupt_);
  EXPECT_EQ(trap_epc_, instruction_->address());
  EXPECT_EQ(trap_value_, (kC1Num << 5) | *CH_EC::kCapExPermitStoreViolation);
  EXPECT_EQ(trap_exception_code_, CheriotState::kCheriExceptionCode);
  EXPECT_EQ(trap_inst_, instruction_);
}

TEST_F(RVCheriotIInstructionTest, RV32ISwBoundsViolation) {
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  AppendRegisterOperands({kC3}, {});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISw);
  creg_1_->ResetMemoryRoot();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, kMemValue}});
  creg_1_->SetBounds(kMemAddress, kOffset - 0x100);
  creg_3_->ResetMemoryRoot();
  instruction_->Execute(nullptr);
  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  state_->LoadMemory(instruction_, kMemAddress + kOffset, db, nullptr, nullptr);
  EXPECT_NE(db->Get<uint32_t>(0), kMemValue);
  db->DecRef();

  EXPECT_TRUE(trap_taken_);
  EXPECT_FALSE(trap_is_interrupt_);
  EXPECT_EQ(trap_epc_, instruction_->address());
  EXPECT_EQ(trap_value_, (kC1Num << 5) | *CH_EC::kCapExBoundsViolation);
  EXPECT_EQ(trap_exception_code_, CheriotState::kCheriExceptionCode);
  EXPECT_EQ(trap_inst_, instruction_);
}

TEST_F(RVCheriotIInstructionTest, RV32ISh) {
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  AppendRegisterOperands({kC3}, {});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISh);
  creg_1_->ResetMemoryRoot();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, kMemValue}});
  instruction_->Execute(nullptr);
  EXPECT_FALSE(trap_taken_);

  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  state_->LoadMemory(instruction_, kMemAddress + kOffset, db, nullptr, nullptr);
  EXPECT_EQ(db->Get<uint32_t>(0), static_cast<uint16_t>(kMemValue));
  db->DecRef();
}

TEST_F(RVCheriotIInstructionTest, RV32ISb) {
  AppendRegisterOperands({kC1}, {});
  AppendImmediateOperands<uint32_t>({kOffset});
  AppendRegisterOperands({kC3}, {});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVISb);
  creg_1_->ResetMemoryRoot();
  SetRegisterValues<uint32_t>({{kC1, kMemAddress}, {kC3, kMemValue}});
  instruction_->Execute(nullptr);
  EXPECT_FALSE(trap_taken_);

  auto* db = state_->db_factory()->Allocate<uint32_t>(1);
  state_->LoadMemory(instruction_, kMemAddress + kOffset, db, nullptr, nullptr);
  EXPECT_EQ(db->Get<uint32_t>(0), static_cast<uint8_t>(kMemValue));
  db->DecRef();
}

// The following instructions aren't tested yet, as the RV32I state doesn't
// implement these instructions beyond interface stubs.

TEST_F(RVCheriotIInstructionTest, RV32IFence) {
  // TODO: implement test once the RiscVState handles the call.
}

TEST_F(RVCheriotIInstructionTest, RV32IEcall) {
  // TODO: implement test once the RiscVState handles the call.
}

TEST_F(RVCheriotIInstructionTest, RV32IEbreak) {
  // TODO: implement test once the RiscVState handles the call.
}

}  // namespace
