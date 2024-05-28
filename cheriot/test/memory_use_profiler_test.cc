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

#include "cheriot/memory_use_profiler.h"

#include <cstdint>
#include <iostream>

#include "absl/strings/str_format.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/data_buffer.h"

namespace {

using ::mpact::sim::cheriot::MemoryUseProfiler;
using ::mpact::sim::cheriot::internal::MemoryUseTracker;
using ::mpact::sim::generic::DataBuffer;
using ::mpact::sim::generic::DataBufferFactory;

constexpr uint64_t kMemoryBase = 0x1234'5678;

class MemoryUseProfilerTest : public ::testing::Test {
 public:
  MemoryUseProfilerTest() {
    db1_ = db_factory_.Allocate<uint8_t>(1);
    db2_ = db_factory_.Allocate<uint16_t>(1);
    db4_ = db_factory_.Allocate<uint32_t>(1);
    db8_ = db_factory_.Allocate<uint64_t>(1);
    profiler_.set_is_enabled(true);
  }

  ~MemoryUseProfilerTest() override {
    db1_->DecRef();
    db2_->DecRef();
    db4_->DecRef();
    db8_->DecRef();
  }

  MemoryUseProfiler profiler_;
  DataBufferFactory db_factory_;
  DataBuffer *db1_;
  DataBuffer *db2_;
  DataBuffer *db4_;
  DataBuffer *db8_;
};

// If no references are captured, then there shouldn't be any output.
TEST_F(MemoryUseProfilerTest, NoReferences) {
  testing::internal::CaptureStdout();
  profiler_.WriteProfile(std::cout);
  auto output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(output, "");
}

// Test single memory references.
TEST_F(MemoryUseProfilerTest, SingleByteLoad) {
  testing::internal::CaptureStdout();
  profiler_.WriteProfile(std::cout);
  profiler_.Load(kMemoryBase, db1_, nullptr, nullptr);
  profiler_.WriteProfile(std::cout);
  auto output = testing::internal::GetCapturedStdout();
  EXPECT_THAT(output, testing::HasSubstr(absl::StrFormat(
                          "0x%llx,0x%llx", kMemoryBase, kMemoryBase)))
      << output;
}

TEST_F(MemoryUseProfilerTest, SingleHalfLoad) {
  testing::internal::CaptureStdout();
  profiler_.WriteProfile(std::cout);
  profiler_.Load(kMemoryBase, db2_, nullptr, nullptr);
  profiler_.WriteProfile(std::cout);
  auto output = testing::internal::GetCapturedStdout();
  EXPECT_THAT(output, testing::HasSubstr(absl::StrFormat(
                          "0x%llx,0x%llx", kMemoryBase, kMemoryBase)))
      << output;
}

TEST_F(MemoryUseProfilerTest, SingleWordLoad) {
  testing::internal::CaptureStdout();
  profiler_.WriteProfile(std::cout);
  profiler_.Load(kMemoryBase, db4_, nullptr, nullptr);
  profiler_.WriteProfile(std::cout);
  auto output = testing::internal::GetCapturedStdout();
  EXPECT_THAT(output, testing::HasSubstr(absl::StrFormat(
                          "0x%llx,0x%llx", kMemoryBase, kMemoryBase)))
      << output;
}

TEST_F(MemoryUseProfilerTest, SingleDoubleLoad) {
  testing::internal::CaptureStdout();
  profiler_.WriteProfile(std::cout);
  profiler_.Load(kMemoryBase, db8_, nullptr, nullptr);
  profiler_.WriteProfile(std::cout);
  auto output = testing::internal::GetCapturedStdout();
  EXPECT_THAT(output, testing::HasSubstr(absl::StrFormat(
                          "0x%llx,0x%llx", kMemoryBase, kMemoryBase + 4)))
      << output;
}

TEST_F(MemoryUseProfilerTest, SingleByteStore) {
  testing::internal::CaptureStdout();
  profiler_.WriteProfile(std::cout);
  profiler_.Store(kMemoryBase, db1_);
  profiler_.WriteProfile(std::cout);
  auto output = testing::internal::GetCapturedStdout();
  EXPECT_THAT(output, testing::HasSubstr(absl::StrFormat(
                          "0x%llx,0x%llx", kMemoryBase, kMemoryBase)))
      << output;
}

TEST_F(MemoryUseProfilerTest, SingleHalfStore) {
  testing::internal::CaptureStdout();
  profiler_.WriteProfile(std::cout);
  profiler_.Store(kMemoryBase, db2_);
  profiler_.WriteProfile(std::cout);
  auto output = testing::internal::GetCapturedStdout();
  EXPECT_THAT(output, testing::HasSubstr(absl::StrFormat(
                          "0x%llx,0x%llx", kMemoryBase, kMemoryBase)))
      << output;
}

TEST_F(MemoryUseProfilerTest, SingleWordStore) {
  testing::internal::CaptureStdout();
  profiler_.WriteProfile(std::cout);
  profiler_.Store(kMemoryBase, db4_);
  profiler_.WriteProfile(std::cout);
  auto output = testing::internal::GetCapturedStdout();
  EXPECT_THAT(output, testing::HasSubstr(absl::StrFormat(
                          "0x%llx,0x%llx", kMemoryBase, kMemoryBase)))
      << output;
}

TEST_F(MemoryUseProfilerTest, SingleDoubleStore) {
  testing::internal::CaptureStdout();
  profiler_.WriteProfile(std::cout);
  profiler_.Store(kMemoryBase, db8_);
  profiler_.WriteProfile(std::cout);
  auto output = testing::internal::GetCapturedStdout();
  EXPECT_THAT(output, testing::HasSubstr(absl::StrFormat(
                          "0x%llx,0x%llx", kMemoryBase, kMemoryBase + 4)))
      << output;
}

TEST_F(MemoryUseProfilerTest, SpanInSingleRange) {
  testing::internal::CaptureStdout();
  for (int i = 0; i < 0x64; i += 4) {
    profiler_.Load(kMemoryBase + i, db4_, nullptr, nullptr);
  }
  profiler_.WriteProfile(std::cout);
  auto output = testing::internal::GetCapturedStdout();
  EXPECT_THAT(output, testing::HasSubstr(absl::StrFormat(
                          "0x%llx,0x%llx", kMemoryBase, kMemoryBase + 0x60)))
      << output;
}

TEST_F(MemoryUseProfilerTest, SpanInMultipleRanges) {
  testing::internal::CaptureStdout();
  auto seg_size = MemoryUseTracker::kSegmentSize;
  for (int i = 0; i < seg_size; i += 4) {
    profiler_.Load(kMemoryBase + i, db4_, nullptr, nullptr);
  }
  profiler_.WriteProfile(std::cout);
  auto output = testing::internal::GetCapturedStdout();
  EXPECT_THAT(output,
              testing::HasSubstr(absl::StrFormat("0x%llx,0x%llx", kMemoryBase,
                                                 kMemoryBase + seg_size - 4)))
      << output;
}
}  // namespace
