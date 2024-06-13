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

#include "cheriot/riscv_cheriot_zicsr_instructions.h"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_csr_enum.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/immediate_operand.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"
#include "riscv//riscv_csr.h"

// This file contains tests for individual Zicsr instructions.

namespace {

using EC = ::mpact::sim::cheriot::ExceptionCode;
using PB = ::mpact::sim::cheriot::CheriotRegister::PermissionBits;
using ::mpact::sim::cheriot::CheriotRegister;
using ::mpact::sim::cheriot::CheriotState;
using ::mpact::sim::cheriot::RiscVCheriotCsrEnum;
using ::mpact::sim::generic::ImmediateOperand;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::riscv::RiscV32SimpleCsr;
using ::mpact::sim::util::TaggedFlatDemandMemory;

constexpr uint32_t kInstAddress = 0x2468;

constexpr char kX1[] = "x1";
constexpr char kX3[] = "x3";

constexpr uint32_t kMScratchValue =
    static_cast<uint32_t>(RiscVCheriotCsrEnum::kMScratch);
constexpr uint32_t kCycleValue =
    static_cast<uint32_t>(RiscVCheriotCsrEnum::kCycle);

// The test fixture allocates a machine state object and an instruction object.
// It also contains convenience methods for interacting with the instruction
// object in a more short hand form.
class ZicsrInstructionsTest : public testing::Test {
 protected:
  ZicsrInstructionsTest() {
    mem_ = new TaggedFlatDemandMemory(8);
    state_ = new CheriotState("test", mem_, nullptr);
    instruction_ = new Instruction(kInstAddress, state_);
    instruction_->set_size(4);
    state_->set_on_trap([this](bool is_interrupt, uint64_t trap_value,
                               uint64_t exception_code, uint64_t epc,
                               const Instruction *inst) {
      return TrapHandler(is_interrupt, trap_value, exception_code, epc, inst);
    });
  }

  ~ZicsrInstructionsTest() override {
    delete instruction_;
    delete mem_;
    delete state_;
  }

  // Appends the source and destination operands for the register names
  // given in the two vectors.
  void AppendRegisterOperands(Instruction *inst,
                              absl::Span<const std::string> sources,
                              absl::Span<const std::string> destinations) {
    for (auto &reg_name : sources) {
      auto *reg = state_->GetRegister<CheriotRegister>(reg_name).first;
      inst->AppendSource(reg->CreateSourceOperand());
    }
    for (auto &reg_name : destinations) {
      auto *reg = state_->GetRegister<CheriotRegister>(reg_name).first;
      inst->AppendDestination(reg->CreateDestinationOperand(0));
    }
  }

  // Appends immediate source operands with the given values.
  template <typename T>
  void AppendImmediateOperands(Instruction *inst,
                               const std::vector<T> &values) {
    for (auto value : values) {
      auto *src = new ImmediateOperand<T>(value);
      inst->AppendSource(src);
    }
  }

  // Takes a vector of tuples of register names and values. Fetches each
  // named register and sets it to the corresponding value.
  template <typename T>
  void SetRegisterValues(const std::vector<std::tuple<std::string, T>> values) {
    for (auto &[reg_name, value] : values) {
      auto *reg = state_->GetRegister<CheriotRegister>(reg_name).first;
      auto *db = state_->db_factory()->Allocate<CheriotRegister::ValueType>(1);
      db->Set<T>(0, value);
      reg->SetDataBuffer(db);
      db->DecRef();
    }
  }

  // Initializes the semantic function of the instruction object.
  void SetSemanticFunction(Instruction::SemanticFunction fcn) {
    instruction_->set_semantic_function(fcn);
  }

  // Returns the value of the named register.
  template <typename T>
  T GetRegisterValue(absl::string_view reg_name) {
    auto *reg = state_->GetRegister<CheriotRegister>(reg_name).first;
    return reg->data_buffer()->Get<T>(0);
  }

  // Handler for any traps that are raised. It is used to capture the trap
  // information for checks.
  bool TrapHandler(bool is_interrupt, uint64_t trap_value,
                   uint64_t exception_code, uint64_t epc,
                   const Instruction *inst);

  TaggedFlatDemandMemory *mem_;
  RiscV32SimpleCsr *csr_;
  CheriotState *state_;
  Instruction *instruction_;
  bool trap_taken_ = false;
  bool trap_is_interrupt_ = false;
  uint64_t trap_value_ = 0;
  uint64_t trap_exception_code_ = 0;
  uint64_t trap_epc_ = 0;
  const Instruction *trap_inst_ = nullptr;
};

bool ZicsrInstructionsTest::TrapHandler(bool is_interrupt, uint64_t trap_value,
                                        uint64_t exception_code, uint64_t epc,
                                        const Instruction *inst) {
  trap_taken_ = true;
  trap_is_interrupt_ = is_interrupt;
  trap_value_ = trap_value;
  trap_exception_code_ = exception_code;
  trap_epc_ = epc;
  trap_inst_ = inst;
  return true;
}

constexpr uint32_t kCsrValue1 = 0xaaaa5555;
constexpr uint32_t kCsrValue2 = 0xa5a5a5a5;

// The following tests all follow the same pattern. First the CSR and any
// registers that are used are initialized with known values. Then the
// instruction is initialized with the proper operands. The instruction is
// executed, before checking the values of registers and CSR for correctness.

// Tests the plain Csrrw/Csrrwi function.
TEST_F(ZicsrInstructionsTest, RiscVZiCsrrw) {
  auto result = state_->csr_set()->GetCsr(kMScratchValue);
  CHECK_OK(result);
  auto *csr = result.value();
  CHECK_NE(csr, nullptr);
  csr->Set(kCsrValue1);
  SetRegisterValues<uint32_t>({{kX1, kCsrValue2}, {kX3, 0}});
  AppendRegisterOperands(instruction_, {kX1}, {kX3});
  AppendImmediateOperands(instruction_, std::vector<uint32_t>{kMScratchValue});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVZiCsrrw);

  instruction_->Execute(nullptr);

  EXPECT_EQ(GetRegisterValue<uint32_t>(kX1), kCsrValue2);
  EXPECT_EQ(GetRegisterValue<uint32_t>(kX3), kCsrValue1);

  EXPECT_EQ(csr->AsUint32(), kCsrValue2);
}

// Tests the plain Csrrs/Csrrsi function.
TEST_F(ZicsrInstructionsTest, RiscVZiCsrrs) {
  auto result = state_->csr_set()->GetCsr(kMScratchValue);
  CHECK_OK(result);
  auto *csr = result.value();
  CHECK_NE(csr, nullptr);
  csr->Set(kCsrValue1);
  SetRegisterValues<uint32_t>({{kX1, kCsrValue2}, {kX3, 0}});
  AppendRegisterOperands(instruction_, {kX1}, {kX3});
  AppendImmediateOperands(instruction_, std::vector<uint32_t>{kMScratchValue});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVZiCsrrs);

  instruction_->Execute(nullptr);

  EXPECT_EQ(GetRegisterValue<uint32_t>(kX1), kCsrValue2);
  EXPECT_EQ(GetRegisterValue<uint32_t>(kX3), kCsrValue1);
  EXPECT_EQ(csr->AsUint32(), kCsrValue1 | kCsrValue2);
}

// Tests the plain Cssrrc/Csrrci function.
TEST_F(ZicsrInstructionsTest, RiscVZiCsrrc) {
  auto result = state_->csr_set()->GetCsr(kMScratchValue);
  CHECK_OK(result);
  auto *csr = result.value();
  CHECK_NE(csr, nullptr);
  csr->Set(kCsrValue1);
  SetRegisterValues<uint32_t>({{kX1, kCsrValue2}, {kX3, 0}});
  AppendRegisterOperands(instruction_, {kX1}, {kX3});
  AppendImmediateOperands(instruction_, std::vector<uint32_t>{kMScratchValue});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVZiCsrrc);

  instruction_->Execute(nullptr);

  EXPECT_EQ(GetRegisterValue<uint32_t>(kX1), kCsrValue2);
  EXPECT_EQ(GetRegisterValue<uint32_t>(kX3), kCsrValue1);
  EXPECT_EQ(csr->AsUint32(), kCsrValue1 & ~kCsrValue2);
}

// Tests Cssrw when the CSR register isn't read (register source is x0).
TEST_F(ZicsrInstructionsTest, RiscVZiCsrrwNr) {
  auto result = state_->csr_set()->GetCsr(kMScratchValue);
  CHECK_OK(result);
  auto *csr = result.value();
  CHECK_NE(csr, nullptr);
  csr->Set(kCsrValue1);
  SetRegisterValues<uint32_t>({{kX1, kCsrValue2}, {kX3, 0}});
  AppendRegisterOperands(instruction_, {kX1}, {kX3});
  AppendImmediateOperands(instruction_, std::vector<uint32_t>{kMScratchValue});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVZiCsrrwNr);

  instruction_->Execute(nullptr);

  EXPECT_EQ(GetRegisterValue<uint32_t>(kX1), kCsrValue2);
  EXPECT_EQ(GetRegisterValue<uint32_t>(kX3), 0);
  EXPECT_EQ(csr->AsUint32(), kCsrValue2);
}

// Tests Cssr[wcs]i when the CSR register isn't written (immediate is 0).
TEST_F(ZicsrInstructionsTest, RiscVZiCsrrNw) {
  auto result = state_->csr_set()->GetCsr(kMScratchValue);
  CHECK_OK(result);
  auto *csr = result.value();
  CHECK_NE(csr, nullptr);
  csr->Set(kCsrValue1);
  SetRegisterValues<uint32_t>({{kX1, kCsrValue2}, {kX3, 0}});
  AppendImmediateOperands(instruction_, std::vector<uint32_t>{kMScratchValue});
  AppendRegisterOperands(instruction_, {}, {kX3});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVZiCsrrNw);

  instruction_->Execute(nullptr);

  EXPECT_EQ(GetRegisterValue<uint32_t>(kX1), kCsrValue2);
  EXPECT_EQ(GetRegisterValue<uint32_t>(kX3), kCsrValue1);
  EXPECT_EQ(csr->AsUint32(), kCsrValue1);
}

// Test for trap if accessing without required permission in pcc.
TEST_F(ZicsrInstructionsTest, RiscVZiCsrrNwTrap) {
  state_->pcc()->ClearPermissions(PB::kPermitAccessSystemRegisters);
  SetRegisterValues<uint32_t>({{kX1, kCsrValue2}, {kX3, 0}});
  AppendImmediateOperands(instruction_, std::vector<uint32_t>{kMScratchValue});
  AppendRegisterOperands(instruction_, {kX1}, {kX3});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVZiCsrrNw);

  instruction_->Execute(nullptr);

  EXPECT_TRUE(trap_taken_);
  EXPECT_FALSE(trap_is_interrupt_);
  EXPECT_EQ(trap_value_,
            (0b1'00000 << 5) | *EC::kCapExPermitAccessSystemRegistersViolation);
  EXPECT_EQ(trap_exception_code_, CheriotState::kCheriExceptionCode);
  EXPECT_EQ(trap_epc_, instruction_->address());
  EXPECT_EQ(trap_inst_, trap_inst_);
}

// Test for no trap if accessing 'cycle' without required permission in pcc, as
// a small subset of CSRs are user mode accessible, and thus does not require
// pcc permission bit.
TEST_F(ZicsrInstructionsTest, RiscVZiCsrrNwNoTrap) {
  state_->pcc()->ClearPermissions(PB::kPermitAccessSystemRegisters);
  SetRegisterValues<uint32_t>({{kX1, kCsrValue2}, {kX3, 0}});
  AppendImmediateOperands(instruction_, std::vector<uint32_t>{kCycleValue});
  AppendRegisterOperands(instruction_, {kX1}, {kX3});
  SetSemanticFunction(&::mpact::sim::cheriot::RiscVZiCsrrNw);

  instruction_->Execute(nullptr);

  EXPECT_FALSE(trap_taken_);
}

}  // namespace
