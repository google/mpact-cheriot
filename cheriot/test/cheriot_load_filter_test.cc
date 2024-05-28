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

#include "cheriot/cheriot_load_filter.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "cheriot/cheriot_register.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/counters.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"
#include "mpact/sim/util/memory/tagged_memory_interface.h"
#include "mpact/sim/util/memory/tagged_memory_watcher.h"

// This file contains the unit tests for CheriotLoadFilter, which traverses
// memory and checks for capabilities that should be revoked.

namespace {

using ::mpact::sim::cheriot::CheriotLoadFilter;
using ::mpact::sim::cheriot::CheriotRegister;
using ::mpact::sim::generic::DataBuffer;
using ::mpact::sim::generic::DataBufferFactory;
using ::mpact::sim::generic::SimpleCounter;
using ::mpact::sim::util::TaggedFlatDemandMemory;
using ::mpact::sim::util::TaggedMemoryInterface;
using ::mpact::sim::util::TaggedMemoryWatcher;

constexpr int kCapSize = CheriotRegister::kCapabilitySizeInBytes;
constexpr int kNumCaps = 8;
// Where the capabilities are located.
constexpr uint64_t kBase = 0x8000'0000;
constexpr uint64_t kTop = 0x8000'0000 + kNumCaps * kCapSize;
// Base of memory addressed by capabilities.
constexpr uint64_t kMemBase = 0x0;
// Base of memory area storing revocation bits.
constexpr uint64_t kRevocationBase = 0x9000'0000;
// Size of increment in the base address of the created capabilities. Each
// capability is given a base address such that cap[i].base == i * kCapBase.
constexpr uint64_t kCapBase = 0x0000'1000;
// Size of each capability, i.e., length().
constexpr int kCapRegionSize = 0x1000;

// Test fixture to provide convenience methods and objects for the test.
class CheriotLoadFilterTest : public ::testing::Test {
 public:
  CheriotLoadFilterTest() : cap_reg_(nullptr, "dummy") {
    db_ = db_factory_.Allocate<uint32_t>(1);
    cap_reg_.SetDataBuffer(db_);
    cap_reg_.ResetNull();
    db_->DecRef();
    memory_ = std::make_unique<TaggedFlatDemandMemory>(
        CheriotRegister::kCapabilitySizeInBytes);
    // Intercept memory accesses for capabilities and revocation memory.
    watcher_ = std::make_unique<TaggedMemoryWatcher>(memory_.get());
    CHECK_OK(watcher_->SetLoadWatchCallback(
        {kBase, kTop},
        [this](uint64_t address, int size) { cap_loads_.push_back(address); }));
    CHECK_OK(watcher_->SetStoreWatchCallback(
        {kBase, kTop}, [this](uint64_t address, int size) {
          cap_stores_.push_back(address);
        }));
    CHECK_OK(watcher_->SetLoadWatchCallback(
        {kRevocationBase, kRevocationBase + 0x1000ULL},
        [this](uint64_t address, int size) {
          revoke_loads_.push_back(address);
        }));
    db_ = db_factory_.Allocate<uint32_t>(2);
    tag_db_ = db_factory_.Allocate<uint8_t>(1);
    counter_.SetIsEnabled(/*is_enabled=*/true);
  }

  ~CheriotLoadFilterTest() override {
    db_->DecRef();
    tag_db_->DecRef();
  }

  // Create a set of capabilities in memory.  A 1 in the mask indicates a valid
  // capability, a 0 means that the capability is invalidated (the tag cleared).
  // The capabilities are set to 0x1000 size region starting at 0x0.
  void CreateMemoryCaps(uint32_t cap_mask) {
    uint64_t address = kBase;
    for (int i = 0; i < kNumCaps; i++) {
      cap_reg_.ResetMemoryRoot();
      cap_reg_.set_address(i * kCapBase);
      cap_reg_.SetBounds(i * kCapBase, kCapRegionSize);
      if (!(cap_mask & (1 << i))) {
        cap_reg_.Invalidate();
      }
      db_->Set<uint32_t>(0, cap_reg_.address());
      db_->Set<uint32_t>(1, cap_reg_.Compress());
      tag_db_->Set<uint8_t>(0, cap_reg_.tag());
      memory_->Store(address, db_, tag_db_);
      address += kCapSize;
    }
  }

  // Revoke the capability with the given base address.
  void Revoke(uint64_t address) {
    uint64_t revoke_address = address - kMemBase;
    uint64_t byte_offset = revoke_address >> 6;
    uint64_t bit_offset = (revoke_address >> 3) & 0b111;
    // Using the tag data buffer only because its size is a byte. It is not
    // loading or storing a tag per se.
    memory_->Load(kRevocationBase + byte_offset, tag_db_, nullptr, nullptr);
    tag_db_->Set<uint8_t>(0, tag_db_->Get<uint8_t>(0) | (1 << bit_offset));
    memory_->Store(kRevocationBase + byte_offset, tag_db_);
  }

  // Check if the capability at the given address is valid or not.
  bool IsValid(uint64_t address) {
    uint64_t cap_address = address & ~0x7ULL;
    memory_->Load(cap_address, nullptr, tag_db_, nullptr, nullptr);
    return tag_db_->Get<uint8_t>(0);
  }

 protected:
  DataBufferFactory db_factory_;
  DataBuffer *db_;
  DataBuffer *tag_db_;
  SimpleCounter<uint64_t> counter_;
  CheriotRegister cap_reg_;
  std::unique_ptr<TaggedMemoryInterface> memory_;
  std::unique_ptr<TaggedMemoryWatcher> watcher_;
  std::vector<uint64_t> cap_loads_;
  std::vector<uint64_t> cap_stores_;
  std::vector<uint64_t> revoke_loads_;
};

// Test the load filter by setting period to 1 and count to 1.
TEST_F(CheriotLoadFilterTest, MemoryLoads_1_1) {
  // Create 8 valid capabilities.
  CreateMemoryCaps(0xff);
  auto load_filter = std::make_unique<CheriotLoadFilter>(
      watcher_.get(), /*period=*/1, /*count=*/1, kBase, kTop, /*cap_base=*/0,
      kRevocationBase);
  counter_.AddListener(load_filter.get());
  // Increment the counter 8 times. This should lead to 8 loads and 8 revocation
  // loads.
  for (int i = 1; i <= kNumCaps; i++) {
    counter_.Increment(1);
    EXPECT_EQ(cap_loads_.size(), i);
    EXPECT_EQ(cap_stores_.size(), 0);
    EXPECT_EQ(revoke_loads_.size(), i);
  }
  cap_loads_.clear();
  cap_stores_.clear();
  revoke_loads_.clear();
  // Create 4 valid caps interleaved with 4 invalid caps.
  CreateMemoryCaps(0b1010'1010);
  // Increment the counter 8 times. This should lead to 8 loads, but only 4
  // revocation loads, since every other capability is invalid.
  for (int i = 1; i <= kNumCaps; i++) {
    counter_.Increment(1);
    EXPECT_EQ(cap_loads_.size(), i);
    EXPECT_EQ(cap_stores_.size(), 0);
    EXPECT_EQ(revoke_loads_.size(), i / 2);
  }
}

// Tests the load filter by setting period to 2 and count to 1. This means
// that 1 capability is checked every two increments.
TEST_F(CheriotLoadFilterTest, MemoryLoads_2_1) {
  // Create 8 valid capabilities.
  CreateMemoryCaps(0xff);
  auto load_filter = std::make_unique<CheriotLoadFilter>(
      watcher_.get(), /*period=*/2, /*count=*/1, kBase, kTop, /*cap_base=*/0,
      kRevocationBase);
  counter_.AddListener(load_filter.get());
  // Iterate 16 times, since the period is twice as long.
  for (int i = 1; i <= kNumCaps * 2; i++) {
    counter_.Increment(1);
    EXPECT_EQ(cap_loads_.size(), i / 2) << i;
    EXPECT_EQ(cap_stores_.size(), 0);
    EXPECT_EQ(revoke_loads_.size(), i / 2) << i;
  }
  cap_loads_.clear();
  cap_stores_.clear();
  revoke_loads_.clear();
  // Create 4 valid caps interleaved with 4 invalid caps.
  CreateMemoryCaps(0b1010'1010);
  for (int i = 1; i <= kNumCaps * 2; i++) {
    counter_.Increment(1);
    EXPECT_EQ(cap_loads_.size(), i / 2) << i;
    EXPECT_EQ(cap_stores_.size(), 0);
    EXPECT_EQ(revoke_loads_.size(), i / 4) << i;
  }
}

// Test the load filter by setting period to 1 and count to 2. This means that
// 2 caps are checked every increment.
TEST_F(CheriotLoadFilterTest, MemoryLoads_1_2) {
  // Create 8 valid capabilities.
  CreateMemoryCaps(0xff);
  auto load_filter = std::make_unique<CheriotLoadFilter>(
      watcher_.get(), /*period=*/1, /*count=*/2, kBase, kTop, /*cap_base=*/0,
      kRevocationBase);
  counter_.AddListener(load_filter.get());
  // Iterate 4 times, since 2 caps are processed in each period.
  for (int i = 1; i <= kNumCaps / 2; i++) {
    counter_.Increment(1);
    EXPECT_EQ(cap_loads_.size(), 2 * i) << i;
    EXPECT_EQ(cap_stores_.size(), 0);
    EXPECT_EQ(revoke_loads_.size(), 2 * i) << i;
  }
  cap_loads_.clear();
  cap_stores_.clear();
  revoke_loads_.clear();
  // Create 4 valid caps interleaved with 4 invalid caps.
  CreateMemoryCaps(0b1010'1010);
  for (int i = 1; i <= kNumCaps / 2; i++) {
    counter_.Increment(1);
    EXPECT_EQ(cap_loads_.size(), 2 * i) << i;
    EXPECT_EQ(cap_stores_.size(), 0);
    EXPECT_EQ(revoke_loads_.size(), i) << i;
  }
}

TEST_F(CheriotLoadFilterTest, FilterTest) {
  // Create 8 valid capabilities.
  CreateMemoryCaps(0xff);
  // The load filter is set to filter all 8 capabilities every increment.
  auto load_filter = std::make_unique<CheriotLoadFilter>(
      watcher_.get(), /*period=*/1, /*count=*/8, kBase, kTop, /*cap_base=*/0,
      kRevocationBase);
  counter_.AddListener(load_filter.get());
  // In this loop, revoke one capability per iteration. Then filter all the
  // capabilities. Only one should be made invalid after each iteration.
  for (int i = 0; i < kNumCaps; i++) {
    Revoke(i * kCapBase);
    counter_.Increment(1);
    EXPECT_EQ(cap_loads_.size(), 8 * (i + 1)) << i;
    EXPECT_EQ(cap_stores_.size(), i + 1) << i;
    EXPECT_EQ(revoke_loads_.size(), 8 - i) << i;
    revoke_loads_.clear();
    // Check validity of the capabilities. Only one capability should be
    // invalidated for every increment.
    for (int j = 0; j < kNumCaps; j++) {
      EXPECT_EQ(IsValid(kBase + j * kCapSize), j > i) << absl::StrCat("j: ", j);
    }
  }
}

}  // namespace
