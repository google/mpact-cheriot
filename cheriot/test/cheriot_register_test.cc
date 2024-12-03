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

#include "cheriot/cheriot_register.h"

#include <cstdint>
#include <limits>
#include <memory>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/arch_state.h"
#include "mpact/sim/generic/operand_interface.h"

// This file contains tests for the 32 bit RiscVCapabilityRegisters (CHERIoT).

namespace {

using ::mpact::sim::cheriot::CheriotRegister;
using ::mpact::sim::generic::ArchState;
using ::mpact::sim::generic::SourceOperandInterface;

using ObjectType = CheriotRegister::ObjectType;
using PermissionBits = CheriotRegister::PermissionBits;

static constexpr uint64_t kBase = 0x1'0011;

class MockArchState : public ArchState {
 public:
  MockArchState(absl::string_view id, SourceOperandInterface* pc_op)
      : ArchState(id, pc_op) {}
  explicit MockArchState(absl::string_view id) : MockArchState(id, nullptr) {}
};

// Test fixture.
class CheriotRegisterTest : public ::testing::Test {
 protected:
  CheriotRegisterTest() {
    arch_state_ = new MockArchState("test");
    cap_reg_ = new CheriotRegister(arch_state_, "test");
  }

  ~CheriotRegisterTest() override {
    delete cap_reg_;
    delete arch_state_;
  }

  CheriotRegister* cap_reg() { return cap_reg_; }

  absl::BitGen bitgen_;
  MockArchState* arch_state_;
  CheriotRegister* cap_reg_;
};

// Verify Reset().
TEST_F(CheriotRegisterTest, Reset) {
  // Register value should be 0 on reset.
  EXPECT_EQ(cap_reg()->data_buffer()->Get<uint32_t>(0), 0);
  // The capability should be the null capability.
  EXPECT_FALSE(cap_reg()->tag());
  EXPECT_EQ(cap_reg()->base(), 0);
  EXPECT_EQ(cap_reg()->length(), 0);
  EXPECT_EQ(cap_reg()->object_type(), ObjectType::kUnsealed);
  EXPECT_EQ(cap_reg()->permissions(), 0);
  EXPECT_FALSE(cap_reg()->IsValid());
  EXPECT_FALSE(cap_reg()->IsUnsealed());
  EXPECT_FALSE(cap_reg()->IsSealed());
  // Update values, then reset, then re-verify.
  cap_reg()->ResetMemoryRoot();
  (void)cap_reg()->SetBounds(0xabcd'0000, 0x10'0000);
  cap_reg()->set_object_type(ObjectType::kUnsealed);
  cap_reg()->ResetNull();
  // The capability should be the null capability.
  EXPECT_FALSE(cap_reg()->tag());
  EXPECT_EQ(cap_reg()->base(), 0);
  EXPECT_EQ(cap_reg()->length(), 0);
  EXPECT_EQ(cap_reg()->object_type(), ObjectType::kUnsealed);
  EXPECT_EQ(cap_reg()->permissions(), 0);
  EXPECT_FALSE(cap_reg()->IsValid());
  EXPECT_FALSE(cap_reg()->IsUnsealed());
  EXPECT_FALSE(cap_reg()->IsSealed());
}

// Verify ResetRoot to see that the capability becomes a memory root capability.
TEST_F(CheriotRegisterTest, ResetMemoryRoot) {
  // The capability is null at first.
  cap_reg()->ResetMemoryRoot();
  // Verify that it is a root capability.
  EXPECT_TRUE(cap_reg()->tag());
  EXPECT_EQ(cap_reg()->base(), 0);
  EXPECT_EQ(cap_reg()->length(), 0x1'0000'0000ULL);
  EXPECT_EQ(cap_reg()->object_type(), ObjectType::kUnsealed);
  EXPECT_EQ(
      cap_reg()->permissions(),
      (PermissionBits::kPermitGlobal | PermissionBits::kPermitLoad |
       PermissionBits::kPermitStore |
       PermissionBits::kPermitLoadStoreCapability |
       PermissionBits::kPermitStoreLocalCapability |
       PermissionBits::kPermitLoadGlobal | PermissionBits::kPermitLoadMutable));
  EXPECT_EQ(cap_reg()->data_buffer()->Get<uint32_t>(0), 0);
  EXPECT_TRUE(cap_reg()->IsValid());
  EXPECT_TRUE(cap_reg()->IsUnsealed());
  EXPECT_FALSE(cap_reg()->IsSealed());
}

// Verify ResetRoot to see that the capability becomes a memory root capability.
TEST_F(CheriotRegisterTest, ResetExecuteRoot) {
  // The capability is null at first.
  cap_reg()->ResetExecuteRoot();
  // Verify that it is a root capability.
  EXPECT_TRUE(cap_reg()->tag());
  EXPECT_EQ(cap_reg()->base(), 0);
  EXPECT_EQ(cap_reg()->length(), 0x1'0000'0000ULL);
  EXPECT_EQ(cap_reg()->object_type(), ObjectType::kUnsealed);
  EXPECT_EQ(
      cap_reg()->permissions(),
      (PermissionBits::kPermitGlobal | PermissionBits::kPermitExecute |
       PermissionBits::kPermitLoad |
       PermissionBits::kPermitLoadStoreCapability |
       PermissionBits::kPermitLoadGlobal | PermissionBits::kPermitLoadMutable |
       PermissionBits::kPermitAccessSystemRegisters));
  EXPECT_EQ(cap_reg()->data_buffer()->Get<uint32_t>(0), 0);
  EXPECT_TRUE(cap_reg()->IsValid());
  EXPECT_TRUE(cap_reg()->IsUnsealed());
  EXPECT_FALSE(cap_reg()->IsSealed());
}

// Verify ResetRoot to see that the capability becomes a memory root capability.
TEST_F(CheriotRegisterTest, ResetSealingRoot) {
  // The capability is null at first.
  cap_reg()->ResetSealingRoot();
  // Verify that it is a root capability.
  EXPECT_TRUE(cap_reg()->tag());
  EXPECT_EQ(cap_reg()->base(), 0);
  EXPECT_EQ(cap_reg()->length(), 0x1'0000'0000ULL);
  EXPECT_EQ(cap_reg()->object_type(), ObjectType::kUnsealed);
  EXPECT_EQ(cap_reg()->permissions(),
            (PermissionBits::kPermitGlobal | PermissionBits::kPermitSeal |
             PermissionBits::kPermitUnseal | PermissionBits::kUserPerm0));
  EXPECT_EQ(cap_reg()->data_buffer()->Get<uint32_t>(0), 0);
  EXPECT_TRUE(cap_reg()->IsValid());
  EXPECT_TRUE(cap_reg()->IsUnsealed());
}

TEST_F(CheriotRegisterTest, CompressNull) {
  // The initial value of the capability is null. Verify that it matches the
  // compressed, and memory compressed values.
  EXPECT_EQ(cap_reg()->Compress(), CheriotRegister::kNullCapability);
}

TEST_F(CheriotRegisterTest, SetBounds) {
  for (uint32_t length_exp = 0; length_exp <= 32; length_exp++) {
    cap_reg()->ResetMemoryRoot();
    uint64_t length = 1ULL << length_exp;
    uint64_t top = kBase + length;
    if (top > 0x1'0000'0000ULL) {
      length = 0x1'0000'0000ULL - kBase;
    }
    bool is_exact = cap_reg()->SetBounds(kBase, length);
    // The bounds are exact if the length exponent is < 10 for the given base.
    EXPECT_EQ(is_exact, length_exp < 9) << length_exp;
    if (is_exact) {
      EXPECT_EQ(length, cap_reg()->length());
      EXPECT_EQ(kBase, cap_reg()->base());
    } else {
      EXPECT_LE(length, cap_reg()->length());
      EXPECT_GE(kBase, cap_reg()->base());
    }
  }
}

TEST_F(CheriotRegisterTest, ClearPermissions) {
  // Memory root permissions.
  cap_reg()->ResetMemoryRoot();
  // Remove permission bits in order according to state transition diagram
  // in section 7.13 of the CherIoT documentation.
  for (uint32_t i :
       {PermissionBits::kPermitGlobal,
        PermissionBits::kPermitStoreLocalCapability,
        PermissionBits::kPermitLoadMutable, PermissionBits::kPermitLoadGlobal,
        PermissionBits::kPermitLoadStoreCapability, PermissionBits::kPermitLoad,
        PermissionBits::kPermitStore}) {
    auto permissions = cap_reg()->permissions();
    cap_reg()->ClearPermissions(i);
    auto new_permissions = cap_reg()->permissions();
    auto diff = new_permissions ^ permissions;
    if (permissions & i) {
      EXPECT_EQ(diff, i);
    } else {
      EXPECT_EQ(diff, 0);
    }
  }
  // Execute root permissions.
  cap_reg()->ResetExecuteRoot();
  for (uint32_t i : {
           PermissionBits::kPermitGlobal,
           PermissionBits::kPermitAccessSystemRegisters,
           PermissionBits::kPermitLoadGlobal,
           PermissionBits::kPermitLoadMutable,
           PermissionBits::kPermitExecute,
           PermissionBits::kPermitLoadStoreCapability,
           PermissionBits::kPermitLoad,
       }) {
    auto permissions = cap_reg()->permissions();
    cap_reg()->ClearPermissions(i);
    auto new_permissions = cap_reg()->permissions();
    auto diff = new_permissions ^ permissions;
    if (permissions & i) {
      EXPECT_EQ(diff, i);
    } else {
      EXPECT_EQ(diff, 0);
    }
  }
  // Sealing root permissions.
  cap_reg()->ResetSealingRoot();
  for (uint32_t i = PermissionBits::kPermitGlobal;
       i <= PermissionBits::kUserPerm0; i <<= 1) {
    auto permissions = cap_reg()->permissions();
    cap_reg()->ClearPermissions(i);
    auto new_permissions = cap_reg()->permissions();
    auto diff = new_permissions ^ permissions;
    if (permissions & i) {
      EXPECT_EQ(diff, i);
    } else {
      EXPECT_EQ(diff, 0);
    }
  }
}

TEST_F(CheriotRegisterTest, Invalidate) {
  cap_reg()->ResetNull();
  EXPECT_FALSE(cap_reg()->IsValid());
  cap_reg()->ResetExecuteRoot();
  EXPECT_TRUE(cap_reg()->IsValid());
  cap_reg()->Invalidate();
  EXPECT_FALSE(cap_reg()->IsValid());
  cap_reg()->ResetMemoryRoot();
  EXPECT_TRUE(cap_reg()->IsValid());
  cap_reg()->Invalidate();
  EXPECT_FALSE(cap_reg()->IsValid());
  cap_reg()->ResetSealingRoot();
  EXPECT_TRUE(cap_reg()->IsValid());
  cap_reg()->Invalidate();
  EXPECT_FALSE(cap_reg()->IsValid());
}

TEST_F(CheriotRegisterTest, SealDataCapabilities) {
  // Create a sealing capability.
  auto seal_cap_reg = std::make_unique<CheriotRegister>(arch_state_, "seal");
  seal_cap_reg->ResetSealingRoot();
  // Try sealing with different object types.
  for (uint32_t i = ObjectType::kUnsealed; i <= 16; i++) {
    // Set cap_reg top be a memory root capability.
    cap_reg()->ResetMemoryRoot();
    auto status = cap_reg()->Seal(*seal_cap_reg, i);
    // Check to see if i is one of the correct object types, and check the
    // status accordingly.
    if ((i >= 9) && (i <= 15)) {
      EXPECT_TRUE(status.ok()) << status.message();
      EXPECT_TRUE(cap_reg()->IsSealed())
          << i << ": " << cap_reg()->tag() << " " << cap_reg()->object_type();
      EXPECT_FALSE(cap_reg()->IsUnsealed())
          << i << ": " << cap_reg()->tag() << " " << cap_reg()->object_type();
    } else {
      EXPECT_FALSE(cap_reg()->IsSealed())
          << i << ": " << cap_reg()->tag() << " " << cap_reg()->object_type();
      EXPECT_TRUE(!cap_reg()->tag() || cap_reg()->IsUnsealed())
          << i << ": " << cap_reg()->tag() << " " << cap_reg()->object_type();
    }
  }
  // Change bounds of sealing capability to > than valid object types.
  (void)seal_cap_reg->SetBounds(0x100, 0x100);
  cap_reg()->ResetMemoryRoot();
  auto status = cap_reg()->Seal(*seal_cap_reg, 9);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), testing::HasSubstr("out of range"))
      << status.message();
  // Try with a sealing root with cleared tag.
  seal_cap_reg->ResetSealingRoot();
  seal_cap_reg->Invalidate();
  EXPECT_FALSE(cap_reg()->Seal(*seal_cap_reg, 9).ok());

  cap_reg()->ResetMemoryRoot();
  seal_cap_reg->ResetSealingRoot();
  // Seal the sealing capability. This should succeed.
  CHECK_OK(seal_cap_reg->Seal(*seal_cap_reg, 10));
  // Now try to seal using the sealed sealing capability. That should fail.
  status = cap_reg()->Seal(*seal_cap_reg, 10);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(),
              testing::HasSubstr("Cannot seal using a sealed capability"))
      << status.message();
  // Try to use a capability without sealing permission.
  seal_cap_reg->ResetSealingRoot();
  seal_cap_reg->ClearPermissions(PermissionBits::kPermitSeal);
  cap_reg()->ResetMemoryRoot();
  status = cap_reg()->Seal(*seal_cap_reg, 10);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied);
  EXPECT_THAT(status.message(),
              testing::HasSubstr("Missing sealing permission"))
      << status.message();
  // Try sealing a null capability.
  seal_cap_reg->ResetSealingRoot();
  cap_reg()->ResetNull();
  status = cap_reg()->Seal(*seal_cap_reg, 10);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(),
              testing::HasSubstr("Target is not a valid capability"));
  // Try sealing twice.
  cap_reg()->ResetMemoryRoot();
  CHECK_OK(cap_reg()->Seal(*seal_cap_reg, 10));
  status = cap_reg()->Seal(*seal_cap_reg, 10);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(),
              testing::HasSubstr("Cannot seal already sealed capability"))
      << status.message();
}

TEST_F(CheriotRegisterTest, SealExecuteCapability) {
  // Create a sealing capability.
  auto seal_cap_reg = std::make_unique<CheriotRegister>(arch_state_, "seal");
  seal_cap_reg->ResetSealingRoot();
  // Try sealing with different object types.
  for (uint32_t i = ObjectType::kUnsealed; i <= 16; i++) {
    // Set cap_reg top be a memory root capability.
    cap_reg()->ResetExecuteRoot();
    auto status = cap_reg()->Seal(*seal_cap_reg, i);
    // If the object type is out of range for data, expect failure.
    if ((i == ObjectType::kInterruptInheritingSentry) ||
        (i == ObjectType::kInterruptDisablingForwardSentry) ||
        (i == ObjectType::kInterruptEnablingForwardSentry) ||
        (i == ObjectType::kInterruptDisablingBackwardSentry) ||
        (i == ObjectType::kInterruptEnablingBackwardSentry) ||
        (i == ObjectType::kSealedExecutable6) ||
        (i == ObjectType::kSealedExecutable7)) {
      EXPECT_TRUE(status.ok()) << status.message();
      EXPECT_TRUE(cap_reg()->IsSealed());
      EXPECT_FALSE(cap_reg()->IsUnsealed());
    } else {
      EXPECT_FALSE(status.ok());
      EXPECT_FALSE(cap_reg()->IsSealed());
    }
  }
  // Change bounds of sealing capability to > than valid object types.
  (void)seal_cap_reg->SetBounds(0x100, 0x1000);
  cap_reg()->ResetExecuteRoot();
  auto status = cap_reg()->Seal(*seal_cap_reg, ObjectType::kSealedExecutable6);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(
      status.message(),
      testing::HasSubstr("Sealing capability is not a valid capability"));
  // Try with a sealing root with cleared tag.
  seal_cap_reg->ResetSealingRoot();
  seal_cap_reg->Invalidate();
  EXPECT_FALSE(cap_reg()
                   ->Seal(*seal_cap_reg, ObjectType::kInterruptInheritingSentry)
                   .ok());
  cap_reg()->ResetExecuteRoot();
  seal_cap_reg->ResetSealingRoot();
  // Seal the sealing capability. This should succeed.
  CHECK_OK(seal_cap_reg->Seal(*seal_cap_reg, 10));
  // Now try to seal using the sealed sealing capability. That should fail.
  status = cap_reg()->Seal(*seal_cap_reg, ObjectType::kSealedExecutable6);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(),
              testing::HasSubstr("Cannot seal using a sealed capability"));
  // Try to use a capability without sealing permission.
  seal_cap_reg->ResetSealingRoot();
  seal_cap_reg->ClearPermissions(PermissionBits::kPermitSeal);
  cap_reg()->ResetExecuteRoot();
  status =
      cap_reg()->Seal(*seal_cap_reg, ObjectType::kInterruptInheritingSentry);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied);
  EXPECT_THAT(status.message(),
              testing::HasSubstr("Missing sealing permission"));
  // Try sealing a null capability.
  seal_cap_reg->ResetSealingRoot();
  cap_reg()->ResetNull();
  status =
      cap_reg()->Seal(*seal_cap_reg, ObjectType::kInterruptInheritingSentry);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(),
              testing::HasSubstr("Target is not a valid capability"));
  // Try sealing twice.
  cap_reg()->ResetExecuteRoot();
  CHECK_OK(
      cap_reg()->Seal(*seal_cap_reg, ObjectType::kInterruptInheritingSentry));
  status =
      cap_reg()->Seal(*seal_cap_reg, ObjectType::kInterruptInheritingSentry);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(),
              testing::HasSubstr("Cannot seal already sealed capability"));
}

TEST_F(CheriotRegisterTest, CopyFrom) {
  constexpr uint32_t kAddress = 0xdeadbeef;
  auto cap_reg_copy = std::make_unique<CheriotRegister>(arch_state_, "copy");
  // Copy from null capability.
  cap_reg()->ResetNull();
  cap_reg()->data_buffer()->Set<uint32_t>(0, kAddress);
  cap_reg()->set_object_type(ObjectType::kReserved8);
  cap_reg()->set_reserved(1);
  cap_reg_copy->CopyFrom(*cap_reg());
  EXPECT_FALSE(cap_reg_copy->IsValid());
  EXPECT_EQ(cap_reg_copy->data_buffer()->Get<uint32_t>(0), kAddress);
  EXPECT_EQ(cap_reg_copy->tag(), cap_reg()->tag());
  EXPECT_EQ(cap_reg_copy->top(), cap_reg()->top());
  EXPECT_EQ(cap_reg_copy->base(), cap_reg()->base());
  EXPECT_EQ(cap_reg_copy->length(), cap_reg()->length());
  EXPECT_EQ(cap_reg_copy->permissions(), cap_reg()->permissions());
  EXPECT_EQ(cap_reg_copy->object_type(), cap_reg()->object_type());
  EXPECT_EQ(cap_reg_copy->reserved(), cap_reg()->reserved());
  // Copy from memory root capability.
  cap_reg()->ResetMemoryRoot();
  cap_reg()->data_buffer()->Set<uint32_t>(0, kAddress);
  cap_reg()->set_object_type(ObjectType::kReserved8);
  cap_reg()->set_reserved(1);
  (void)cap_reg()->SetBounds(kBase, kAddress + 1);
  cap_reg_copy->CopyFrom(*cap_reg());
  EXPECT_TRUE(cap_reg_copy->IsValid());
  EXPECT_EQ(cap_reg_copy->data_buffer()->Get<uint32_t>(0), kAddress);
  EXPECT_EQ(cap_reg_copy->tag(), cap_reg()->tag());
  EXPECT_EQ(cap_reg_copy->top(), cap_reg()->top());
  EXPECT_EQ(cap_reg_copy->base(), cap_reg()->base());
  EXPECT_EQ(cap_reg_copy->length(), cap_reg()->length());
  EXPECT_EQ(cap_reg_copy->permissions(), cap_reg()->permissions());
  EXPECT_EQ(cap_reg_copy->object_type(), cap_reg()->object_type());
  // Copy from execute root capability.
  cap_reg()->ResetExecuteRoot();
  cap_reg()->data_buffer()->Set<uint32_t>(0, kAddress);
  cap_reg()->set_object_type(ObjectType::kReserved8);
  cap_reg()->set_reserved(1);
  (void)cap_reg()->SetBounds(kBase, kAddress + 1);
  cap_reg_copy->CopyFrom(*cap_reg());
  EXPECT_TRUE(cap_reg_copy->IsValid());
  EXPECT_EQ(cap_reg_copy->data_buffer()->Get<uint32_t>(0), kAddress);
  EXPECT_EQ(cap_reg_copy->tag(), cap_reg()->tag());
  EXPECT_EQ(cap_reg_copy->top(), cap_reg()->top());
  EXPECT_EQ(cap_reg_copy->base(), cap_reg()->base());
  EXPECT_EQ(cap_reg_copy->length(), cap_reg()->length());
  EXPECT_EQ(cap_reg_copy->permissions(), cap_reg()->permissions());
  EXPECT_EQ(cap_reg_copy->object_type(), cap_reg()->object_type());
  // Copy from sealing root capability.
  cap_reg()->ResetSealingRoot();
  cap_reg()->data_buffer()->Set<uint32_t>(0, kAddress);
  cap_reg()->set_object_type(ObjectType::kReserved8);
  cap_reg()->set_reserved(1);
  (void)cap_reg()->SetBounds(kBase, kAddress + 1);
  cap_reg_copy->CopyFrom(*cap_reg());
  EXPECT_TRUE(cap_reg_copy->IsValid());
  EXPECT_EQ(cap_reg_copy->data_buffer()->Get<uint32_t>(0), kAddress);
  EXPECT_EQ(cap_reg_copy->tag(), cap_reg()->tag());
  EXPECT_EQ(cap_reg_copy->top(), cap_reg()->top());
  EXPECT_EQ(cap_reg_copy->base(), cap_reg()->base());
  EXPECT_EQ(cap_reg_copy->length(), cap_reg()->length());
  EXPECT_EQ(cap_reg_copy->permissions(), cap_reg()->permissions());
  EXPECT_EQ(cap_reg_copy->object_type(), cap_reg()->object_type());
}

// Test compress/expand of bounds.
TEST_F(CheriotRegisterTest, CompressExpand) {
  // First some random combinations.
  for (int i = 0; i < 1000; i++) {
    cap_reg()->ResetMemoryRoot();
    // Generate random address and compressed capability.
    uint32_t address = absl::Uniform(absl::IntervalClosed, bitgen_, 0ULL,
                                     std::numeric_limits<uint32_t>::max());
    uint32_t compressed = absl::Uniform(absl::IntervalClosed, bitgen_, 0ULL,
                                        std::numeric_limits<uint32_t>::max());
    // Expand the capability, get the base and top, then compress it again.
    cap_reg()->Expand(address, compressed, true);
    uint32_t base = cap_reg()->base();
    uint64_t top = cap_reg()->top();
    uint32_t re_compressed = cap_reg()->Compress();
    // The starting compressed value should be the same as the re-compressed.
    EXPECT_EQ(re_compressed, compressed) << absl::StrCat(
        "address: ", absl::Hex(address, absl::kZeroPad8),
        " compressed: ", absl::Hex(compressed, absl::kZeroPad8),
        " re-compressed: ", absl::Hex(re_compressed, absl::kZeroPad8));
    cap_reg()->ResetMemoryRoot();
    // Expand the re-compressed capability. The base and top should be the same.
    cap_reg()->Expand(address, re_compressed, true);
    EXPECT_EQ(base, cap_reg()->base()) << absl::StrCat(
        "address: ", absl::Hex(address, absl::kZeroPad8),
        " compressed: ", absl::Hex(compressed, absl::kZeroPad8),
        " re-compressed: ", absl::Hex(re_compressed, absl::kZeroPad8));
    EXPECT_EQ(top, cap_reg()->top()) << absl::StrCat(
        "address: ", absl::Hex(address, absl::kZeroPad8),
        " compressed: ", absl::Hex(compressed, absl::kZeroPad8),
        " re-compressed: ", absl::Hex(re_compressed, absl::kZeroPad8));
  }
  for (uint32_t length_exp = 0; length_exp <= 32; length_exp++) {
    cap_reg()->ResetMemoryRoot();
    uint64_t length = 1ULL << length_exp;
    cap_reg()->data_buffer()->Set<uint32_t>(0, kBase);
    uint64_t top = kBase + length;
    if (top > 0x1'0000'0000) {
      length = 0x1'0000'0000 - kBase;
    }
    // Set bounds.
    (void)cap_reg()->SetBounds(kBase, length);
    // Get the base, top, and length.
    uint32_t cap_base = cap_reg()->base();
    uint64_t cap_top = cap_reg()->top();
    uint64_t cap_length = cap_reg()->length();
    // Compress the capability.
    uint32_t compressed = cap_reg()->Compress();
    // Expand the capability. Make sure the base, top, and length are the same.
    cap_reg()->Expand(kBase, compressed, true);
    EXPECT_TRUE(cap_reg()->IsValid())
        << absl::StrCat("length_exp: ", length_exp);
    EXPECT_EQ(cap_reg()->base(), cap_base)
        << absl::StrCat(length_exp, " base: ", absl::Hex(cap_reg()->base()),
                        " cap_base: ", absl::Hex(cap_base));
    EXPECT_EQ(cap_reg()->top(), cap_top)
        << absl::StrCat(length_exp, " top: ", absl::Hex(cap_reg()->top()),
                        " cap_top: ", absl::Hex(cap_top));
    EXPECT_EQ(cap_reg()->length(), cap_length)
        << absl::StrCat(length_exp, " length: ", absl::Hex(cap_reg()->length()),
                        " cap_length: ", absl::Hex(cap_length));
  }
}

}  // namespace
