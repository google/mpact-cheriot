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

#include "cheriot/cheriot_rvv_decoder.h"

#include <cstdint>
#include <string>

#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_rvv_decoder.h"
#include "cheriot/riscv_cheriot_rvv_encoding.h"
#include "cheriot/riscv_cheriot_rvv_enums.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/type_helpers.h"
#include "mpact/sim/util/memory/memory_interface.h"
#include "riscv//riscv_state.h"

namespace mpact {
namespace sim {
namespace cheriot {

using RV_EC = ::mpact::sim::riscv::ExceptionCode;

using ::mpact::sim::generic::operator*;  // NOLINT: is used below (clang error).

CheriotRVVDecoder::CheriotRVVDecoder(CheriotState* state,
                                     util::MemoryInterface* memory)
    : state_(state), memory_(memory) {
  // Need a data buffer to load instructions from memory. Allocate a single
  // buffer that can be reused for each instruction word.
  inst_db_ = db_factory_.Allocate<uint32_t>(1);
  // Allocate the isa factory class, the top level isa decoder instance, and
  // the encoding parser.
  cheriot_rvv_isa_factory_ = new CheriotRVVIsaFactory();
  cheriot_rvv_isa_ = new isa32_rvv::RiscVCheriotRVVInstructionSet(
      state, cheriot_rvv_isa_factory_);
  cheriot_rvv_encoding_ = new isa32_rvv::RiscVCheriotRVVEncoding(state);
}

CheriotRVVDecoder::~CheriotRVVDecoder() {
  delete cheriot_rvv_isa_;
  delete cheriot_rvv_isa_factory_;
  delete cheriot_rvv_encoding_;
  inst_db_->DecRef();
}

generic::Instruction* CheriotRVVDecoder::DecodeInstruction(uint64_t address) {
  // First check that the address is aligned properly. If not, create and return
  // an instruction object that will raise an exception.
  if (address & 0x1) {
    auto* inst = new generic::Instruction(0, state_);
    inst->set_size(1);
    inst->SetDisassemblyString("Misaligned instruction address");
    inst->set_opcode(*isa32_rvv::OpcodeEnum::kNone);
    inst->set_address(address);
    inst->set_semantic_function([this](generic::Instruction* inst) {
      state_->Trap(/*is_interrupt*/ false, inst->address(),
                   *RV_EC::kInstructionAddressMisaligned, inst->address() ^ 0x1,
                   inst);
    });
    return inst;
  }

  // If the address is greater than the max address, return an instruction
  // that will raise an exception.
  if (address > state_->max_physical_address()) {
    auto* inst = new generic::Instruction(0, state_);
    inst->set_size(0);
    inst->SetDisassemblyString("Instruction access fault");
    inst->set_opcode(*isa32_rvv::OpcodeEnum::kNone);
    inst->set_address(address);
    inst->set_semantic_function([this](generic::Instruction* inst) {
      state_->Trap(/*is_interrupt*/ false, inst->address(),
                   *RV_EC::kInstructionAccessFault, inst->address(), nullptr);
    });
    return inst;
  }

  // Read the instruction word from memory and parse it in the encoding parser.
  memory_->Load(address, inst_db_, nullptr, nullptr);
  uint32_t iword = inst_db_->Get<uint32_t>(0);
  cheriot_rvv_encoding_->ParseInstruction(iword);

  // Call the isa decoder to obtain a new instruction object for the instruction
  // word that was parsed above.
  auto* instruction = cheriot_rvv_isa_->Decode(address, cheriot_rvv_encoding_);
  return instruction;
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
