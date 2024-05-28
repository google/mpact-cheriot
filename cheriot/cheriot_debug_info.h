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

#ifndef MPACT_CHERIOT__CHERIOT_DEBUG_INFO_H_
#define MPACT_CHERIOT__CHERIOT_DEBUG_INFO_H_

#include <cstdint>
#include <string>

#include "absl/container/flat_hash_map.h"

namespace mpact {
namespace sim {
namespace cheriot {

// Enumeration of the RiscV debug ids for accessible registers.
enum class DebugRegisterEnum : uint32_t {
  // CSRs.
  // Program counter.
  kPc = 0x07b1,

  // Capability registers.
  kC0 = 0x1000,
  kC1,
  kC2,
  kC3,
  kC4,
  kC5,
  kC6,
  kC7,
  kC8,
  kC9,
  kC10,
  kC11,
  kC12,
  kC13,
  kC14,
  kC15,
  kC16,
  kC17,
  kC18,
  kC19,
  kC20,
  kC21,
  kC22,
  kC23,
  kC24,
  kC25,
  kC26,
  kC27,
  kC28,
  kC29,
  kC30,
  kC31,

  // Floating point registers.
  kF0 = 0x1020,
  kF1,
  kF2,
  kF3,
  kF4,
  kF5,
  kF6,
  kF7,
  kF8,
  kF9,
  kF10,
  kF11,
  kF12,
  kF13,
  kF14,
  kF15,
  kF16,
  kF17,
  kF18,
  kF19,
  kF20,
  kF21,
  kF22,
  kF23,
  kF24,
  kF25,
  kF26,
  kF27,
  kF28,
  kF29,
  kF30,
  kF31,
};

// Singleton class used to store RiscV debug register ids.
class CheriotDebugInfo {
 public:
  using DebugRegisterMap = absl::flat_hash_map<uint32_t, std::string>;

  static CheriotDebugInfo* Instance();

  const DebugRegisterMap& debug_register_map() const {
    return debug_register_map_;
  }

 private:
  CheriotDebugInfo();
  DebugRegisterMap debug_register_map_;
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__CHERIOT_DEBUG_INFO_H_
