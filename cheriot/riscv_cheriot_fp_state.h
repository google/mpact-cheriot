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

#ifndef MPACT_CHERIOT__RISCV_CHERIOT_FP_STATE_H_
#define MPACT_CHERIOT__RISCV_CHERIOT_FP_STATE_H_

#include <cstdint>

#include "riscv//riscv_csr.h"
#include "riscv//riscv_fp_host.h"
#include "riscv//riscv_fp_info.h"

// This file contains code that manages the fp state of the RiscV processor.

namespace mpact {
namespace sim {
namespace cheriot {

class RiscVCheriotFPState;
class CheriotState;

using ::mpact::sim::riscv::FPRoundingMode;
using ::mpact::sim::riscv::HostFloatingPointInterface;
using ::mpact::sim::riscv::RiscVSimpleCsr;

// Floating point CSR.
class RiscVFcsr : public RiscVSimpleCsr<uint32_t> {
 public:
  RiscVFcsr() = delete;
  explicit RiscVFcsr(RiscVCheriotFPState *fp_state);
  ~RiscVFcsr() override = default;

  // Overrides.
  uint32_t AsUint32() override;
  uint64_t AsUint64() override;
  void Write(uint32_t value) override;
  void Write(uint64_t value) override;

 private:
  RiscVCheriotFPState *fp_state_;
};

// Floating point rounding mode csr.
class RiscVFrm : public RiscVSimpleCsr<uint32_t> {
 public:
  RiscVFrm() = delete;
  explicit RiscVFrm(RiscVCheriotFPState *fp_state);
  ~RiscVFrm() override = default;

  // Overrides.
  uint32_t AsUint32() override;
  uint64_t AsUint64() override { return AsUint32(); }
  void Write(uint32_t value) override;
  void Write(uint64_t value) override { Write(static_cast<uint32_t>(value)); }
  uint32_t GetUint32() override;
  uint64_t GetUint64() override { return GetUint32(); }
  void Set(uint32_t value) override;
  void Set(uint64_t value) override { Set(static_cast<uint32_t>(value)); }

 private:
  RiscVCheriotFPState *fp_state_;
};

// Floating point status flags csr.
class RiscVFflags : public RiscVSimpleCsr<uint32_t> {
 public:
  RiscVFflags() = delete;
  explicit RiscVFflags(RiscVCheriotFPState *fp_state);
  ~RiscVFflags() override = default;

  // Overrides.
  uint32_t AsUint32() override;
  uint64_t AsUint64() override { return AsUint32(); }
  void Write(uint32_t value) override;
  void Write(uint64_t value) override { Write(static_cast<uint32_t>(value)); }
  uint32_t GetUint32() override;
  uint64_t GetUint64() override { return GetUint32(); }
  void Set(uint32_t value) override;
  void Set(uint64_t value) override { Set(static_cast<uint32_t>(value)); }

 private:
  RiscVCheriotFPState *fp_state_;
};

class RiscVCheriotFPState {
 public:
  RiscVCheriotFPState() = delete;
  RiscVCheriotFPState(const RiscVCheriotFPState &) = delete;
  explicit RiscVCheriotFPState(CheriotState *rv_state);
  ~RiscVCheriotFPState();

  FPRoundingMode GetRoundingMode() const;

  void SetRoundingMode(FPRoundingMode mode);

  bool rounding_mode_valid() const { return rounding_mode_valid_; }

  // FP CSRs.
  RiscVFcsr *fcsr() const { return fcsr_; }
  RiscVFrm *frm() const { return frm_; }
  RiscVFflags *fflags() const { return fflags_; }
  // Parent state.
  CheriotState *rv_state() const { return rv_state_; }
  // Host interface.
  HostFloatingPointInterface *host_fp_interface() const {
    return host_fp_interface_;
  }

 private:
  CheriotState *rv_state_;
  RiscVFcsr *fcsr_ = nullptr;
  RiscVFrm *frm_ = nullptr;
  RiscVFflags *fflags_ = nullptr;
  HostFloatingPointInterface *host_fp_interface_;

  bool rounding_mode_valid_ = true;
  FPRoundingMode rounding_mode_ = FPRoundingMode::kRoundToNearest;
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__RISCV_CHERIOT_FP_STATE_H_
