// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cheriot/cheriot_vector_state.h"

#include <cstdint>

#include "absl/log/log.h"
#include "cheriot/cheriot_state.h"
#include "riscv//riscv_csr.h"

namespace mpact {
namespace sim {
namespace cheriot {
namespace {

constexpr char kVlName[] = "vl";
constexpr uint32_t kVlReadMask = 0xffff'ffff;
constexpr uint32_t kVlWriteMask = 0;
constexpr uint32_t kVlInitial = 0;

constexpr char kVtypeName[] = "vtype";
constexpr uint32_t kVtypeReadMask = 0xffff'ffff;
constexpr uint32_t kVtypeWriteMask = 0;
constexpr uint32_t kVtypeInitial = 0;

constexpr char kVlenbName[] = "vlenb";
constexpr uint32_t kVlenbReadMask = 0xffff'ffff;
constexpr uint32_t kVlenbWriteMask = 0;

constexpr char kVstartName[] = "vstart";
constexpr uint32_t kVstartReadMask = 0xffff'ffff;
constexpr uint32_t kVstartWriteMask = 0;
constexpr uint32_t kVstartInitial = 0;

constexpr char kVxsatName[] = "vxsat";
constexpr uint32_t kVxsatReadMask = 1;
constexpr uint32_t kVxsatWriteMask = 1;
constexpr uint32_t kVxsatInitial = 0;

constexpr char kVxrmName[] = "vxrm";
constexpr uint32_t kVxrmReadMask = 3;
constexpr uint32_t kVxrmWriteMask = 3;
constexpr uint32_t kVxrmInitial = 0;

constexpr char kVcsrName[] = "vcsr";
constexpr uint32_t kVcsrReadMask = 7;
constexpr uint32_t kVcsrWriteMask = 7;
constexpr uint32_t kVcsrInitial = 0;

// Helper function to avoid some extra code below.
static inline void LogIfError(absl::Status status) {
  if (status.ok()) return;
  LOG(ERROR) << status.message();
}

}  // namespace

using ::mpact::sim::riscv::RiscVCsrEnum;
using ::mpact::sim::riscv::RiscVSimpleCsr;

CheriotVl::CheriotVl(CheriotVectorState* vector_state)
    : RiscVSimpleCsr<uint32_t>(kVlName, RiscVCsrEnum::kVl, kVlInitial,
                               kVlReadMask, kVlWriteMask,
                               vector_state->state()),
      vector_state_(vector_state) {}

uint32_t CheriotVl::AsUint32() { return vector_state_->vector_length(); }

CheriotVtype::CheriotVtype(CheriotVectorState* vector_state)
    : RiscVSimpleCsr<uint32_t>(kVtypeName, RiscVCsrEnum::kVtype, kVtypeInitial,
                               kVtypeReadMask, kVtypeWriteMask,
                               vector_state->state()),
      vector_state_(vector_state) {}

uint32_t CheriotVtype::AsUint32() { return vector_state_->vtype(); }

CheriotVstart::CheriotVstart(CheriotVectorState* vector_state)
    : RiscVSimpleCsr<uint32_t>(kVstartName, RiscVCsrEnum::kVstart,
                               kVstartInitial, kVstartReadMask,
                               kVstartWriteMask, vector_state->state()),
      vector_state_(vector_state) {}

uint32_t CheriotVstart::AsUint32() { return vector_state_->vstart(); }

void CheriotVstart::Write(uint32_t value) { vector_state_->set_vstart(value); }

CheriotVxsat::CheriotVxsat(CheriotVectorState* vector_state)
    : RiscVSimpleCsr<uint32_t>(kVxsatName, RiscVCsrEnum::kVxsat, kVxsatInitial,
                               kVxsatReadMask, kVxsatWriteMask,
                               vector_state->state()),
      vector_state_(vector_state) {}

uint32_t CheriotVxsat::AsUint32() { return vector_state_->vxsat() ? 1 : 0; }

void CheriotVxsat::Write(uint32_t value) {
  vector_state_->set_vxsat(value & 1);
}

CheriotVxrm::CheriotVxrm(CheriotVectorState* vector_state)
    : RiscVSimpleCsr<uint32_t>(kVxrmName, RiscVCsrEnum::kVxrm, kVxrmInitial,
                               kVxrmReadMask, kVxrmWriteMask,
                               vector_state->state()),
      vector_state_(vector_state) {}

uint32_t CheriotVxrm::AsUint32() { return vector_state_->vxrm(); }

void CheriotVxrm::Write(uint32_t value) {
  vector_state_->set_vxrm(value & kVxrmWriteMask);
}

CheriotVcsr::CheriotVcsr(CheriotVectorState* vector_state)
    : RiscVSimpleCsr<uint32_t>(kVcsrName, RiscVCsrEnum::kVcsr, kVcsrInitial,
                               kVcsrReadMask, kVcsrWriteMask,
                               vector_state->state()),
      vector_state_(vector_state) {}

uint32_t CheriotVcsr::AsUint32() {
  const uint32_t vxrm_shifted = (vector_state_->vxrm() & kVxrmWriteMask) << 1;
  const uint32_t vxsat = vector_state_->vxsat() ? 1 : 0;
  return vxrm_shifted | vxsat;
}

void CheriotVcsr::Write(uint32_t value) {
  const uint32_t vxrm = (value >> 1) & kVxrmWriteMask;
  const uint32_t vxsat = value & 1;
  vector_state_->set_vxrm(vxrm);
  vector_state_->set_vxsat(vxsat);
}

// Constructor for the vector class. Need to pass in the parent RV32 state and
// the vector length in bytes.
CheriotVectorState::CheriotVectorState(CheriotState* state, int byte_length)
    : vector_register_byte_length_(byte_length),
      vl_csr_(this),
      vtype_csr_(this),
      vlenb_csr_(kVlenbName, RiscVCsrEnum::kVlenb, vector_register_byte_length_,
                 kVlenbReadMask, kVlenbWriteMask, state),
      vstart_csr_(this),
      vxsat_csr_(this),
      vxrm_csr_(this),
      vcsr_csr_(this) {
  state_ = state;
  state->set_rv_vector(this);
  state->set_vector_register_width(byte_length);

  LogIfError(state->csr_set()->AddCsr(&vl_csr_));
  LogIfError(state->csr_set()->AddCsr(&vtype_csr_));
  LogIfError(state->csr_set()->AddCsr(&vlenb_csr_));
  LogIfError(state->csr_set()->AddCsr(&vstart_csr_));
  LogIfError(state->csr_set()->AddCsr(&vxsat_csr_));
  LogIfError(state->csr_set()->AddCsr(&vxrm_csr_));
  LogIfError(state->csr_set()->AddCsr(&vcsr_csr_));
}

// This function parses the vector type, as used in the vset* instructions
// and sets the internal vector state accordingly.
void CheriotVectorState::SetVectorType(uint32_t vtype) {
  static const int lmul8_values[8] = {8, 16, 32, 64, 0, 1, 2, 4};
  static const int sew_values[8] = {8, 16, 32, 64, 0, 0, 0, 0};
  set_vtype(vtype);
  // The vtype field is divided into the following fields:
  // [2..0]: vector length multiplier.
  // [5..3]: element width specifier.
  // [6]:    vector tail agnostic bit.
  // [7]:    vector mask agnostic bit.
  // Extract the lmul.
  set_vector_length_multiplier(lmul8_values[(vtype & 0b111)]);
  // Extract the sew and convert from bits to bytes.
  set_selected_element_width(sew_values[(vtype >> 3) & 0b111] >> 3);
  // Extract the tail and mask agnostic flags.
  set_vector_tail_agnostic(static_cast<bool>((vtype >> 6) & 0b1));
  set_vector_mask_agnostic(static_cast<bool>((vtype >> 7) & 0b1));
  // Compute the new max vector length.
  max_vector_length_ = vector_register_byte_length() *
                       vector_length_multiplier() /
                       (8 * selected_element_width());
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
