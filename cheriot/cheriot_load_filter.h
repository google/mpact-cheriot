/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MPACT_CHERIOT__CHERIOT_LOAD_FILTER_H_
#define MPACT_CHERIOT__CHERIOT_LOAD_FILTER_H_

#include <cstdint>

#include "cheriot/cheriot_register.h"
#include "mpact/sim/generic/counters_base.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/util/memory/tagged_memory_interface.h"

// This file defines a class that performs periodic revocation filtering of
// capabilities in memory. It does so by in effect doing a "load capability"
// instruction, which checks for revocation, followed by a "store capability"
// instruction, if the load capability invalidated the capability due to
// revocation. No exceptions are thrown. This class is linked to a counter and
// the SetValue method is invoked every time that counter changes values.

namespace mpact::sim::cheriot {

using ::mpact::sim::generic::CounterValueSetInterface;
using ::mpact::sim::generic::DataBuffer;
using ::mpact::sim::generic::DataBufferFactory;
using ::mpact::sim::util::TaggedMemoryInterface;

class CheriotLoadFilter : public CounterValueSetInterface<uint64_t> {
 public:
  // The constructor takes the following arguments:
  //   tagged_memory: The memory interface to use for capability loads/stores
  //                  and revocation queries.
  //   period:        The number of times SetValue is called before triggering
  //                  a filtering operation.
  //   count:         The number of capabilities to filter in an operation.
  //   base:          The base address of the capability filter range.
  //   top:           The top address of the filter range (inclusive).
  //   cap_base:      The base address of the capabilities address space. E.g.,
  //                  the base of the region of memory to which revokable
  //                  capabilities may use as their base addresses.
  //   revocation_base: The base address of the revocation data area.
  CheriotLoadFilter(TaggedMemoryInterface* tagged_memory, int period, int count,
                    uint64_t base, uint64_t top, uint64_t cap_base,
                    uint64_t revocation_base);
  CheriotLoadFilter() = delete;
  CheriotLoadFilter(const CheriotLoadFilter&) = delete;
  CheriotLoadFilter& operator=(const CheriotLoadFilter&) = delete;
  ~CheriotLoadFilter() override;

  // Overridden method called by updates of a linked counter.
  void SetValue(const uint64_t& value) override;

 private:
  // Loads the capability at the given address, checks for valid tag and
  // capability validity, and if valid, checks for revocation. If revoked,
  // it invalidates the capability and stores it back to memory.
  void FilterCapability(uint64_t address);
  // Checks for revocation status for the capability at the given address.
  // Returns true if it has been revoked.
  bool MustRevoke(uint64_t address);

  // Memory interface to use for loads/stores.
  TaggedMemoryInterface* tagged_memory_;
  // Counter to keep track of the number of times SetValue is called.
  int update_counter_ = 0;
  int period_;
  int count_;
  // Base and top addresses of the region to filter.
  uint64_t base_;
  uint64_t top_;
  uint64_t cap_base_;
  // Base address of the revocation data area.
  uint64_t revocation_base_;
  // Current capability address.
  uint64_t cap_address_;
  // Capability register used to expand the loaded capability into.
  CheriotRegister* cap_reg_;
  // Data buffer factory and data buffers used in loads/stores.
  DataBufferFactory db_factory_;
  DataBuffer* db_;
  DataBuffer* tag_db_;
};

}  // namespace mpact::sim::cheriot

#endif  // MPACT_CHERIOT__CHERIOT_LOAD_FILTER_H_
