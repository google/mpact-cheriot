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

#include "cheriot/cheriot_register.h"

namespace mpact::sim::cheriot {

CheriotLoadFilter::CheriotLoadFilter(TaggedMemoryInterface *tagged_memory,
                                     int period, int count, uint64_t base,
                                     uint64_t top, uint64_t cap_base,
                                     uint64_t revocation_base)
    : tagged_memory_(tagged_memory),
      period_(period),
      count_(count),
      base_(base),
      top_(top),
      cap_base_(cap_base),
      revocation_base_(revocation_base) {
  cap_reg_ = new CheriotRegister(nullptr, "filter_cap");
  db_ = db_factory_.Allocate<uint32_t>(1);
  db_->Set<uint32_t>(0, 0);
  db_->set_latency(0);
  cap_reg_->SetDataBuffer(db_);
  db_->DecRef();
  // Allocate data buffers used in loads/stores.
  db_ = db_factory_.Allocate<uint8_t>(CheriotRegister::kCapabilitySizeInBytes);
  db_->set_latency(0);
  tag_db_ = db_factory_.Allocate<uint8_t>(1);
  tag_db_->set_latency(0);
  cap_address_ = base_;
}

CheriotLoadFilter::~CheriotLoadFilter() {
  delete cap_reg_;
  db_->DecRef();
  tag_db_->DecRef();
}

// This is called when the linked counter increments. We are not interested
// in the value of the counter, just the number of increments.
void CheriotLoadFilter::SetValue(const uint64_t &) {
  if (++update_counter_ >= period_) {
    update_counter_ = 0;
    // Once triggered, perform count_ filter loads/stores.
    for (int i = 0; i < count_; ++i) {
      FilterCapability(cap_address_);
      cap_address_ += CheriotRegister::kCapabilitySizeInBytes;
      // Reset the capability pointer if it wrapped or went past the memory
      // space.
      if ((cap_address_ < base_) || (cap_address_ >= top_)) {
        cap_address_ = base_;
      }
    }
  }
}

// Filter the capability at the given address.
void CheriotLoadFilter::FilterCapability(uint64_t address) {
  // Load the capability.
  tagged_memory_->Load(address, db_, tag_db_, nullptr, nullptr);
  // If the tag is 0, no need to go on.
  auto tag = tag_db_->Get<uint8_t>(0);
  if (tag == 0) return;

  // Expand the capability. Return false if the tag is not valid.
  cap_reg_->Expand(db_->Get<uint32_t>(0), db_->Get<uint32_t>(1), tag);
  if (!cap_reg_->tag()) return;

  // Check for revocation.
  if (!MustRevoke(cap_reg_->base())) return;

  // Invalidate and store the capability back to memory.
  cap_reg_->Invalidate();
  db_->Set<uint32_t>(0, cap_reg_->address());
  db_->Set<uint32_t>(1, cap_reg_->Compress());
  tag_db_->Set<uint8_t>(0, cap_reg_->tag());
  tagged_memory_->Store(cap_address_, db_, tag_db_);
}

// Check if the capability must be revoked.
bool CheriotLoadFilter::MustRevoke(uint64_t address) {
  // If the address is less than the memory base, return false.
  if (address < cap_base_) return false;
  // Compute the address of the byte containing the revocation information.
  uint64_t offset = address - cap_base_;
  // Shift by 3 for the size of the capability (8), and by 3 for 8 bits in a
  // byte.
  uint64_t revocation_offset = offset >> 6;
  tagged_memory_->Load(revocation_base_ + revocation_offset, tag_db_, nullptr,
                       nullptr);
  // Get the revocation bit.
  uint8_t revocation_bits = tag_db_->Get<uint8_t>(0);
  int bit_offset = (offset >> 3) & 0b111;
  bool result = revocation_bits & (1 << bit_offset);
  return result;
}

}  // namespace mpact::sim::cheriot
