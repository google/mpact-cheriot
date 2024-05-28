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

#include "cheriot/riscv_cheriot_priv_instructions.h"

#include <cstdint>

#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "riscv//riscv_xstatus.h"

namespace mpact {
namespace sim {
namespace cheriot {

using UIntReg = uint32_t;

void RiscVPrivMRet(const Instruction *inst) {
  auto *state = static_cast<CheriotState *>(inst->state());
  state->pcc()->CopyFrom(*state->mepcc());
  state->set_branch(true);
  auto *mstatus = state->mstatus();
  // Set mstatus:mie to the value of mstatus:mpie.
  mstatus->set_mie(mstatus->mpie());
  // Set mstatus:mpie to 1.
  mstatus->set_mpie(1);
  mstatus->set_mpp(*PrivilegeMode::kMachine);
  state->SignalReturnFromInterrupt();
  mstatus->Submit();
}

void RiscVPrivWfi(const Instruction *inst) {
  // WFI is treated as a no-op, unless the user sets a callback.
  auto *state = static_cast<CheriotState *>(inst->state());
  state->WFI(inst);
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
