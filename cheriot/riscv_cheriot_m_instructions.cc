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

#include "cheriot/riscv_cheriot_m_instructions.h"

#include <limits>
#include <type_traits>

#include "cheriot/cheriot_register.h"
#include "cheriot/riscv_cheriot_instruction_helpers.h"
#include "mpact/sim/generic/type_helpers.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::WideType;

using RegType = CheriotRegister;
using UintReg = typename std::make_unsigned<RegType::ValueType>::type;
using WideUintReg = typename WideType<UintReg>::type;
using IntReg = typename std::make_signed<RegType::ValueType>::type;
using WideIntReg = typename WideType<IntReg>::type;

void MMul(Instruction* instruction) {
  RVCheriotBinaryOp<RegType, UintReg, WideIntReg>(
      instruction, [](WideIntReg a_wide, WideIntReg b_wide) {
        WideIntReg c_wide = a_wide * b_wide;
        return static_cast<UintReg>(
            (c_wide & std::numeric_limits<UintReg>::max()));
      });
}

void MMulh(Instruction* instruction) {
  RVCheriotBinaryOp<RegType, IntReg>(instruction,
                                     [](WideIntReg a_wide, WideIntReg b_wide) {
                                       WideIntReg c_wide = a_wide * b_wide;
                                       return static_cast<IntReg>(c_wide >> 32);
                                     });
}

void MMulhu(Instruction* instruction) {
  RVCheriotBinaryOp<RegType, UintReg>(
      instruction, [](WideUintReg a_wide, WideUintReg b_wide) {
        WideUintReg c_wide = a_wide * b_wide;
        return static_cast<UintReg>(c_wide >> 32);
      });
}

void MMulhsu(Instruction* instruction) {
  RVCheriotBinaryOp<RegType, UintReg, WideIntReg, WideUintReg>(
      instruction, [](WideIntReg a_wide, WideUintReg b_wide) {
        WideIntReg c_wide = a_wide * b_wide;
        return static_cast<IntReg>(c_wide >> 32);
      });
}

void MDiv(Instruction* instruction) {
  RVCheriotBinaryOp<RegType, IntReg>(
      instruction, [](IntReg a, IntReg b) -> IntReg {
        if (b == 0) return -1;
        if ((b == -1) && (a == std::numeric_limits<IntReg>::min())) {
          return std::numeric_limits<IntReg>::min();
        }
        return a / b;
      });
}

void MDivu(Instruction* instruction) {
  RVCheriotBinaryOp<RegType, UintReg>(
      instruction, [](UintReg a, UintReg b) -> UintReg {
        if (b == 0) return std::numeric_limits<UintReg>::max();
        return a / b;
      });
}

void MRem(Instruction* instruction) {
  RVCheriotBinaryOp<RegType, IntReg>(
      instruction, [](IntReg a, IntReg b) -> IntReg {
        if (b == 0) return a;
        if ((b == -1) && (a == std::numeric_limits<IntReg>::min())) {
          return 0;
        }
        return a % b;
      });
}

void MRemu(Instruction* instruction) {
  RVCheriotBinaryOp<RegType, UintReg>(instruction, [](UintReg a, UintReg b) {
    if (b == 0) return a;
    return a % b;
  });
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
