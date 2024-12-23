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

#include <cstdint>
#include <cstring>

#include "absl/log/log.h"
#include "cheriot/cheriot_register.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/ref_count.h"
#include "mpact/sim/util/memory/memory_interface.h"
#include "mpact/sim/util/memory/tagged_memory_interface.h"
#include "riscv//riscv_plic.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::DataBuffer;
using ::mpact::sim::generic::Instruction;
using ::mpact::sim::generic::ReferenceCount;
using ::mpact::sim::riscv::RiscVPlicIrqInterface;
using ::mpact::sim::util::MemoryInterface;
using ::mpact::sim::util::TaggedMemoryInterface;

CheriotIbexHWRevoker::CheriotIbexHWRevoker(RiscVPlicIrqInterface *plic_irq,
                                           uint64_t heap_base,
                                           uint64_t heap_size,
                                           TaggedMemoryInterface *heap_memory,
                                           uint64_t revocation_bits_base,
                                           MemoryInterface *revocation_memory)
    : plic_irq_(plic_irq),
      heap_base_(heap_base),
      heap_max_(heap_base + heap_size),
      heap_memory_(heap_memory),
      revocation_memory_(revocation_memory),
      revocation_bits_base_(revocation_bits_base) {
  cap_reg_ = new CheriotRegister(nullptr, "filter_cap");
  db_ = db_factory_.Allocate<uint32_t>(2);
  db_->Set<uint32_t>(0, 0);
  db_->Set<uint32_t>(1, 0);
  db_->set_latency(0);
  cap_reg_->SetDataBuffer(db_);
  db_->DecRef();
  // Allocate data buffers used in loads/stores.
  db_ = db_factory_.Allocate<uint8_t>(CheriotRegister::kCapabilitySizeInBytes);
  db_->set_latency(0);
  tag_db_ = db_factory_.Allocate<uint8_t>(1);
  tag_db_->set_latency(0);
  Reset();
}

CheriotIbexHWRevoker::CheriotIbexHWRevoker(uint64_t heap_base,
                                           uint64_t heap_size,
                                           TaggedMemoryInterface *heap_memory,
                                           uint64_t revocation_bits_base,
                                           MemoryInterface *revocation_memory)
    : CheriotIbexHWRevoker(nullptr, heap_base, heap_size, heap_memory,
                           revocation_bits_base, revocation_memory) {}

CheriotIbexHWRevoker::~CheriotIbexHWRevoker() {
  delete cap_reg_;
  db_->DecRef();
  tag_db_->DecRef();
}

// Reset state to initial values.
void CheriotIbexHWRevoker::Reset() {
  num_calls_ = 0;
  start_address_ = 0;
  end_address_ = 0;
  go_ = 0;
  epoch_ = 0;
  interrupt_enable_ = 0;
  interrupt_status_ = 0;
}

// This is called by the counter using the CounterValueSetInterface interface.
void CheriotIbexHWRevoker::SetValue(const uint64_t &val) {
  if (interrupt_status_) SetInterrupt(false);
  if (!sweep_in_progress_) return;
  num_calls_++;
  if (num_calls_ >= period_) {
    num_calls_ = 0;
    Revoke();
  }
}

// Reads from the MMRs.
void CheriotIbexHWRevoker::Load(uint64_t address, DataBuffer *db,
                                DataBuffer *tags, Instruction *inst,
                                ReferenceCount *context) {
  if (tags != nullptr) std::memset(tags->raw_ptr(), 0, tags->size<uint8_t>());
  Load(address, db, inst, context);
}

// Reads from the MMRs.
void CheriotIbexHWRevoker::Load(uint64_t address, DataBuffer *db,
                                Instruction *inst, ReferenceCount *context) {
  uint32_t offset = address & 0xffff;
  switch (db->size<uint8_t>()) {
    case 1:
      db->Set<uint32_t>(0, static_cast<uint8_t>(Read(offset)));
      break;
    case 2:
      db->Set<uint32_t>(0, static_cast<uint16_t>(Read(offset)));
      break;
    case 4:
      db->Set<uint32_t>(0, static_cast<uint32_t>(Read(offset)));
      break;
    case 8:
      db->Set<uint32_t>(0, static_cast<uint64_t>(Read(offset)));
      break;
    default:
      ::memset(db->raw_ptr(), 0, sizeof(db->size<uint8_t>()));
      break;
  }
  // Execute the instruction to process and write back the load data.
  if (nullptr != inst) {
    if (db->latency() > 0) {
      inst->IncRef();
      if (context != nullptr) context->IncRef();
      inst->state()->function_delay_line()->Add(db->latency(),
                                                [inst, context]() {
                                                  inst->Execute(context);
                                                  if (context != nullptr)
                                                    context->DecRef();
                                                  inst->DecRef();
                                                });
    } else {
      inst->Execute(context);
    }
  }
}

// Vector load is not supported.
void CheriotIbexHWRevoker::Load(DataBuffer *address_db, DataBuffer *mask_db,
                                int el_size, DataBuffer *db, Instruction *inst,
                                ReferenceCount *context) {
  // This is left unimplemented. Vector load is not supported.
  LOG(FATAL) << "Vector load not supported";  // Crash OK
}

// Writes to the MMRs.
void CheriotIbexHWRevoker::Store(uint64_t address, DataBuffer *db,
                                 DataBuffer *tags) {
  Store(address, db);
}

// Writes to the MMRs.
void CheriotIbexHWRevoker::Store(uint64_t address, DataBuffer *db) {
  uint32_t offset = address & 0xffff;
  switch (db->size<uint8_t>()) {
    case 1:
      return Write(offset, static_cast<uint32_t>(db->Get<uint8_t>(0)));
    case 2:
      return Write(offset, static_cast<uint32_t>(db->Get<uint16_t>(0)));
    case 4:
      return Write(offset, static_cast<uint32_t>(db->Get<uint32_t>(0)));
    case 8:
      return Write(offset, static_cast<uint32_t>(db->Get<uint32_t>(0)));
      return Write(offset + 4, static_cast<uint32_t>(db->Get<uint32_t>(1)));
    default:
      return;
  }
}

// Vector accesses are not supported.
void CheriotIbexHWRevoker::Store(DataBuffer *address, DataBuffer *mask,
                                 int el_size, DataBuffer *db) {
  // This is left unimplemented. Vector store is not supported.
  LOG(FATAL) << "Vector store not supported";  // Crash OK
}

uint32_t CheriotIbexHWRevoker::Read(uint32_t offset) {
  uint32_t value = 0;
  switch (offset) {
    case kStartAddressOffset:  // start address
      value = start_address_;
      break;
    case kEndAddressOffset:  // end address
      value = end_address_;
      break;
    case kGoOffset:  // go
      value = 0x5500'0000 | (go_ & 0x00ff'ffff);
      break;
    case kEpochOffset:  // epoch
      value = (epoch_ << 1) | (sweep_in_progress_ ? 0b1 : 0b0);
      break;
    case kStatusOffset:  // stat q
      value = interrupt_enable_ ? interrupt_status_ : 0;
      break;
    case kInterruptEnableOffset:  // interrupt enable
      value = interrupt_enable_ & 0b1;
      break;
    default:
      value = 0;
      break;
  }
  return value;
}

// Vector store is not supported.
void CheriotIbexHWRevoker::Write(uint32_t offset, uint32_t value) {
  switch (offset) {
    case kStartAddressOffset:  // start address
      start_address_ = value;
      break;
    case kEndAddressOffset:  // end address
      end_address_ = value;
      break;
    case kGoOffset:  // go
      WriteGo();
      go_ = value;
      break;
    // 0x000c:  Epoch is not writable.
    case kStatusOffset:
      interrupt_status_ = 0;
      plic_irq_->SetIrq(false);
      break;
    case kInterruptEnableOffset:  // interrupt enable
      interrupt_enable_ = value & 0b1;
      ;
      break;
    default:
      return;
  }
}

void CheriotIbexHWRevoker::WriteGo() {
  if (sweep_in_progress_) return;
  sweep_in_progress_ = true;
  current_cap_ = 0;
  num_calls_ = 0;
  epoch_ = 0;
}

void CheriotIbexHWRevoker::Revoke() {
  if (!sweep_in_progress_) return;
  // Increment the epoch.
  epoch_++;
  uint64_t cap_address = start_address_ + (current_cap_++ << 3);
  // Align address to the capability size.
  cap_address &= ~0b111ULL;
  ProcessCapability(cap_address);
  // Check to see if we have reached the end of the region.
  if (cap_address >= end_address_) {
    sweep_in_progress_ = false;
    SetInterrupt(true);
  }
}

// Process the capability at the given address.
void CheriotIbexHWRevoker::ProcessCapability(uint64_t address) {
  if ((address < start_address_) || (address >= end_address_)) return;
  // Load the capability.
  heap_memory_->Load(address, db_, tag_db_, nullptr, nullptr);
  // If the tag is 0, no need to go on.
  auto tag = tag_db_->Get<uint8_t>(0);
  if (tag == 0) return;

  // Expand the capability. Return if the tag is not valid.
  cap_reg_->Expand(db_->Get<uint32_t>(0), db_->Get<uint32_t>(1), tag);
  if (!cap_reg_->tag()) return;

  // Check for revocation.
  if (!MustRevoke(cap_reg_->base())) return;

  // Invalidate and store the capability back to memory.
  cap_reg_->Invalidate();
  db_->Set<uint32_t>(0, cap_reg_->address());
  db_->Set<uint32_t>(1, cap_reg_->Compress());
  tag_db_->Set<uint8_t>(0, cap_reg_->tag());
  heap_memory_->Store(address, db_, tag_db_);
}

// Check if the capability must be revoked.
bool CheriotIbexHWRevoker::MustRevoke(uint64_t address) {
  if (address < heap_base_) return false;
  if (address >= heap_max_) return false;
  // Compute the address of the byte containing the revocation information.
  uint64_t offset = address - heap_base_;
  // Shift by 3 for the size of the capability (8), and by 3 for 8 bits in a
  // byte.
  uint64_t revocation_offset = offset >> 6;
  revocation_memory_->Load(revocation_bits_base_ + revocation_offset, tag_db_,
                           nullptr, nullptr);
  // Get the revocation bit.
  uint8_t revocation_bits = tag_db_->Get<uint8_t>(0);
  int bit_offset = (offset >> 3) & 0b111;
  bool result = revocation_bits & (1 << bit_offset);
  return result;
}

void CheriotIbexHWRevoker::SetInterrupt(bool value) {
  if (!value) {
    plic_irq_->SetIrq(false);
    interrupt_status_ = 0;
    return;
  }
  interrupt_status_ = static_cast<uint32_t>(value);

  if (!interrupt_enable_) return;

  if (plic_irq_ != nullptr) plic_irq_->SetIrq(value);
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
