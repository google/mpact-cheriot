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

#ifndef MPACT_CHERIOT__RISCV_CHERIOT_PRIV_INSTRUCTIONS_H_
#define MPACT_CHERIOT__RISCV_CHERIOT_PRIV_INSTRUCTIONS_H_

#include "mpact/sim/generic/instruction.h"

// This file contains the declarations of the instruction semantic functions
// for the RiscV 32i privileged instructions.

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::Instruction;

void RiscVPrivMRet(const Instruction *inst);

void RiscVPrivWfi(const Instruction *inst);
void RiscVPrivSFenceVmaZZ(const Instruction *inst);
void RiscVPrivSFenceVmaZN(const Instruction *inst);
void RiscVPrivSFenceVmaNZ(const Instruction *inst);
void RiscVPrivSFenceVmaNN(const Instruction *inst);

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__RISCV_CHERIOT_PRIV_INSTRUCTIONS_H_
