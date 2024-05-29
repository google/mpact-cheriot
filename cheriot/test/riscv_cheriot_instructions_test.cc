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

#include "cheriot/riscv_cheriot_instructions.h"

#include <array>
#include <cstdint>
#include <ios>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/immediate_operand.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/type_helpers.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"
#include "riscv//riscv_state.h"

// This file contains the unit tests for the RiscV CHERIoT capability specific
// instructions.

namespace {

using ::mpact::sim::generic::operator*;  // NOLINT: used below (clang error).
using ::mpact::sim::cheriot::CheriotRegister;
using PB = CheriotRegister::PermissionBits;
using OT = CheriotRegister::ObjectType;
using ::mpact::sim::cheriot::CheriotState;
using CH_EC = ::mpact::sim::cheriot::ExceptionCode;
using RV_EC = ::mpact::sim::riscv::ExceptionCode;
using ISA = ::mpact::sim::riscv::IsaExtension;
using ::mpact::sim::generic::ImmediateOperand;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::util::TaggedFlatDemandMemory;
// Instruction semantic functions.
using ::mpact::sim::cheriot::CheriotAuicap;
using ::mpact::sim::cheriot::CheriotCAndPerm;
using ::mpact::sim::cheriot::CheriotCClearTag;
using ::mpact::sim::cheriot::CheriotCGetAddr;
using ::mpact::sim::cheriot::CheriotCGetBase;
using ::mpact::sim::cheriot::CheriotCGetHigh;
using ::mpact::sim::cheriot::CheriotCGetLen;
using ::mpact::sim::cheriot::CheriotCGetPerm;
using ::mpact::sim::cheriot::CheriotCGetTag;
using ::mpact::sim::cheriot::CheriotCGetType;
using ::mpact::sim::cheriot::CheriotCIncAddr;
using ::mpact::sim::cheriot::CheriotCJal;
using ::mpact::sim::cheriot::CheriotCJalrCra;
using ::mpact::sim::cheriot::CheriotCLc;
using ::mpact::sim::cheriot::CheriotCLcChild;
using ::mpact::sim::cheriot::CheriotCMove;
using ::mpact::sim::cheriot::CheriotCRepresentableAlignmentMask;
using ::mpact::sim::cheriot::CheriotCRoundRepresentableLength;
using ::mpact::sim::cheriot::CheriotCSc;
using ::mpact::sim::cheriot::CheriotCSeal;
using ::mpact::sim::cheriot::CheriotCSetAddr;
using ::mpact::sim::cheriot::CheriotCSetBounds;
using ::mpact::sim::cheriot::CheriotCSetBoundsExact;
using ::mpact::sim::cheriot::CheriotCSetEqualExact;
using ::mpact::sim::cheriot::CheriotCSetHigh;
using ::mpact::sim::cheriot::CheriotCSpecialR;
using ::mpact::sim::cheriot::CheriotCSpecialRW;
using ::mpact::sim::cheriot::CheriotCSub;
using ::mpact::sim::cheriot::CheriotCTestSubset;
using ::mpact::sim::cheriot::CheriotCUnseal;
// Register name definitions.
constexpr char kCra[] = "c1";
constexpr char kC1[] = "c11";
constexpr char kC2[] = "c12";
constexpr char kC3[] = "c13";
constexpr char kC4[] = "c14";
// Register number definitions.
constexpr int kC1Num = 11;
constexpr int kPccNum = 0b1'00000;

constexpr uint32_t kInstAddress = 0x2468;
constexpr uint32_t kMemAddress = 0x1000;

constexpr int kDataSeal10 = 10;
constexpr int kInstSizeNormal = 4;

// Test fixture.
class RiscVCheriotInstructionsTest : public ::testing::Test {
 protected:
  static constexpr int kCapabilityGranule = 8;

  RiscVCheriotInstructionsTest() {
    memory_ = new TaggedFlatDemandMemory(kCapabilityGranule);
    state_ = new CheriotState("test_state", memory_);
    ResetInstruction(kInstSizeNormal);
    for (auto &[reg_name, cap_reg_ptr] :
         std::vector<std::tuple<std::string, CheriotRegister **>>{
             {kC1, &c1_reg_},
             {kC2, &c2_reg_},
             {kC3, &c3_reg_},
             {kC4, &c4_reg_},
             {kCra, &cra_reg_}}) {
      *cap_reg_ptr = state_->GetRegister<CheriotRegister>(reg_name).first;
    }
    state_->set_on_trap([this](bool is_interrupt, uint64_t trap_value,
                               uint64_t exception_code, uint64_t epc,
                               const Instruction *inst) {
      return TrapHandler(is_interrupt, trap_value, exception_code, epc, inst);
    });
  }

  ~RiscVCheriotInstructionsTest() override {
    inst_->DecRef();
    delete state_;
    delete memory_;
  }

  // Handler for any traps that are raised. It is used to capture the trap
  // information for checks.
  bool TrapHandler(bool is_interrupt, uint64_t trap_value,
                   uint64_t exception_code, uint64_t epc,
                   const Instruction *inst);
  // Clear the captured trap values.
  void ResetTrapHandler();

  void AppendCapabilityOperands(Instruction *inst,
                                absl::Span<const std::string> sources,
                                absl::Span<const std::string> dests) {
    for (auto &reg_name : sources) {
      auto *reg = state_->GetRegister<CheriotRegister>(reg_name).first;
      inst->AppendSource(reg->CreateSourceOperand(reg_name));
    }
    for (auto &reg_name : dests) {
      auto *reg = state_->GetRegister<CheriotRegister>(reg_name).first;
      inst->AppendDestination(reg->CreateDestinationOperand(0, reg_name));
    }
  }

  template <typename T>
  void AppendImmediateOperand(Instruction *inst, T value) {
    auto *src = new ImmediateOperand<T>(value);
    inst_->AppendSource(src);
  }

  template <typename T>
  void AppendImmediateOperands(const std::vector<T> &values) {
    for (auto &value : values) {
      AppendImmediateOperand<T>(inst_, value);
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

  void ResetInstruction(int size) {
    if (inst_ != nullptr) inst_->DecRef();
    inst_ = new Instruction(kInstAddress, state_);
    inst_->set_size(size);
  }

  // Return true if the capability is null (except for address field).
  bool IsNullCapability(CheriotRegister *cap) {
    return cap->is_null() ||
           (cap->top() == 0 && cap->base() == 0 && cap->permissions() == 0 &&
            cap->tag() == 0 && cap->object_type() == 0 && cap->reserved() == 0);
  }

  void SetUpForLoadCapabilityTest(uint32_t address,
                                  const CheriotRegister *cap) {
    ResetInstruction(kInstSizeNormal);
    inst()->set_semantic_function(&CheriotCLc);
    // Add the child instruction.
    auto *child = new Instruction(kInstAddress, state());
    child->set_semantic_function(&CheriotCLcChild);
    inst()->AppendChild(child);
    child->DecRef();
    // Store a capability to memory.
    auto cap_db = state()->db_factory()->Allocate<uint32_t>(2);
    cap_db->Set<uint32_t>(0, 0xdeadbeef);
    cap_db->Set<uint32_t>(1, cap->Compress());
    auto tag_db = state()->db_factory()->Allocate<uint8_t>(1);
    tag_db->Set<uint8_t>(0, 1);
    state()->StoreCapability(inst(), address, cap_db, tag_db);
    cap_db->DecRef();
    tag_db->DecRef();
  }

  // Accessors.
  Instruction *inst() { return inst_; }
  TaggedFlatDemandMemory *memory() { return memory_; }
  CheriotState *state() { return state_; }
  // Capability register pointers.
  CheriotRegister *c1_reg() { return c1_reg_; }
  CheriotRegister *c2_reg() { return c2_reg_; }
  CheriotRegister *c3_reg() { return c3_reg_; }
  CheriotRegister *c4_reg() { return c4_reg_; }
  CheriotRegister *cra_reg() { return cra_reg_; }
  absl::BitGen &bitgen() { return bitgen_; }
  bool trap_taken() { return trap_taken_; }
  bool trap_is_interrupt() { return trap_is_interrupt_; }
  uint64_t trap_value() { return trap_value_; }
  uint64_t trap_exception_code() { return trap_exception_code_; }
  uint64_t trap_epc() { return trap_epc_; }
  const Instruction *trap_inst() { return trap_inst_; }

 private:
  Instruction *inst_ = nullptr;
  TaggedFlatDemandMemory *memory_;
  CheriotState *state_;
  CheriotRegister *c1_reg_;
  CheriotRegister *c2_reg_;
  CheriotRegister *c3_reg_;
  CheriotRegister *c4_reg_;
  CheriotRegister *cra_reg_;
  absl::BitGen bitgen_;
  bool trap_taken_ = false;
  bool trap_is_interrupt_ = false;
  uint64_t trap_value_ = 0;
  uint64_t trap_exception_code_ = 0;
  uint64_t trap_epc_ = 0;
  const Instruction *trap_inst_ = nullptr;
};

bool RiscVCheriotInstructionsTest::TrapHandler(bool is_interrupt,
                                               uint64_t trap_value,
                                               uint64_t exception_code,
                                               uint64_t epc,
                                               const Instruction *inst) {
  trap_taken_ = true;
  trap_is_interrupt_ = is_interrupt;
  trap_value_ = trap_value;
  trap_exception_code_ = exception_code;
  trap_epc_ = epc;
  trap_inst_ = inst;
  return true;
}

void RiscVCheriotInstructionsTest::ResetTrapHandler() {
  trap_taken_ = false;
  trap_is_interrupt_ = false;
  trap_value_ = 0;
  trap_exception_code_ = 0;
  trap_epc_ = 0;
  trap_inst_ = nullptr;
}

TEST_F(RiscVCheriotInstructionsTest, Auicap) {
  inst()->set_semantic_function(&CheriotAuicap);
  // Make the source register the memory root.
  c1_reg()->ResetMemoryRoot();
  // Try different offsets.
  for (int32_t offset : {16, 1024, 0, -16 - 1024}) {
    AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
    c1_reg()->set_address(kMemAddress);
    c2_reg()->set_address(offset);
    inst()->Execute(nullptr);
    // Verify that the source didn't change, and that the value of the
    // destination capability is as expected.
    EXPECT_EQ(c1_reg()->address(), kMemAddress);
    EXPECT_EQ(c3_reg()->address(), kMemAddress + offset);
    EXPECT_EQ(c3_reg()->top(), c1_reg()->top());
    EXPECT_EQ(c3_reg()->base(), c1_reg()->base());
    EXPECT_TRUE(c3_reg()->tag());
  }
  // Try with c1 being sealed.
  CHECK_OK(c1_reg()->Seal(*state()->sealing_root(), kDataSeal10));
  uint32_t offset = 0x1000;
  c2_reg()->set_address(offset);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), kMemAddress + offset);
  EXPECT_EQ(c3_reg()->top(), c1_reg()->top());
  EXPECT_EQ(c3_reg()->base(), c1_reg()->base());
  EXPECT_FALSE(c3_reg()->tag());
  // Now limit c1 to a smaller range and set the offset outside that.
  c1_reg()->ResetMemoryRoot();
  offset = 0x210;
  c1_reg()->SetBounds(kMemAddress, offset - 16);
  c1_reg()->set_address(kMemAddress);
  EXPECT_TRUE(c1_reg()->tag());
  // Set the offset to be just outside.
  offset = (1 << (c1_reg()->exponent() + 9)) + 1;
  c2_reg()->set_address(offset);
  inst()->Execute(nullptr);
  EXPECT_EQ(c1_reg()->address(), kMemAddress);
  EXPECT_EQ(c3_reg()->address(), kMemAddress + offset);
  EXPECT_EQ(c3_reg()->top(), c1_reg()->top());
  EXPECT_EQ(c3_reg()->base(), c1_reg()->base());
  EXPECT_FALSE(c3_reg()->tag());
}

// Verify that permission removal using CAndPerm works.
TEST_F(RiscVCheriotInstructionsTest, CAndPerm) {
  uint32_t mask = PB::kPermitGlobal | PB::kPermitLoadGlobal | PB::kPermitStore |
                  PB::kPermitLoadMutable | PB::kPermitStoreLocalCapability |
                  PB::kPermitLoad | PB::kPermitLoadStoreCapability;
  inst()->set_semantic_function(&CheriotCAndPerm);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC1});
  c1_reg()->ResetMemoryRoot();  // Full memory permissions.
  uint32_t and_mask = mask;
  uint32_t expected = PB::kPermitGlobal | PB::kPermitLoadGlobal |
                      PB::kPermitLoadMutable | PB::kPermitStoreLocalCapability |
                      PB::kPermitLoadStoreCapability | PB::kPermitStore |
                      PB::kPermitLoad;
  EXPECT_EQ(c1_reg()->permissions(), expected);
  for (uint32_t i :
       {PB::kPermitGlobal, PB::kPermitLoadGlobal, PB::kPermitLoadMutable,
        PB::kPermitStoreLocalCapability, PB::kPermitLoadStoreCapability,
        PB::kPermitStore, PB::kPermitLoad}) {
    and_mask = mask & ~i;
    expected &= and_mask;
    c2_reg()->set_address(and_mask);
    inst()->Execute(nullptr);
    EXPECT_EQ(c1_reg()->permissions(), expected) << absl::StrFormat(
        "p: %08x expected: %08x\n", c1_reg()->permissions(), expected);
    EXPECT_TRUE(c1_reg()->tag());
  }
  // A sealed capability should clear the tag.
  c1_reg()->ResetMemoryRoot();
  CHECK_OK(c1_reg()->Seal(*state()->sealing_root(), kDataSeal10));
  expected = PB::kPermitGlobal | PB::kPermitLoadGlobal |
             PB::kPermitLoadMutable | PB::kPermitStoreLocalCapability |
             PB::kPermitLoadStoreCapability | PB::kPermitStore |
             PB::kPermitLoad;
  and_mask = mask & ~PB::kPermitGlobal;
  expected &= and_mask;
  c2_reg()->set_address(and_mask);
  inst()->Execute(nullptr);
  EXPECT_EQ(c1_reg()->permissions(), expected) << absl::StrFormat(
      "p: %08x expected: %08x\n", c1_reg()->permissions(), expected);
  EXPECT_FALSE(c1_reg()->tag());
}

// Verify that CClearTag clears the tag properly.
TEST_F(RiscVCheriotInstructionsTest, CClearTag) {
  inst()->set_semantic_function(&CheriotCClearTag);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  c1_reg()->ResetMemoryRoot();
  // Make c3 reg tag true.
  c3_reg()->ResetMemoryRoot();
  EXPECT_TRUE(c3_reg()->tag());
  inst()->Execute(nullptr);
  EXPECT_FALSE(c3_reg()->tag());
}

// Verify that the correct address is returned.
TEST_F(RiscVCheriotInstructionsTest, CGetAddr) {
  inst()->set_semantic_function(&CheriotCGetAddr);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c3_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 0);
  c1_reg()->set_address(kMemAddress);
  c3_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), kMemAddress);
  EXPECT_TRUE(IsNullCapability(c3_reg()));
}

// Verify that the correct base is returned.
TEST_F(RiscVCheriotInstructionsTest, CGetBase) {
  inst()->set_semantic_function(&CheriotCGetBase);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c3_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->base(), 0);
  EXPECT_TRUE(c1_reg()->SetBounds(kMemAddress, 0x200));
  c1_reg()->set_address(kMemAddress);
  c3_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), kMemAddress);
  EXPECT_TRUE(IsNullCapability(c3_reg()));
}

// Verify that the correct 'compressed' value is returned.
TEST_F(RiscVCheriotInstructionsTest, CGetHigh) {
  inst()->set_semantic_function(&CheriotCGetHigh);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c3_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), c1_reg()->Compress());
  EXPECT_TRUE(c1_reg()->SetBounds(kMemAddress, 0x200));
  c3_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), c1_reg()->Compress());
  EXPECT_TRUE(IsNullCapability(c3_reg()));
  c1_reg()->ResetNull();
  c3_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 0);
  EXPECT_TRUE(IsNullCapability(c3_reg()));
}

// Verify that the correct length is returned.
TEST_F(RiscVCheriotInstructionsTest, CGetLen) {
  inst()->set_semantic_function(&CheriotCGetLen);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c3_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 0xffff'ffff);
  c1_reg()->SetBounds(kMemAddress, 0x200);
  c3_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 0x200);
  EXPECT_TRUE(IsNullCapability(c3_reg()));
}

// Verify that the correct permission bits are returned.
TEST_F(RiscVCheriotInstructionsTest, CGetPerm) {
  inst()->set_semantic_function(&CheriotCGetPerm);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  c1_reg()->ResetNull();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 0);
  EXPECT_TRUE(IsNullCapability(c3_reg()));
  c1_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(),
            PB::kPermitGlobal | PB::kPermitLoadGlobal | PB::kPermitStore |
                PB::kPermitLoadMutable | PB::kPermitStoreLocalCapability |
                PB::kPermitLoad | PB::kPermitLoadStoreCapability);
  EXPECT_TRUE(IsNullCapability(c3_reg()));
  c1_reg()->ResetExecuteRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(),
            PB::kPermitGlobal | PB::kPermitExecute | PB::kPermitLoad |
                PB::kPermitLoadStoreCapability | PB::kPermitLoadGlobal |
                PB::kPermitLoadMutable | PB::kPermitAccessSystemRegisters);
  EXPECT_TRUE(IsNullCapability(c3_reg()));
  c1_reg()->ResetSealingRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), PB::kPermitGlobal | PB::kPermitSeal |
                                     PB::kPermitUnseal | PB::kUserPerm0);
  EXPECT_TRUE(IsNullCapability(c3_reg()));
}

// Verify that CGetTag gets the correct tag value.
TEST_F(RiscVCheriotInstructionsTest, CGetTag) {
  inst()->set_semantic_function(&CheriotCGetTag);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  inst()->Execute(nullptr);
  // Initial value of a capability register is null, so the tag should be false.
  EXPECT_EQ(c3_reg()->address(), 0);
  // Make c1 a valid capability, now the tag should be true.
  c1_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 1);
  EXPECT_TRUE(IsNullCapability(c3_reg()));
}

// Checking that CGetType gets the correct object type.
TEST_F(RiscVCheriotInstructionsTest, CGetType) {
  inst()->set_semantic_function(&CheriotCGetType);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  inst()->Execute(nullptr);
  c1_reg()->ResetExecuteRoot();
  EXPECT_EQ(c3_reg()->address(), 0);
  EXPECT_TRUE(IsNullCapability(c3_reg()));
  for (int i = 0; i < 8; i++) {
    c1_reg()->set_object_type(i);
    inst()->Execute(nullptr);
    EXPECT_EQ(c3_reg()->address(), i & 0x7);
    EXPECT_TRUE(IsNullCapability(c3_reg()));
  }
  c1_reg()->ResetMemoryRoot();
  for (int i = 0; i < 8; i++) {
    c1_reg()->set_object_type(i);
    inst()->Execute(nullptr);
    if (i == 0) {
      EXPECT_EQ(c3_reg()->address(), 0);
    } else {
      EXPECT_EQ(c3_reg()->address(), 0x8 | i);
    }
    EXPECT_TRUE(IsNullCapability(c3_reg()));
  }
}

TEST_F(RiscVCheriotInstructionsTest, CIncAddr) {
  inst()->set_semantic_function(&CheriotCIncAddr);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  // Set up c1 as a valid capability, base kMemAddress, length 200.
  c1_reg()->ResetMemoryRoot();
  c1_reg()->SetBounds(kMemAddress, 0x80);
  c1_reg()->set_address(kMemAddress);
  // Set the value of c2 to 0x10.
  c2_reg()->set_address(0x10);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), kMemAddress + 0x10);
  EXPECT_TRUE(c3_reg()->tag());
  EXPECT_EQ(c3_reg()->top(), c1_reg()->top());
  EXPECT_EQ(c3_reg()->base(), c1_reg()->base());
  // Change increment to 0x200.
  c2_reg()->set_address(0x20);
  // Increment again.
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), kMemAddress + 0x20);
  EXPECT_TRUE(c3_reg()->tag());
  EXPECT_EQ(c3_reg()->top(), c1_reg()->top());
  EXPECT_EQ(c3_reg()->base(), c1_reg()->base());
  // Change increment to 1 << (exponent + 9) + 1;
  c2_reg()->set_address((0x1 << (c1_reg()->exponent() + 9)) + 1);
  // Increment again. This time the tag will be cleared.
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(),
            kMemAddress + (0x1 << (c1_reg()->exponent() + 9)) + 1);
  EXPECT_FALSE(c3_reg()->tag())
      << absl::StrFormat("b: 0x%08x a: 0x%08x e:%d", c3_reg()->base(),
                         c3_reg()->address(), c3_reg()->exponent());
  EXPECT_EQ(c3_reg()->top(), c1_reg()->top());
  EXPECT_EQ(c3_reg()->base(), c1_reg()->base());
  // Change increment back to 0x100.
  c2_reg()->set_address(0x100);
  // Seal the source capability. That will make the tag false.
  EXPECT_TRUE(c1_reg()->Seal(*state()->sealing_root(), kDataSeal10).ok());
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), kMemAddress + 0x100);
  EXPECT_FALSE(c3_reg()->tag());
  EXPECT_EQ(c3_reg()->top(), c1_reg()->top());
  EXPECT_EQ(c3_reg()->base(), c1_reg()->base());
}

// Jump and link - no traps.
TEST_F(RiscVCheriotInstructionsTest, CJal) {
  inst()->set_semantic_function(&CheriotCJal);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  state()->pcc()->set_address(inst()->address());
  c1_reg()->set_address(0x200);
  // Set interrupt enable to true.
  state()->mstatus()->set_mie(1);
  state()->mstatus()->Submit();
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken()) << "ec: " << std::hex << trap_exception_code()
                             << " value: " << trap_value();
  EXPECT_EQ(state()->pcc()->address(), inst()->address() + 0x200);
  EXPECT_TRUE(c3_reg()->tag());
  EXPECT_TRUE(c3_reg()->IsSentry());
  EXPECT_EQ(c3_reg()->object_type(), OT::kInterruptEnablingReturnSentry);
  EXPECT_TRUE(state()->pcc()->tag());
  // Set interrupt enable to false.
  state()->mstatus()->set_mie(0);
  state()->mstatus()->Submit();
  state()->pcc()->set_address(inst()->address());
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken()) << "ec: " << std::hex << trap_exception_code()
                             << " value: " << trap_value();
  EXPECT_EQ(state()->pcc()->address(), inst()->address() + 0x200);
  EXPECT_TRUE(c3_reg()->tag());
  EXPECT_TRUE(c3_reg()->IsSentry());
  EXPECT_EQ(c3_reg()->object_type(), OT::kInterruptDisablingReturnSentry);
  EXPECT_TRUE(state()->pcc()->tag());
}

// Jump and link - out of bounds error.
TEST_F(RiscVCheriotInstructionsTest, CJalOutOfBounds) {
  inst()->set_semantic_function(&CheriotCJal);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  state()->pcc()->set_address(inst()->address());
  c1_reg()->set_address(0x200);
  // Set interrupt enable to true.
  state()->mstatus()->set_mie(1);
  state()->mstatus()->Submit();
  // Restrict the bounds of pcc.
  EXPECT_TRUE(state()->pcc()->SetBounds(kInstAddress, 0x100));
  inst()->Execute(nullptr);
}

// Jump and link - misaligned (jumping to 2 byte aligned address with no
// compact instructions).
TEST_F(RiscVCheriotInstructionsTest, CJalMisaligned) {
  inst()->set_semantic_function(&CheriotCJal);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  state()->pcc()->set_address(inst()->address());
  state()->misa()->Set(state()->misa()->AsUint64() & ~*ISA::kCompressed);
  c1_reg()->set_address(0x202);
  // Set interrupt enable to true.
  state()->mstatus()->set_mie(1);
  state()->mstatus()->Submit();
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), kInstAddress + 0x202);
  EXPECT_EQ(trap_exception_code(), *RV_EC::kInstructionAddressMisaligned);
  EXPECT_EQ(inst(), trap_inst());
}

// Jump and link register (capability) indirect - no traps, unsealed source.
TEST_F(RiscVCheriotInstructionsTest, CJalr) {
  inst()->set_semantic_function(&CheriotCJalrCra);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kCra});
  state()->pcc()->set_address(inst()->address());
  // Set up the destination capability.
  c1_reg()->ResetExecuteRoot();
  c1_reg()->set_address(kInstAddress + 0x100);
  c1_reg()->SetBounds(kInstAddress, 0x400);
  // Set offset.
  c2_reg()->set_address(0x100);
  // Set interrupt enable to true.
  state()->mstatus()->set_mie(1);
  state()->mstatus()->Submit();
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken()) << "ec: " << std::hex << trap_exception_code()
                             << " value: " << trap_value();
  EXPECT_EQ(state()->pcc()->address(), inst()->address() + 0x200);
  EXPECT_TRUE(cra_reg()->tag());
  EXPECT_TRUE(cra_reg()->IsSentry());
  EXPECT_EQ(cra_reg()->object_type(), OT::kInterruptEnablingReturnSentry);
  EXPECT_TRUE(state()->pcc()->tag());
  // Set interrupt enable to false.
  state()->mstatus()->set_mie(0);
  state()->mstatus()->Submit();
  state()->pcc()->set_address(inst()->address());
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken()) << "ec: " << std::hex << trap_exception_code()
                             << " value: " << trap_value();
  EXPECT_EQ(state()->pcc()->address(), inst()->address() + 0x200);
  EXPECT_TRUE(cra_reg()->tag());
  EXPECT_TRUE(cra_reg()->IsSentry());
  EXPECT_EQ(cra_reg()->object_type(), OT::kInterruptDisablingReturnSentry);
  EXPECT_TRUE(state()->pcc()->tag());
}

// Jump and link register (capability) indirect - no traps, sentry.
TEST_F(RiscVCheriotInstructionsTest, CJalrSentry) {
  inst()->set_semantic_function(&CheriotCJalrCra);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kCra});
  state()->pcc()->set_address(inst()->address());
  // Set up the destination capability.
  c1_reg()->ResetExecuteRoot();
  c1_reg()->set_address(kInstAddress + 0x200);
  c1_reg()->SetBounds(kInstAddress, 0x400);
  (void)c1_reg()->Seal(*(state()->sealing_root()),
                       OT::kInterruptEnablingSentry);
  // Set offset to zero (because c1_reg is sealed).
  c2_reg()->set_address(0);
  // Set interrupt enable to false.
  state()->mstatus()->set_mie(0);
  state()->mstatus()->Submit();
  inst()->Execute(nullptr);
  EXPECT_TRUE(state()->mstatus()->mie());
  EXPECT_FALSE(trap_taken()) << "ec: " << std::hex << trap_exception_code()
                             << " value: " << trap_value();
  EXPECT_EQ(state()->pcc()->address(), inst()->address() + 0x200);
  EXPECT_TRUE(cra_reg()->tag());
  EXPECT_TRUE(cra_reg()->IsSentry());
  EXPECT_EQ(cra_reg()->object_type(), OT::kInterruptDisablingReturnSentry);
  EXPECT_TRUE(state()->pcc()->tag());
  // Set up the destination capability.
  c1_reg()->ResetExecuteRoot();
  c1_reg()->SetBounds(kInstAddress, 0x400);
  c1_reg()->set_address(kInstAddress + 0x200);
  (void)c1_reg()->Seal(*(state()->sealing_root()),
                       OT::kInterruptDisablingSentry);
  // Set interrupt enable to true.
  state()->mstatus()->set_mie(1);
  state()->mstatus()->Submit();
  state()->pcc()->set_address(inst()->address());
  inst()->Execute(nullptr);
  EXPECT_FALSE(state()->mstatus()->mie());
  EXPECT_FALSE(trap_taken()) << "ec: " << std::hex << trap_exception_code()
                             << " value: " << trap_value();
  EXPECT_EQ(state()->pcc()->address(), inst()->address() + 0x200);
  EXPECT_TRUE(cra_reg()->tag());
  EXPECT_TRUE(cra_reg()->IsSentry());
  EXPECT_EQ(cra_reg()->object_type(), OT::kInterruptEnablingReturnSentry);
  EXPECT_TRUE(state()->pcc()->tag());
}

// Verify an unset tag generates a tag violation exception.
TEST_F(RiscVCheriotInstructionsTest, CJalrTagViolation) {
  inst()->set_semantic_function(&CheriotCJalrCra);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kCra});
  state()->pcc()->set_address(inst()->address());
  // Set up the destination capability.
  c1_reg()->ResetExecuteRoot();
  c1_reg()->SetBounds(kInstAddress, 0x400);
  c1_reg()->set_address(kInstAddress + 0x200);
  // Clear c1_reg tag.
  c1_reg()->Invalidate();
  // Set offset to non-zero.
  c2_reg()->set_address(0);
  inst()->Execute(nullptr);
  EXPECT_EQ(state()->pcc()->address(), inst()->address());
  EXPECT_FALSE(c3_reg()->tag());
  EXPECT_FALSE(c3_reg()->IsSentry());
  EXPECT_TRUE(trap_taken()) << "ec: " << std::hex << trap_exception_code()
                            << " value: " << trap_value();
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), kC1Num << 5 | *CH_EC::kCapExTagViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

// For a jalr with a sentry, the immediate has to be zero or it will cause
// an exception. Make sure the exception happens.
TEST_F(RiscVCheriotInstructionsTest, CJalrSentryNonZeroImmediate) {
  inst()->set_semantic_function(&CheriotCJalrCra);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kCra});
  state()->pcc()->set_address(inst()->address());
  // Set up the destination capability.
  c1_reg()->ResetExecuteRoot();
  c1_reg()->set_address(kInstAddress + 0x200);
  c1_reg()->SetBounds(kInstAddress, 0x400);
  (void)c1_reg()->Seal(*(state()->sealing_root()),
                       OT::kInterruptEnablingSentry);
  // Set offset to non-zero - should cause an exception.
  c2_reg()->set_address(0x100);
  inst()->Execute(nullptr);
  EXPECT_EQ(state()->pcc()->address(), inst()->address());
  EXPECT_FALSE(c3_reg()->tag());
  EXPECT_FALSE(c3_reg()->IsSentry());
  EXPECT_TRUE(trap_taken()) << "ec: " << std::hex << trap_exception_code()
                            << " value: " << trap_value();
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), kC1Num << 5 | *CH_EC::kCapExSealViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

// If the source capability does not have execute permission, there should
// be an exception.
TEST_F(RiscVCheriotInstructionsTest, CJalrExecuteViolation) {
  inst()->set_semantic_function(&CheriotCJalrCra);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kCra});
  state()->pcc()->set_address(inst()->address());
  // Set up the destination capability.
  c1_reg()->ResetExecuteRoot();
  c1_reg()->SetBounds(kInstAddress, 0x400);
  c1_reg()->set_address(kInstAddress + 0x200);
  c1_reg()->ClearPermissions(PB::kPermitExecute);
  // Clear c1_reg tag.
  c1_reg()->Invalidate();
  // Set offset to non-zero.
  c2_reg()->set_address(0);
  inst()->Execute(nullptr);
  EXPECT_EQ(state()->pcc()->address(), inst()->address());
  EXPECT_FALSE(c3_reg()->tag());
  EXPECT_FALSE(c3_reg()->IsSentry());
  EXPECT_TRUE(trap_taken()) << "ec: " << std::hex << trap_exception_code()
                            << " value: " << trap_value();
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), kC1Num << 5 | *CH_EC::kCapExTagViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

// If the architecture does not have compact instructions, then misaligned
// access on two byte boundary should cause an exception.
TEST_F(RiscVCheriotInstructionsTest, CJalrAlignmentViolation) {
  inst()->set_semantic_function(&CheriotCJalrCra);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  state()->pcc()->set_address(inst()->address());
  // Set up the destination capability.
  c1_reg()->ResetExecuteRoot();
  c1_reg()->set_address(kInstAddress + 0x200);
  // Set offset to non-zero.
  c2_reg()->set_address(2);
  // Clear the compressed isa bit.
  state()->misa()->Set(state()->misa()->AsUint64() & ~*ISA::kCompressed);
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken()) << "ec: " << std::hex << trap_exception_code()
                            << " value: " << trap_value();
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), kInstAddress + 0x202);
  EXPECT_EQ(trap_exception_code(), *RV_EC::kInstructionAddressMisaligned);
  EXPECT_EQ(inst(), trap_inst());
}

// Check load capability - no traps.
TEST_F(RiscVCheriotInstructionsTest, CLc) {
  SetUpForLoadCapabilityTest(kMemAddress + 0x200, state()->memory_root());
  AppendCapabilityOperands(inst(), {kC1, kC2}, {});
  AppendCapabilityOperands(inst()->child(), {}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  c2_reg()->set_address(0x200);
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken());
  EXPECT_TRUE(*c3_reg() == *state()->memory_root());
}

// Load without global flag should clear global flag of loaded capability.
TEST_F(RiscVCheriotInstructionsTest, CLcNoLoadGlobal) {
  c4_reg()->ResetMemoryRoot();
  c4_reg()->ClearPermissions(PB::kPermitGlobal | PB::kPermitLoadGlobal);
  SetUpForLoadCapabilityTest(kMemAddress + 0x200, state()->memory_root());
  AppendCapabilityOperands(inst(), {kC1, kC2}, {});
  AppendCapabilityOperands(inst()->child(), {}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->ClearPermissions(PB::kPermitLoadGlobal);
  c1_reg()->set_address(kMemAddress);
  c2_reg()->set_address(0x200);
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken());
  EXPECT_FALSE(*c3_reg() == *state()->memory_root());
  EXPECT_TRUE(*c3_reg() == *c4_reg());
}

// Load without mutable flag should clear mutable and store permissions of
// unsealed capability.
TEST_F(RiscVCheriotInstructionsTest, CLcNoLoadMutableUnsealed) {
  c4_reg()->ResetMemoryRoot();
  c4_reg()->ClearPermissions(PB::kPermitLoadMutable | PB::kPermitStore);
  SetUpForLoadCapabilityTest(kMemAddress + 0x200, state()->memory_root());
  AppendCapabilityOperands(inst(), {kC1, kC2}, {});
  AppendCapabilityOperands(inst()->child(), {}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->ClearPermissions(PB::kPermitLoadMutable);
  c1_reg()->set_address(kMemAddress);
  c2_reg()->set_address(0x200);
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken());
  EXPECT_FALSE(*c3_reg() == *state()->memory_root());
  EXPECT_TRUE(*c3_reg() == *c4_reg());
}

// Load without mutable flag should not clear mutable and store permissions of
// sealed capability.
TEST_F(RiscVCheriotInstructionsTest, CLcNoLoadMutableSealed) {
  c4_reg()->ResetMemoryRoot();
  (void)c4_reg()->Seal(*(state()->sealing_root()), kDataSeal10);
  SetUpForLoadCapabilityTest(kMemAddress + 0x200, c4_reg());
  AppendCapabilityOperands(inst(), {kC1, kC2}, {});
  AppendCapabilityOperands(inst()->child(), {}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->ClearPermissions(PB::kPermitLoadMutable);
  c1_reg()->set_address(kMemAddress);
  c2_reg()->set_address(0x200);
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken());
  EXPECT_TRUE(*c3_reg() == *c4_reg());
}

TEST_F(RiscVCheriotInstructionsTest, CLcNoLoadStoreCapability) {
  SetUpForLoadCapabilityTest(kMemAddress + 0x200, state()->memory_root());
  AppendCapabilityOperands(inst(), {kC1, kC2}, {});
  AppendCapabilityOperands(inst()->child(), {}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->ClearPermissions(PB::kPermitLoadStoreCapability);
  c1_reg()->set_address(kMemAddress);
  c2_reg()->set_address(0x200);
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken());
  // Result should be equal to memory root, but without the valid tag and some
  // permissions removed.
  c4_reg()->ResetMemoryRoot();
  c4_reg()->set_address(0xdeadbeef);
  c4_reg()->ClearPermissions(
      PB::kPermitGlobal | PB::kPermitLoadGlobal | PB::kPermitLoadMutable |
      PB::kPermitStoreLocalCapability | PB::kPermitStore);
  c4_reg()->Invalidate();
  EXPECT_TRUE(*c3_reg() == *c4_reg()) << "c3_reg(): " << c3_reg()->AsString()
                                      << "\nc4_reg(): " << c4_reg()->AsString();
}

// Check load capability with invalid capability.
TEST_F(RiscVCheriotInstructionsTest, CLcTagViolation) {
  SetUpForLoadCapabilityTest(kMemAddress + 0x200, state()->memory_root());
  AppendCapabilityOperands(inst(), {kC1, kC2}, {});
  AppendCapabilityOperands(inst()->child(), {}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  c1_reg()->Invalidate();
  c2_reg()->set_address(0x200);
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), (kC1Num << 5) | *CH_EC::kCapExTagViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

// Check load capability with sealed capability.
TEST_F(RiscVCheriotInstructionsTest, CLcSealViolation) {
  SetUpForLoadCapabilityTest(kMemAddress + 0x200, state()->memory_root());
  AppendCapabilityOperands(inst(), {kC1, kC2}, {});
  AppendCapabilityOperands(inst()->child(), {}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  (void)c1_reg()->Seal(*(state()->sealing_root()), kDataSeal10);
  c2_reg()->set_address(0x200);
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), (kC1Num << 5) | *CH_EC::kCapExSealViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

// Check load capability with no load permission.
TEST_F(RiscVCheriotInstructionsTest, CLcPermitLoadViolation) {
  SetUpForLoadCapabilityTest(kMemAddress + 0x200, state()->memory_root());
  AppendCapabilityOperands(inst(), {kC1, kC2}, {});
  AppendCapabilityOperands(inst()->child(), {}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  c1_reg()->ClearPermissions(PB::kPermitLoad);
  c2_reg()->set_address(0x200);
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), (kC1Num << 5) | *CH_EC::kCapExPermitLoadViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

// Check load capability with bounds violation.
TEST_F(RiscVCheriotInstructionsTest, CLcPermitBoundsViolation) {
  SetUpForLoadCapabilityTest(kMemAddress + 0x200, state()->memory_root());
  AppendCapabilityOperands(inst(), {kC1, kC2}, {});
  AppendCapabilityOperands(inst()->child(), {}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  c1_reg()->SetBounds(0, kMemAddress + 0x100);
  c2_reg()->set_address(0x200);
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), (kC1Num << 5) | *CH_EC::kCapExBoundsViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

// Check load capability with unaligned address.
TEST_F(RiscVCheriotInstructionsTest, CLcUnaligned) {
  SetUpForLoadCapabilityTest(kMemAddress + 0x200, state()->memory_root());
  AppendCapabilityOperands(inst(), {kC1, kC2}, {});
  AppendCapabilityOperands(inst()->child(), {}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  c2_reg()->set_address(0x201);
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), kMemAddress + 0x201);
  EXPECT_EQ(trap_exception_code(), *RV_EC::kLoadAddressMisaligned);
  EXPECT_EQ(inst(), trap_inst());
}

// Verify that copy works.
TEST_F(RiscVCheriotInstructionsTest, CMove) {
  inst()->set_semantic_function(&CheriotCMove);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  c1_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_TRUE(*c1_reg() == *c3_reg());
  c1_reg()->ResetExecuteRoot();
  inst()->Execute(nullptr);
  EXPECT_TRUE(*c3_reg() == *c1_reg());
  c1_reg()->ResetSealingRoot();
  inst()->Execute(nullptr);
  EXPECT_TRUE(*c3_reg() == *c1_reg());
  c1_reg()->ResetNull();
  inst()->Execute(nullptr);
  EXPECT_TRUE(*c3_reg() == *c1_reg());
}

// Verify that CRepresentableAlignmentMask return the correct mask.
TEST_F(RiscVCheriotInstructionsTest, CRepresentableAlignmentMask) {
  inst()->set_semantic_function(&CheriotCRepresentableAlignmentMask);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  std::array fixed_values{5039028};
  for (int i = 0; i < 1000; i++) {
    uint32_t len;
    if (i < fixed_values.size()) {
      len = fixed_values[i];
    } else {
      len = absl::Uniform(absl::IntervalClosed, bitgen(), 0ULL,
                          std::numeric_limits<uint32_t>::max());
    }
    c1_reg()->set_address(len);
    inst()->Execute(nullptr);
    uint32_t alignment;
    if (len <= 511)
      alignment = 1;
    else if (len <= 1022)
      alignment = 2;
    else if (len <= 2044)
      alignment = 4;
    else if (len <= 4088)
      alignment = 8;
    else if (len <= 8176)
      alignment = 16;
    else if (len <= 16352)
      alignment = 32;
    else if (len <= 32704)
      alignment = 64;
    else if (len <= 65408)
      alignment = 128;
    else if (len <= 130816)
      alignment = 256;
    else if (len <= 261632)
      alignment = 512;
    else if (len <= 523264)
      alignment = 1024;
    else if (len <= 1046528)
      alignment = 2048;
    else if (len <= 2093056)
      alignment = 4096;
    else if (len <= 4186112)
      alignment = 8192;
    else if (len <= 8372224)
      alignment = 16384;
    else
      alignment = 16777216;
    uint32_t mask = ~(alignment - 1);
    EXPECT_EQ(mask, c3_reg()->address())
        << "len: " << len << " exp alignment: " << alignment
        << " alignment: " << (~(c3_reg()->address()) + 1) << std::hex
        << " mask: " << mask << " c3_reg: " << c3_reg()->address();
  }
}

// Verify that round to representable length works properly. The key here is
// that the result of this instruction should be the minimum length >= the
// given length that can be used for exact bounds assuming a suitably aligned
// base address.
TEST_F(RiscVCheriotInstructionsTest, CRoundRepresentableLength) {
  inst()->set_semantic_function(&CheriotCRoundRepresentableLength);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  for (int i = 0; i < 1000; i++) {
    uint32_t len = absl::Uniform(absl::IntervalClosed, bitgen(), 0ULL,
                                 std::numeric_limits<uint32_t>::max());
    c1_reg()->set_address(len);
    inst()->Execute(nullptr);
    uint32_t alignment;
    if (len <= 511)
      alignment = 1;
    else if (len <= 1022)
      alignment = 2;
    else if (len <= 2044)
      alignment = 4;
    else if (len <= 4088)
      alignment = 8;
    else if (len <= 8176)
      alignment = 16;
    else if (len <= 16352)
      alignment = 32;
    else if (len <= 32704)
      alignment = 64;
    else if (len <= 65408)
      alignment = 128;
    else if (len <= 130816)
      alignment = 256;
    else if (len <= 261632)
      alignment = 512;
    else if (len <= 523264)
      alignment = 1024;
    else if (len <= 1046528)
      alignment = 2048;
    else if (len <= 2093056)
      alignment = 4096;
    else if (len <= 4186112)
      alignment = 8192;
    else if (len <= 8372224)
      alignment = 16384;
    else
      alignment = 16777216;
    uint32_t length = alignment * ((len + alignment - 1) / alignment);
    EXPECT_EQ(length, c3_reg()->address());
  }
}

// Check store capability - no traps.
TEST_F(RiscVCheriotInstructionsTest, CSc) {
  inst()->set_semantic_function(&CheriotCSc);
  AppendCapabilityOperands(inst(), {kC1, kC2, kC3}, {});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  c2_reg()->set_address(0x200);
  c3_reg()->ResetSealingRoot();
  c3_reg()->set_address(kDataSeal10);
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken());
  auto *db = state()->db_factory()->Allocate<uint32_t>(2);
  memory()->Load(kMemAddress + 0x200, db, nullptr, nullptr);
  EXPECT_EQ(db->Get<uint32_t>(0), c3_reg()->address());
  EXPECT_EQ(db->Get<uint32_t>(1), c3_reg()->Compress());
  db->DecRef();
}

// Check store capability with invalid capability.
TEST_F(RiscVCheriotInstructionsTest, CScTagViolation) {
  inst()->set_semantic_function(&CheriotCSc);
  AppendCapabilityOperands(inst(), {kC1, kC2, kC3}, {});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  c1_reg()->Invalidate();
  c2_reg()->set_address(0x200);
  c3_reg()->ResetSealingRoot();
  c3_reg()->set_address(kDataSeal10);
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), (kC1Num << 5) | *CH_EC::kCapExTagViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

// Check store capability with sealed capability.
TEST_F(RiscVCheriotInstructionsTest, CScSealViolation) {
  inst()->set_semantic_function(&CheriotCSc);
  AppendCapabilityOperands(inst(), {kC1, kC2, kC3}, {});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  (void)c1_reg()->Seal(*(state()->sealing_root()), kDataSeal10);
  c2_reg()->set_address(0x200);
  c3_reg()->ResetSealingRoot();
  c3_reg()->set_address(kDataSeal10);
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), (kC1Num << 5) | *CH_EC::kCapExSealViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

// Check store capability with no load permission.
TEST_F(RiscVCheriotInstructionsTest, CScPermitStoreViolation) {
  inst()->set_semantic_function(&CheriotCSc);
  AppendCapabilityOperands(inst(), {kC1, kC2, kC3}, {});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  c1_reg()->ClearPermissions(PB::kPermitStore);
  c2_reg()->set_address(0x200);
  c3_reg()->ResetSealingRoot();
  c3_reg()->set_address(kDataSeal10);
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), (kC1Num << 5) | *CH_EC::kCapExPermitStoreViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

// Check store capability with no load store capability permission.
TEST_F(RiscVCheriotInstructionsTest, CScPermitStoreCapViolation) {
  inst()->set_semantic_function(&CheriotCSc);
  AppendCapabilityOperands(inst(), {kC1, kC2, kC3}, {});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  c1_reg()->ClearPermissions(PB::kPermitLoadStoreCapability);
  c2_reg()->set_address(0x200);
  c3_reg()->ResetSealingRoot();
  c3_reg()->set_address(kDataSeal10);
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(),
            (kC1Num << 5) | *CH_EC::kCapExPermitStoreCapabilityViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

// Check for proper generation of store local cap violation.
TEST_F(RiscVCheriotInstructionsTest, CScStoreLocalCapViolation) {
  inst()->set_semantic_function(&CheriotCSc);
  AppendCapabilityOperands(inst(), {kC1, kC2, kC3}, {});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  c1_reg()->ClearPermissions(PB::kPermitStoreLocalCapability);
  c2_reg()->set_address(0x200);
  c3_reg()->ResetMemoryRoot();
  c3_reg()->ClearPermissions(PB::kPermitGlobal);
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken());
  auto *db = state()->db_factory()->Allocate<uint32_t>(2);
  memory()->Load(kMemAddress + 0x200, db, nullptr, nullptr);
  // Invalidate c3 - the stored tag should be the same as c3, but with the tag
  // cleared.
  c3_reg()->Invalidate();
  EXPECT_EQ(db->Get<uint32_t>(0), c3_reg()->address());
  EXPECT_EQ(db->Get<uint32_t>(1), c3_reg()->Compress());
  db->DecRef();
}

// Check store capability with bounds violation.
TEST_F(RiscVCheriotInstructionsTest, CScPermitBoundsViolation) {
  inst()->set_semantic_function(&CheriotCSc);
  AppendCapabilityOperands(inst(), {kC1, kC2, kC3}, {});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  c1_reg()->SetBounds(0, kMemAddress + 0x100);
  c2_reg()->set_address(0x200);
  c3_reg()->ResetSealingRoot();
  c3_reg()->set_address(kDataSeal10);
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), (kC1Num << 5) | *CH_EC::kCapExBoundsViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

// Check store capability with unaligned address.
TEST_F(RiscVCheriotInstructionsTest, CScUnaligned) {
  inst()->set_semantic_function(&CheriotCSc);
  AppendCapabilityOperands(inst(), {kC1, kC2, kC3}, {});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress);
  c2_reg()->set_address(0x201);
  c3_reg()->ResetSealingRoot();
  c3_reg()->set_address(kDataSeal10);
  inst()->Execute(nullptr);
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(), kMemAddress + 0x201);
  EXPECT_EQ(trap_exception_code(), *RV_EC::kStoreAddressMisaligned);
  EXPECT_EQ(inst(), trap_inst());
}

TEST_F(RiscVCheriotInstructionsTest, CSeal) {
  inst()->set_semantic_function(&CheriotCSeal);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  c2_reg()->ResetSealingRoot();
  // Memory sealing.
  c1_reg()->ResetMemoryRoot();
  for (uint32_t o_type = 0; o_type < 18; o_type++) {
    c2_reg()->set_address(o_type);
    inst()->Execute(nullptr);
    EXPECT_EQ(c3_reg()->object_type(), o_type & 0b111);
    // For executable or illegal object types the tag will be false.
    if ((o_type <= 8) || (o_type > 15)) {
      EXPECT_FALSE(c3_reg()->tag());
    } else {
      EXPECT_TRUE(c3_reg()->tag());
    }
  }
  // Executable sealing.
  c1_reg()->ResetExecuteRoot();
  for (uint32_t o_type = 0; o_type < 18; o_type++) {
    c2_reg()->set_address(o_type);
    inst()->Execute(nullptr);
    EXPECT_EQ(c3_reg()->object_type(), o_type & 0b111);
    // For executable object types the tag should be true.
    if ((o_type == OT::kSentry) || (o_type == OT::kInterruptDisablingSentry) ||
        (o_type == OT::kInterruptEnablingSentry) ||
        (o_type == OT::kSealedExecutable6) ||
        (o_type == OT::kSealedExecutable7)) {
      EXPECT_TRUE(c3_reg()->tag());
    } else {
      EXPECT_FALSE(c3_reg()->tag());
    }
  }
  // Sealing type outside range.
  EXPECT_TRUE(c2_reg()->SetBounds(0, 12));
  c2_reg()->set_address(14);
  c1_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->object_type(), 14 & 0b111);
  EXPECT_FALSE(c3_reg()->tag());
  // Attempt sealing using a sealed capability.
  c2_reg()->ResetSealingRoot();
  c2_reg()->set_address(kDataSeal10);
  EXPECT_TRUE(c2_reg()->Seal(*(state()->sealing_root()), kDataSeal10).ok());
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->object_type(), 10 & 0b111);
  EXPECT_FALSE(c3_reg()->tag());
}

TEST_F(RiscVCheriotInstructionsTest, CSetAddr) {
  inst()->set_semantic_function(&CheriotCSetAddr);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  c1_reg()->ResetMemoryRoot();
  EXPECT_EQ(c1_reg()->address(), 0);
  c2_reg()->set_address(kMemAddress);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), kMemAddress);
  EXPECT_TRUE(c3_reg()->tag());
  // If c1 is sealed, the tag is cleared.
  EXPECT_TRUE(c1_reg()->Seal(*(state()->sealing_root()), 9).ok());
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), kMemAddress);
  EXPECT_FALSE(c3_reg()->tag());
  // If address is out of range, the tag is cleared.
  c1_reg()->ResetMemoryRoot();
  c1_reg()->SetBounds(0, 200);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), kMemAddress);
  EXPECT_FALSE(c3_reg()->tag());
}

TEST_F(RiscVCheriotInstructionsTest, CSetBounds) {
  inst()->set_semantic_function(&CheriotCSetBounds);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  c1_reg()->ResetMemoryRoot();
  // Set the requested new base.
  c1_reg()->set_address(kMemAddress);
  for (uint32_t len = 1; len < 0x8000'0000; len <<= 1) {
    // Set the requested new length.
    c2_reg()->set_address(len);
    inst()->Execute(nullptr);
    // The bounds will be no smaller than requested.
    EXPECT_LE(c3_reg()->base(), kMemAddress);
    EXPECT_GE(c3_reg()->length(), len);
    EXPECT_TRUE(c3_reg()->tag());
  }
  // Request bounds outside the capability - first base below.
  c1_reg()->SetBounds(kMemAddress, 0x200);
  c1_reg()->set_address(0);
  c2_reg()->set_address(0x100);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->base(), 0);
  EXPECT_EQ(c3_reg()->length(), 0x100);
  EXPECT_FALSE(c3_reg()->tag());
  // Next, length too long.
  c1_reg()->set_address(kMemAddress);
  c2_reg()->set_address(0x300);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->base(), kMemAddress);
  EXPECT_EQ(c3_reg()->length(), 0x300);
  EXPECT_FALSE(c3_reg()->tag());
  // Base too high.
  c1_reg()->set_address(kMemAddress * 2);
  c2_reg()->set_address(0x100);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->base(), kMemAddress * 2);
  EXPECT_EQ(c3_reg()->length(), 0x100);
  EXPECT_FALSE(c3_reg()->tag());
  // Sealed capability.
  c1_reg()->ResetMemoryRoot();
  EXPECT_TRUE(c1_reg()->Seal(*(state()->sealing_root()), kDataSeal10).ok());
  c1_reg()->set_address(kMemAddress);
  c2_reg()->set_address(0x100);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->base(), kMemAddress);
  EXPECT_EQ(c3_reg()->length(), 0x100);
  EXPECT_FALSE(c3_reg()->tag());
}

TEST_F(RiscVCheriotInstructionsTest, CSetBoundsExact) {
  inst()->set_semantic_function(&CheriotCSetBoundsExact);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  c1_reg()->ResetMemoryRoot();
  // Set the requested new base.
  c1_reg()->set_address(kMemAddress);
  for (uint32_t len = 1; len < 0x8000'0000; len <<= 1) {
    // Set the requested new length.
    c2_reg()->set_address(len);
    inst()->Execute(nullptr);
    // The bounds will be no smaller than requested.
    EXPECT_LE(c3_reg()->base(), kMemAddress);
    EXPECT_GE(c3_reg()->length(), len);
    // If they are not exactly what were requested, the tag will be false.
    if ((c3_reg()->length() != len) || (c3_reg()->base() != kMemAddress)) {
      EXPECT_FALSE(c3_reg()->tag());
    } else {
      EXPECT_TRUE(c3_reg()->tag());
    }
  }
  // Request bounds outside the capability - first base below.
  c1_reg()->SetBounds(kMemAddress, 0x200);
  c1_reg()->set_address(0);
  c2_reg()->set_address(0x100);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->base(), 0);
  EXPECT_EQ(c3_reg()->length(), 0x100);
  EXPECT_FALSE(c3_reg()->tag());
  // Next, length too long.
  c1_reg()->set_address(kMemAddress);
  c2_reg()->set_address(0x300);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->base(), kMemAddress);
  EXPECT_EQ(c3_reg()->length(), 0x300);
  EXPECT_FALSE(c3_reg()->tag());
  // Base too high.
  c1_reg()->set_address(kMemAddress * 2);
  c2_reg()->set_address(0x100);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->base(), kMemAddress * 2);
  EXPECT_EQ(c3_reg()->length(), 0x100);
  EXPECT_FALSE(c3_reg()->tag());
  // Sealed capability.
  c1_reg()->ResetMemoryRoot();
  EXPECT_TRUE(c1_reg()->Seal(*(state()->sealing_root()), kDataSeal10).ok());
  c1_reg()->set_address(kMemAddress);
  c2_reg()->set_address(0x100);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->base(), kMemAddress);
  EXPECT_EQ(c3_reg()->length(), 0x100);
  EXPECT_FALSE(c3_reg()->tag());
}

TEST_F(RiscVCheriotInstructionsTest, CSetEqualExact) {
  inst()->set_semantic_function(&CheriotCSetEqualExact);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c2_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 1);
  // Change c1.
  c1_reg()->ResetExecuteRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 0);
  // Change c2 too.
  c2_reg()->ResetExecuteRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 1);
  // Change c1 to sealing root.
  c1_reg()->ResetSealingRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 0);
  // Change c2 too.
  c2_reg()->ResetSealingRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 1);
}

TEST_F(RiscVCheriotInstructionsTest, CSetHigh) {
  inst()->set_semantic_function(&CheriotCSetHigh);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c1_reg()->set_address(kMemAddress + 10);
  // Initialize another capability register.
  c4_reg()->ResetMemoryRoot();
  c4_reg()->SetBounds(kMemAddress, 200);
  uint32_t high = c4_reg()->Compress();
  c2_reg()->set_address(high);
  inst()->Execute(nullptr);
  // Tag should be cleared.
  EXPECT_FALSE(c3_reg()->tag());
  // Other fields should be the same.
  EXPECT_EQ(c3_reg()->address(), c1_reg()->address());
  EXPECT_EQ(c3_reg()->base(), c4_reg()->base());
  EXPECT_EQ(c3_reg()->length(), c4_reg()->length());
  EXPECT_EQ(c3_reg()->permissions(), c4_reg()->permissions());
  EXPECT_EQ(c3_reg()->object_type(), c4_reg()->object_type());
}

TEST_F(RiscVCheriotInstructionsTest, CSpecialR) {
  inst()->set_semantic_function(&CheriotCSpecialR);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  c1_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken());
  EXPECT_TRUE(*c1_reg() == *c3_reg());
}

TEST_F(RiscVCheriotInstructionsTest, CSpecialRException) {
  inst()->set_semantic_function(&CheriotCSpecialR);
  AppendCapabilityOperands(inst(), {kC1}, {kC3});
  c1_reg()->ResetMemoryRoot();
  // Remove system registers access permission.
  state()->pcc()->ClearPermissions(PB::kPermitAccessSystemRegisters);
  inst()->Execute(nullptr);
  // C3 should be null capability, just like c2 is.
  EXPECT_TRUE(*c3_reg() == *c4_reg());
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(),
            kPccNum << 5 | *CH_EC::kCapExPermitAccessSystemRegistersViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

TEST_F(RiscVCheriotInstructionsTest, CSpecialRW) {
  inst()->set_semantic_function(&CheriotCSpecialRW);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c2_reg()->ResetSealingRoot();
  inst()->Execute(nullptr);
  EXPECT_FALSE(trap_taken());
  EXPECT_TRUE(*state()->sealing_root() == *c3_reg());
  EXPECT_TRUE(*state()->memory_root() == *c2_reg());
}

TEST_F(RiscVCheriotInstructionsTest, CSpecialRWException) {
  inst()->set_semantic_function(&CheriotCSpecialRW);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c2_reg()->ResetSealingRoot();
  // Remove system registers access permission.
  state()->pcc()->ClearPermissions(PB::kPermitAccessSystemRegisters);
  inst()->Execute(nullptr);
  // C3 is unmodified.
  EXPECT_TRUE(*c3_reg() == *c4_reg());
  EXPECT_TRUE(trap_taken());
  EXPECT_FALSE(trap_is_interrupt());
  EXPECT_EQ(trap_epc(), kInstAddress);
  EXPECT_EQ(trap_value(),
            kPccNum << 5 | *CH_EC::kCapExPermitAccessSystemRegistersViolation);
  EXPECT_EQ(trap_exception_code(), CheriotState::kCheriExceptionCode);
  EXPECT_EQ(inst(), trap_inst());
}

TEST_F(RiscVCheriotInstructionsTest, CSub) {
  inst()->set_semantic_function(&CheriotCSub);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  for (int i = 0; i < 100; i++) {
    c3_reg()->ResetMemoryRoot();
    // Generate random address and compressed capability.
    uint32_t val0 = absl::Uniform(absl::IntervalClosed, bitgen(), 0ULL,
                                  std::numeric_limits<uint32_t>::max());
    uint32_t val1 = absl::Uniform(absl::IntervalClosed, bitgen(), 0ULL,
                                  std::numeric_limits<uint32_t>::max());
    c1_reg()->set_address(val0);
    c2_reg()->set_address(val1);
    inst()->Execute(nullptr);
    EXPECT_EQ(c3_reg()->address(), val0 - val1);
    EXPECT_FALSE(c3_reg()->tag());
  }
}

// Tests if cs2 is a subset of cs1.
TEST_F(RiscVCheriotInstructionsTest, CTestSubset) {
  inst()->set_semantic_function(&CheriotCTestSubset);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  c1_reg()->ResetMemoryRoot();
  c2_reg()->ResetMemoryRoot();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 1);
  // Narrow c1 bounds, now the result should be 0.
  c1_reg()->set_address(kMemAddress);
  c1_reg()->SetBounds(kMemAddress, 0x400);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 0);
  // Make c2 bounds narrower than c1, result should be 1.
  c2_reg()->set_address(kMemAddress + 0x100);
  c2_reg()->SetBounds(kMemAddress + 0x100, 0x200);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 1);
  // Remove a permission bit from c1, result should be 0.
  c1_reg()->ClearPermissions(PB::kPermitGlobal);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 0);
  // Remove that and another bit from c2 result should be 1.
  c2_reg()->ClearPermissions(PB::kPermitGlobal | PB::kPermitLoadGlobal);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 1);
  // If c1 is invalidated, then the result is 0.
  c1_reg()->Invalidate();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 0);
  // If c2 is also invalidated, the result is 1.
  c2_reg()->Invalidate();
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->address(), 1);
}

TEST_F(RiscVCheriotInstructionsTest, CUnseal) {
  inst()->set_semantic_function(&CheriotCUnseal);
  AppendCapabilityOperands(inst(), {kC1, kC2}, {kC3});
  c2_reg()->ResetSealingRoot();
  // Set unsealing cap address to 10.
  c2_reg()->set_address(kDataSeal10);
  // If c1 is unsealed, it fails.
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->object_type(), OT::kUnsealed);
  EXPECT_FALSE(c3_reg()->tag());
  EXPECT_EQ(c3_reg()->permissions(), c1_reg()->permissions());
  // Seal c1.
  c1_reg()->ResetMemoryRoot();
  // Seal c1 with otype 10.
  EXPECT_TRUE(c1_reg()->Seal(*(state()->sealing_root()), kDataSeal10).ok());
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->object_type(), OT::kUnsealed);
  EXPECT_TRUE(c3_reg()->tag());
  EXPECT_EQ(c3_reg()->permissions(), c1_reg()->permissions());
  // Remove global permission from c2. The resulting capability will have it
  // removed too.
  c2_reg()->ClearPermissions(PB::kPermitGlobal);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->object_type(), OT::kUnsealed);
  EXPECT_TRUE(c3_reg()->tag());
  EXPECT_NE(c3_reg()->permissions(), c1_reg()->permissions());
  EXPECT_EQ(c3_reg()->permissions() | PB::kPermitGlobal,
            c1_reg()->permissions());
  // Set the wrong unsealing value.
  c2_reg()->ResetSealingRoot();
  c2_reg()->set_address(11);
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->object_type(), OT::kUnsealed);
  EXPECT_FALSE(c3_reg()->tag());
  EXPECT_EQ(c3_reg()->permissions(), c1_reg()->permissions());
  // If c2 is sealed it fails.
  c2_reg()->set_address(kDataSeal10);
  EXPECT_TRUE(c2_reg()->Seal(*(state()->sealing_root()), kDataSeal10).ok());
  inst()->Execute(nullptr);
  EXPECT_EQ(c3_reg()->object_type(), OT::kUnsealed);
  EXPECT_FALSE(c3_reg()->tag());
  EXPECT_EQ(c3_reg()->permissions(), c1_reg()->permissions());
}

}  // namespace
