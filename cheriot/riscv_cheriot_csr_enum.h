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

#ifndef MPACT_CHERIOT__RISCV_CHERIOT_CSR_ENUM_H_
#define MPACT_CHERIOT__RISCV_CHERIOT_CSR_ENUM_H_

#include <any>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/operand_interface.h"

// This file contains CHERIoT specific definitions for classes used to model the
// RiscV control and status registers. For now, these are not modeled as actual
// register state, instead, they're tied into the RiscV machine state a bit
// more. This is so that side-effects from reads/writes can more easily be
// handled.

namespace mpact {
namespace sim {
namespace cheriot {

enum class RiscVCheriotCsrEnum {
  // User trap setup.
  kUStatus = 0x000,
  kUIe = 0x004,
  kUTvec = 0x005,
  // User vector CSRs.
  kVstart = 0x008,
  kVxsat = 0x009,
  kVxrm = 0x00a,
  kVcsr = 0x00f,
  // User trap handling.
  kUScratch = 0x040,
  kUEpc = 0x041,
  kUCause = 0x042,
  kUTval = 0x043,
  kUIp = 0x044,
  // User floating point CSRs.
  kFFlags = 0x001,
  kFrm = 0x002,
  kFCsr = 0x003,
  // User counters/timers.
  kCycle = 0xc00,
  kTime = 0xc01,
  kInstret = 0xc02,

  // Ignoring perf monitoring counters for now.

  kCycleH = 0xc80,
  kTimeH = 0xc81,
  kInstretH = 0x82,

  // Ignoring high bits of perf monitoring counters for now.

  // Supervisor trap setup.
  kSStatus = 0x100,
  kSEDeleg = 0x102,
  kSIDeleg = 0x103,
  kSIe = 0x104,
  kSTvec = 0x105,
  kSCounteren = 0x106,
  // Supervisor trap handling.
  kSScratch = 0x140,  // Scratch register for supervisor trap handlers.
  kSEpc = 0x141,      // Supervisor exception program counter.
  kSCause = 0x142,    // Supervisor trap cause.
  kSTval = 0x143,     // Supervisor bad address or instruction.
  kSIp = 0x144,       // Supervisor interrupt pending.

  // Supervisor protection and translation.
  kSAtp = 0x180,  // Supervisor address translation and protection.

  // Machine information registers.
  kMVendorId = 0xf11,  // Vendor ID.
  kMArchId = 0xf12,    // Architecture ID.
  kMImpId = 0xf13,     // Implementation ID.
  kMHartId = 0xf14,    // Hardware thread ID.
  // Machine trap setup.
  kMStatus = 0x300,     // Machine status register.
  kMIsa = 0x301,        // ISA and extensions.
  kMEDeleg = 0x302,     // Machine exception delegation register.
  kMIDeleg = 0x303,     // Machine interrupt delegation register.
  kMIe = 0x304,         // Machine interrupt-enable register.
  kMTvec = 0x305,       // Machine trap-handler base address.
  kMCounterEn = 0x306,  // Machine counter enable.
  // Machine trap handling.
  kMScratch = 0x340,  // Scratch register for machine trap handlers.
  kMEpc = 0x341,      // Machine exception program counter.
  kMCause = 0x342,    // Machine trap cause.
  kMTval = 0x343,     // Machine bad address or instruction.
  kMIp = 0x344,       // Machine interrupt pending.

  // Ignoring machine memory protection for now.

  kMCycle = 0xb00,    // Machine cycle counter.
  kMInstret = 0xb02,  // Machine instructions-retired counter.

  // Permformance counters.
  kMHpmcounter3 = 0xb03,
  kMHpmcounter4 = 0xb04,
  kMHpmcounter5 = 0xb05,
  kMHpmcounter6 = 0xb06,
  kMHpmcounter7 = 0xb07,
  kMHpmcounter8 = 0xb08,
  kMHpmcounter9 = 0xb09,
  kMHpmcounter10 = 0xb0a,
  kMHpmcounter11 = 0xb0b,
  kMHpmcounter12 = 0xb0c,
  kMHpmcounter13 = 0xb0d,
  kMHpmcounter14 = 0xb0e,
  kMHpmcounter15 = 0xb0f,
  kMHpmcounter16 = 0xb10,
  kMHpmcounter17 = 0xb11,
  kMHpmcounter18 = 0xb12,
  kMHpmcounter19 = 0xb13,
  kMHpmcounter20 = 0xb14,
  kMHpmcounter21 = 0xb15,
  kMHpmcounter22 = 0xb16,
  kMHpmcounter23 = 0xb17,
  kMHpmcounter24 = 0xb18,
  kMHpmcounter25 = 0xb19,
  kMHpmcounter26 = 0xb1a,
  kMHpmcounter27 = 0xb1b,
  kMHpmcounter28 = 0xb1c,
  kMHpmcounter29 = 0xb1d,
  kMHpmcounter30 = 0xb1e,
  kMHpmcounter31 = 0xb1f,

  kVl = 0xc20,     // Vector length.
  kVtype = 0xc21,  // Vector type.
  kVlenb = 0xc22,  // Vector length in bytes.

  kMCycleH = 0xb80,    // Upper 32 bits of mcycle.
  kMInstretH = 0xb82,  // Upper 32 bits of MInstret

  // Performance counters (high).
  kMHpmcounter3H = 0xb83,
  kMHpmcounter4H = 0xb84,
  kMHpmcounter5H = 0xb85,
  kMHpmcounter6H = 0xb86,
  kMHpmcounter7H = 0xb87,
  kMHpmcounter8H = 0xb88,
  kMHpmcounter9H = 0xb89,
  kMHpmcounter10H = 0xb8a,
  kMHpmcounter11H = 0xb8b,
  kMHpmcounter12H = 0xb8c,
  kMHpmcounter13H = 0xb8d,
  kMHpmcounter14H = 0xb8e,
  kMHpmcounter15H = 0xb8f,
  kMHpmcounter16H = 0xb90,
  kMHpmcounter17H = 0xb91,
  kMHpmcounter18H = 0xb92,
  kMHpmcounter19H = 0xb93,
  kMHpmcounter20H = 0xb94,
  kMHpmcounter21H = 0xb95,
  kMHpmcounter22H = 0xb96,
  kMHpmcounter23H = 0xb97,
  kMHpmcounter24H = 0xb98,
  kMHpmcounter25H = 0xb99,
  kMHpmcounter26H = 0xb9a,
  kMHpmcounter27H = 0xb9b,
  kMHpmcounter28H = 0xb9c,
  kMHpmcounter29H = 0xb9d,
  kMHpmcounter30H = 0xb9e,
  kMHpmcounter31H = 0xb9f,

  // CHERI mccsr register.
  kMCcsr = 0xbc0,

  // Ignoring debug trace registers.

  // Ignoring debug mode registers.

  // CherIoT specific registers.
  kMshwm = 0xbc1,   // Stack high water mark.
  kMshwmb = 0xbc2,  // Stack high water mark base.

  // Simulator specific CSRs. These are numbered 0x1800-0x18ff.
  kSimMode = 0x1800,
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__RISCV_CHERIOT_CSR_ENUM_H_
