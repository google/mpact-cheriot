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

#ifndef MPACT_CHERIOT_CHERIOT_IBEX_HW_REVOKER_H_
#define MPACT_CHERIOT_CHERIOT_IBEX_HW_REVOKER_H_

#include <cstdint>

#include "cheriot/cheriot_register.h"
#include "mpact/sim/generic/counters_base.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/ref_count.h"
#include "mpact/sim/util/memory/memory_interface.h"
#include "mpact/sim/util/memory/tagged_memory_interface.h"
#include "riscv//riscv_plic.h"

// This file contains the class declaration for the model of the Ibex HW
// revoker for Cheriot. The HW revoker is a module that is used to invalidate
// (or revoke the validity of) capabilities pointing to a freed portion of heap
// memory. It is controlled by a set of memory mapped registers.
//
// The HW revoker is implemented as a counter value set object. It is bound
// to a counter that is incremented whenever an instruction is executed, and,
// when active, performs an action every 'period' number of increments
// (configurable).
//
// The HW revoker is programmed using a memory interface. It supports non-vector
// loads and stores only.
//
// The HW revoker is described in more detail in the following documents:
// https://lowrisc.github.io/sonata-system/doc/ip/revoker.html
// https://github.com/microsoft/cheriot-safe/blob/main/src/msft_cheri_subsystem/msftDvIp_mmreg.sv

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::CounterValueSetInterface;
using ::mpact::sim::generic::DataBuffer;
using ::mpact::sim::generic::DataBufferFactory;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::generic::ReferenceCount;
using ::mpact::sim::riscv::RiscVPlic;
using ::mpact::sim::riscv::RiscVPlicIrqInterface;
using ::mpact::sim::util::MemoryInterface;
using ::mpact::sim::util::TaggedMemoryInterface;

class CheriotIbexHWRevoker : public CounterValueSetInterface<uint64_t>,
                             public TaggedMemoryInterface {
 public:
  static constexpr uint32_t kStartAddressOffset = 0x0000;
  static constexpr uint32_t kEndAddressOffset = 0x0004;
  static constexpr uint32_t kGoOffset = 0x0008;
  static constexpr uint32_t kEpochOffset = 0x000c;
  static constexpr uint32_t kStatusOffset = 0x0010;
  static constexpr uint32_t kInterruptEnableOffset = 0x0014;

  CheriotIbexHWRevoker() = delete;
  CheriotIbexHWRevoker(const CheriotIbexHWRevoker &) = delete;
  CheriotIbexHWRevoker &operator=(const CheriotIbexHWRevoker &) = delete;
  CheriotIbexHWRevoker(RiscVPlicIrqInterface *plic_irq, uint64_t heap_base,
                       uint64_t heap_size, TaggedMemoryInterface *heap_memory,
                       uint64_t revocation_bits_base,
                       MemoryInterface *revocation_memory);
  CheriotIbexHWRevoker(uint64_t heap_base, uint64_t heap_size,
                       TaggedMemoryInterface *heap_memory,
                       uint64_t revocation_bits_base,
                       MemoryInterface *revocation_memory);
  ~CheriotIbexHWRevoker();
  // Resets the interrupt controller.
  void Reset();
  // CounterValueSetInterface override. This is called when the value of the
  // bound counter is modified.
  void SetValue(const uint64_t &val) override;

  // MemoryInterface overrides.
  // Non-vector load method.
  void Load(uint64_t address, DataBuffer *db, DataBuffer *tags,
            Instruction *inst, ReferenceCount *context) override;
  void Load(uint64_t address, DataBuffer *db, Instruction *inst,
            ReferenceCount *context) override;
  // Vector load method - this is stubbed out.
  void Load(DataBuffer *address_db, DataBuffer *mask_db, int el_size,
            DataBuffer *db, Instruction *inst,
            ReferenceCount *context) override;
  // Non-vector store method.
  void Store(uint64_t address, DataBuffer *db, DataBuffer *tags) override;
  void Store(uint64_t address, DataBuffer *dbs) override;
  // Vector store method - this is stubbed out.
  void Store(DataBuffer *address, DataBuffer *mask, int el_size,
             DataBuffer *db) override;

  // Getters & setters.
  void set_plic_irq(RiscVPlicIrqInterface *plic_irq) { plic_irq_ = plic_irq; }
  int period() const { return period_; }
  void set_period(int period) { period_ = period; }
  int cap_count() const { return cap_count_; }
  void set_cap_count(int cap_count) { cap_count_ = cap_count; }
  uint64_t revocation_bits_base() const { return revocation_bits_base_; }
  void set_revocation_bits_base(uint64_t revocation_bits_base) {
    revocation_bits_base_ = revocation_bits_base;
  }

 private:
  // MMR read/write methods.
  uint32_t Read(uint32_t offset);
  void Write(uint32_t offset, uint32_t value);
  void WriteGo();
  // Perform an iteration of revocation.
  void Revoke();
  void ProcessCapability(uint64_t address);
  bool MustRevoke(uint64_t address);
  void SetInterrupt(bool value);
  // The number of times SetValue is called before triggering a revocation
  // operation.
  int period_ = 1;
  // Tracker for the number of times SetValue is called.
  int num_calls_ = 0;
  // The max number of capabilities to revoke in a single operation.
  int cap_count_ = 0;
  // Current capability index.
  uint64_t current_cap_ = 0;
  // Sweep in progress.
  bool sweep_in_progress_ = false;
  RiscVPlicIrqInterface *plic_irq_ = nullptr;
  // Heap range.
  uint64_t heap_base_ = 0;
  uint64_t heap_max_ = 0;
  // Memory interface to use for the tagged heap.
  TaggedMemoryInterface *heap_memory_ = nullptr;
  // Memory interface to use for the revocation bits.
  MemoryInterface *revocation_memory_ = nullptr;
  // Data buffers.
  DataBuffer *db_ = nullptr;
  DataBuffer *tag_db_ = nullptr;
  // Capability register.
  CheriotRegister *cap_reg_ = nullptr;
  // Base address of the revocation bits.
  uint64_t revocation_bits_base_ = 0;
  // DB factory.
  DataBufferFactory db_factory_;

  // MMRs
  uint64_t start_address_ = 0;
  uint64_t end_address_ = 0;
  uint32_t go_ = 0;
  uint32_t epoch_ = 0;
  uint32_t interrupt_enable_ = 0;
  uint32_t interrupt_status_ = 0;
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT_CHERIOT_IBEX_HW_REVOKER_H_
