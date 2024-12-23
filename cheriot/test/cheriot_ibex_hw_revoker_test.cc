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

#include "cheriot/cheriot_ibex_hw_revoker.h"

#include <sys/types.h>

#include <cstdint>
#include <cstring>

#include "cheriot/cheriot_register.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/util/memory/flat_demand_memory.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"
#include "mpact/sim/util/memory/tagged_memory_interface.h"
#include "riscv//riscv_plic.h"

// This file contains unit tests for the CheriotIbexHWRevoker class.

namespace {

using ::mpact::sim::cheriot::CheriotIbexHWRevoker;
using ::mpact::sim::cheriot::CheriotRegister;
using ::mpact::sim::generic::DataBuffer;
using ::mpact::sim::generic::DataBufferFactory;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::generic::ReferenceCount;
using ::mpact::sim::riscv::RiscVPlicIrqInterface;
using ::mpact::sim::util::FlatDemandMemory;
using ::mpact::sim::util::TaggedFlatDemandMemory;
using ::mpact::sim::util::TaggedMemoryInterface;

constexpr uint64_t kRevocationBase = 0x200'0000;
constexpr uint64_t kHeapBase = 0x8001'0000;
constexpr uint64_t kSweepBase = 0x8000'0000;

// Mock plic source interface.
class MockPlicSource : public RiscVPlicIrqInterface {
 public:
  MockPlicSource() = default;
  ~MockPlicSource() override = default;
  void SetIrq(bool irq_value) override { irq_value_ = irq_value; }

  bool irq_value() const { return irq_value_; }
  void set_irq_value(bool value) { irq_value_ = value; }

 private:
  bool irq_value_ = false;
};

// This class is used to capture memory load/store addresses.
class MemoryViewer : public TaggedMemoryInterface {
 public:
  MemoryViewer() = delete;
  MemoryViewer(TaggedMemoryInterface *memory) : memory_(memory) {}
  ~MemoryViewer() override = default;

  void Load(uint64_t address, DataBuffer *db, DataBuffer *tags,
            Instruction *inst, ReferenceCount *context) override {
    ld_address_ = address;
    memory_->Load(address, db, tags, inst, context);
  }
  void Load(uint64_t address, DataBuffer *db, Instruction *inst,
            ReferenceCount *context) override {
    ld_address_ = address;
    memory_->Load(address, db, inst, context);
  }
  void Load(DataBuffer *address_db, DataBuffer *mask_db, int el_size,
            DataBuffer *db, Instruction *inst,
            ReferenceCount *context) override {
    ld_address_ = address_db->Get<uint64_t>(0);
    memory_->Load(address_db, mask_db, el_size, db, inst, context);
  }
  void Store(uint64_t address, DataBuffer *db, DataBuffer *tags) override {
    st_address_ = address;
    memory_->Store(address, db, tags);
  }
  void Store(uint64_t address, DataBuffer *db) override {
    st_address_ = address;
    memory_->Store(address, db);
  }
  void Store(DataBuffer *address_db, DataBuffer *mask_db, int el_size,
             DataBuffer *db) override {
    st_address_ = address_db->Get<uint64_t>(0);
    memory_->Store(address_db, mask_db, el_size, db);
  }

  uint64_t ld_address() const { return ld_address_; }
  uint64_t st_address() const { return st_address_; }

 private:
  TaggedMemoryInterface *memory_ = nullptr;
  uint64_t ld_address_ = 0;
  uint64_t st_address_ = 0;
};

class CheriotIbexHwRevokerTest : public ::testing::Test {
 protected:
  CheriotIbexHwRevokerTest() {
    db1_ = db_factory_.Allocate<uint8_t>(1);
    db4_ = db_factory_.Allocate<uint32_t>(1);
    db8_ = db_factory_.Allocate<uint32_t>(2);
    db128_ = db_factory_.Allocate<uint8_t>(128);
    plic_irq_ = new MockPlicSource();
    heap_memory_ = new TaggedFlatDemandMemory(8);
    memory_viewer_ = new MemoryViewer(heap_memory_);
    revocation_memory_ = new FlatDemandMemory();
    revoker_ =
        new CheriotIbexHWRevoker(plic_irq_, kHeapBase, 0x8000, memory_viewer_,
                                 kRevocationBase, revocation_memory_);
    cap_reg_ = new CheriotRegister(nullptr, "cap");
    cap_db_ = db_factory_.Allocate<uint32_t>(1);
    cap_reg_->SetDataBuffer(cap_db_);
  }

  ~CheriotIbexHwRevokerTest() override {
    db1_->DecRef();
    db4_->DecRef();
    db8_->DecRef();
    db128_->DecRef();
    cap_db_->DecRef();
    delete plic_irq_;
    delete revoker_;
    delete heap_memory_;
    delete memory_viewer_;
    delete revocation_memory_;
    delete cap_reg_;
  }

  // Call to advance the revoker.
  void AdvanceRevoker() { revoker_->SetValue(0); }

  // Convenience method to set the revocation bit for the given address.
  void RevokeAddress(uint64_t address) {
    if (address < kHeapBase) return;
    uint64_t offset = address - kHeapBase;
    offset >>= 3;
    auto bit = offset & 0x7;
    offset >>= 3;
    revocation_memory_->Load(kRevocationBase + offset, db1_, nullptr, nullptr);
    uint8_t val = db1_->Get<uint8_t>(0);
    val |= 1 << bit;
    db1_->Set<uint8_t>(0, val);
    revocation_memory_->Store(kRevocationBase + offset, db1_);
  }

  // This clears the revocation bits for the memory range [kHeapBase, kHeapBase
  // + 0x8000].
  void ClearRevocationBits() {
    std::memset(db128_->raw_ptr(), 0, db128_->size<uint8_t>());
    uint64_t address = kRevocationBase;
    for (uint64_t i = 0; i < 0x100; ++i) {
      revocation_memory_->Store(address, db128_);
      address += db128_->size<uint8_t>();
    }
  }

  // The following methods are convenience methods for accessing the MMRs of
  // the hw revoker using the revoker's memory interface.
  void SetStartAddress(uint32_t address) {
    db4_->Set<uint32_t>(0, address);
    revoker_->Store(CheriotIbexHWRevoker::kStartAddressOffset, db4_);
  }
  uint32_t GetStartAddress() {
    revoker_->Load(CheriotIbexHWRevoker::kStartAddressOffset, db4_, nullptr,
                   nullptr);
    return db4_->Get<uint32_t>(0);
  }
  void SetEndAddress(uint32_t address) {
    db4_->Set<uint32_t>(0, address);
    revoker_->Store(CheriotIbexHWRevoker::kEndAddressOffset, db4_);
  }
  uint32_t GetEndAddress() {
    revoker_->Load(CheriotIbexHWRevoker::kEndAddressOffset, db4_, nullptr,
                   nullptr);
    return db4_->Get<uint32_t>(0);
  }
  void SetGo(uint32_t go) {
    db4_->Set<uint32_t>(0, go);
    revoker_->Store(CheriotIbexHWRevoker::kGoOffset, db4_);
  }
  uint32_t GetGo() {
    revoker_->Load(CheriotIbexHWRevoker::kGoOffset, db4_, nullptr, nullptr);
    return db4_->Get<uint32_t>(0);
  }
  void SetEpoch(uint32_t epoch) {
    db4_->Set<uint32_t>(0, epoch);
    revoker_->Store(CheriotIbexHWRevoker::kEpochOffset, db4_);
  }
  uint32_t GetEpoch() {
    revoker_->Load(CheriotIbexHWRevoker::kEpochOffset, db4_, nullptr, nullptr);
    return db4_->Get<uint32_t>(0);
  }
  void SetStatus(uint32_t status) {
    db4_->Set<uint32_t>(0, status);
    revoker_->Store(CheriotIbexHWRevoker::kStatusOffset, db4_);
  }
  uint32_t GetStatus() {
    revoker_->Load(CheriotIbexHWRevoker::kStatusOffset, db4_, nullptr, nullptr);
    return db4_->Get<uint32_t>(0);
  }
  void SetInterruptEnable(uint32_t enable) {
    db4_->Set<uint32_t>(0, enable);
    revoker_->Store(CheriotIbexHWRevoker::kInterruptEnableOffset, db4_);
  }
  uint32_t GetInterruptEnable() {
    revoker_->Load(CheriotIbexHWRevoker::kInterruptEnableOffset, db4_, nullptr,
                   nullptr);
    return db4_->Get<uint32_t>(0);
  }

  // Convenience method to write a valid capability to memory with the given
  // base.
  void WriteCapability(uint64_t address, uint64_t base) {
    cap_reg_->ResetMemoryRoot();
    cap_reg_->SetAddress(base);
    cap_reg_->SetBounds(base, 0x10);
    db8_->Set<uint32_t>(0, cap_reg_->address());
    db8_->Set<uint32_t>(1, cap_reg_->Compress());
    db1_->Set<uint8_t>(0, true);
    heap_memory_->Store(address, db8_, db1_);
  }

  // Convenience method to read a capability from memory with the given base.
  CheriotRegister *ReadCapability(uint64_t address) {
    heap_memory_->Load(address, db8_, db1_, nullptr, nullptr);
    cap_reg_->Expand(db8_->Get<uint32_t>(0), db8_->Get<uint32_t>(1),
                     db1_->Get<uint8_t>(0));
    return cap_reg_;
  }

  uint64_t GetLoadAddress() { return memory_viewer_->ld_address(); }
  uint64_t GetStoreAddress() { return memory_viewer_->st_address(); }
  MockPlicSource *plic_irq() { return plic_irq_; }

 private:
  CheriotRegister *cap_reg_ = nullptr;
  DataBufferFactory db_factory_;
  DataBuffer *db1_;
  DataBuffer *db4_;
  DataBuffer *db8_;
  DataBuffer *db128_;
  DataBuffer *cap_db_;
  CheriotIbexHWRevoker *revoker_ = nullptr;
  MockPlicSource *plic_irq_ = nullptr;
  TaggedFlatDemandMemory *heap_memory_ = nullptr;
  FlatDemandMemory *revocation_memory_ = nullptr;
  MemoryViewer *memory_viewer_ = nullptr;
};

// Initial state should all be clear.
TEST_F(CheriotIbexHwRevokerTest, TestInitial) {
  EXPECT_EQ(GetStartAddress(), 0);
  EXPECT_EQ(GetEndAddress(), 0);
  EXPECT_EQ(GetGo(), 0x5500'0000);
  EXPECT_EQ(GetEpoch(), 0);
  EXPECT_EQ(GetStatus(), 0);
  EXPECT_EQ(GetInterruptEnable(), 0);
}

// No valid capabilities in the sweep range.
TEST_F(CheriotIbexHwRevokerTest, RevokeNone) {
  SetStartAddress(kSweepBase);
  SetEndAddress(kSweepBase + 0x100);
  SetGo(1);
  EXPECT_EQ(GetGo(), 0x5500'0001);
  // Expect zero status.
  EXPECT_EQ(GetStatus(), 0);
  // Expect sweep to be started.
  EXPECT_EQ(GetEpoch(), 1);
  // Step through 256/8 - 1 capabilities.
  int num = 0x100 / 8;
  for (int i = 0; i < num; ++i) {
    AdvanceRevoker();
    EXPECT_EQ(GetLoadAddress(), kSweepBase + (i << 3));
    EXPECT_EQ(GetEpoch(), ((i + 1) << 1) | 1);
    EXPECT_EQ(GetStatus(), 0);
  }
  // Step through the next capability. The sweep should be done.
  AdvanceRevoker();
  // Notice the in progress bit is cleared.
  EXPECT_EQ(GetEpoch(), ((num + 1) << 1) | 0);
  // Interrupt status should be 0, as interrupt enable is off.
  EXPECT_EQ(GetStatus(), 0);
}

TEST_F(CheriotIbexHwRevokerTest, RevokeOne) {
  // Write a capability at the sweep base.
  for (auto offset = 0; offset < 0x100; offset += 0x8) {
    WriteCapability(kSweepBase + offset, kHeapBase + offset);
    auto *cap = ReadCapability(kSweepBase);
    EXPECT_TRUE(cap->tag());
  }
  // Revoke one capability.
  RevokeAddress(kHeapBase + 0x20);
  // Set the sweep range to include the capability.
  SetStartAddress(kSweepBase);
  SetEndAddress(kSweepBase + 0x100);
  SetGo(1);
  // Expect zero status.
  EXPECT_EQ(GetStatus(), 0);
  // Expect sweep to be started.
  EXPECT_EQ(GetEpoch(), 1);
  // Step through the sweep.
  while ((GetEpoch() & 0x1) == 1) {
    AdvanceRevoker();
  }
  // Since interrupt enable is not set, the status should be zero.
  EXPECT_EQ(GetStatus(), 0);
  // Verify that only the one revoked capability was invalidated.
  for (auto offset = 0; offset < 0x100; offset += 0x8) {
    auto *cap = ReadCapability(kSweepBase + offset);
    if (offset == 0x20) {
      EXPECT_FALSE(cap->tag());
    } else {
      EXPECT_TRUE(cap->tag());
    }
  }
}

TEST_F(CheriotIbexHwRevokerTest, RevokeWithInterrupt) {
  // Write a capability at the sweep base.
  for (auto offset = 0; offset < 0x100; offset += 0x8) {
    WriteCapability(kSweepBase + offset, kHeapBase + offset);
    auto *cap = ReadCapability(kSweepBase);
    EXPECT_TRUE(cap->tag());
  }
  // Revoke one capability.
  RevokeAddress(kHeapBase + 0x20);
  // Set the sweep range to include the capability.
  SetStartAddress(kSweepBase);
  SetEndAddress(kSweepBase + 0x100);
  // Enable interrupt.
  SetInterruptEnable(1);
  SetGo(1);
  while ((GetEpoch() & 0x1) == 1) {
    AdvanceRevoker();
  }
  EXPECT_EQ(GetStatus(), 1);
  // Verify that the interrupt was set.
  EXPECT_TRUE(plic_irq()->irq_value());
}

}  // namespace
