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

#include "cheriot/riscv_cheriot_minstret.h"

#include <cstdint>
#include <string>

#include "cheriot/cheriot_state.h"
#include "riscv//riscv_csr.h"

namespace mpact::sim::cheriot {

using ::mpact::sim::riscv::RiscVCsrEnum;

RiscVCheriotMInstret::RiscVCheriotMInstret(std::string name,
                                           CheriotState *state)
    : RiscVSimpleCsr<uint32_t>(name, RiscVCsrEnum::kMInstret, state) {}

// Read the current value of the counter and apply the offset.
uint32_t RiscVCheriotMInstret::GetUint32() {
  if (counter_ == nullptr) return offset_;
  uint32_t value = GetCounterValue() + offset_;
  return value;
}

uint64_t RiscVCheriotMInstret::GetUint64() {
  return static_cast<uint64_t>(GetUint32());
}

void RiscVCheriotMInstret::Set(uint32_t value) {
  offset_ = value - GetCounterValue();
}

void RiscVCheriotMInstret::Set(uint64_t value) {
  Set(static_cast<uint32_t>(value));
}

RiscVCheriotMInstreth::RiscVCheriotMInstreth(std::string name,
                                             CheriotState *state)
    : RiscVSimpleCsr<uint32_t>(name, RiscVCsrEnum::kMInstretH, state) {}

// Read the current value of the counter and apply the offset.
uint32_t RiscVCheriotMInstreth::GetUint32() {
  if (counter_ == nullptr) return offset_;
  uint32_t value = GetCounterValue() + offset_;
  return value;
}

uint64_t RiscVCheriotMInstreth::GetUint64() {
  return static_cast<uint64_t>(GetUint32());
}

void RiscVCheriotMInstreth::Set(uint32_t value) {
  offset_ = value - GetCounterValue();
}

void RiscVCheriotMInstreth::Set(uint64_t value) {
  Set(static_cast<uint32_t>(value));
}

}  // namespace mpact::sim::cheriot
