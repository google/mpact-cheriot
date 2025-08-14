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

#ifndef MPACT_CHERIOT_CHERIOT_VECTOR_TRUE_OPERAND_H_
#define MPACT_CHERIOT_CHERIOT_VECTOR_TRUE_OPERAND_H_

#include <cstdint>
#include <string>

#include "riscv//riscv_register.h"

// File defines a Cheriot version of the RV32VectorTrueOperand registers.

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::riscv::RV32VectorSourceOperand;

class CheriotState;

class CheriotVectorTrueOperand : public RV32VectorSourceOperand {
 public:
  explicit CheriotVectorTrueOperand(CheriotState* state);

  CheriotVectorTrueOperand() = delete;
  bool AsBool(int) final { return true; }
  int8_t AsInt8(int) final { return 0xff; }
  uint8_t AsUint8(int) final { return 0xff; }
  int16_t AsInt16(int) final { return 0xffff; }
  uint16_t AsUint16(int) final { return 0xffff; }
  int32_t AsInt32(int) final { return 0xffff'ffff; }
  uint32_t AsUint32(int) final { return 0xffff'ffff; }
  int64_t AsInt64(int) final { return 0xffff'ffff'ffff'ffffULL; }
  uint64_t AsUint64(int) final { return 0xffff'ffff'ffff'ffffLL; }
  std::string AsString() const override { return ""; }

 private:
  static constexpr char kName[] = "__VectorTrue__";
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT_CHERIOT_VECTOR_TRUE_OPERAND_H_
