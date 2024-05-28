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

#ifndef MPACT_CHERIOT__RISCV_CHERIOT_ZICSR_INSTRUCTIONS_H_
#define MPACT_CHERIOT__RISCV_CHERIOT_ZICSR_INSTRUCTIONS_H_

#include "mpact/sim/generic/instruction.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::Instruction;

void RiscVZiCsrrw(Instruction *instruction);
void RiscVZiCsrrs(Instruction *instruction);
void RiscVZiCsrrc(Instruction *instruction);
void RiscVZiCsrrwNr(Instruction *instruction);
void RiscVZiCsrrNw(Instruction *instruction);

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__RISCV_CHERIOT_ZICSR_INSTRUCTIONS_H_
