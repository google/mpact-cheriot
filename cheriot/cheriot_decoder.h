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

#ifndef MPACT_CHERIOT_CHERIOT_DECODER_H_
#define MPACT_CHERIOT_CHERIOT_DECODER_H_

#include <cstdint>
#include <memory>

#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_decoder.h"
#include "cheriot/riscv_cheriot_encoding.h"
#include "cheriot/riscv_cheriot_enums.h"
#include "mpact/sim/generic/arch_state.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/generic/decoder_interface.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/util/memory/memory_interface.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::ArchState;

// This is the factory class needed by the generated decoder. It is responsible
// for creating the decoder for each slot instance. Since the riscv architecture
// only has a single slot, it's a pretty simple class.
class CheriotIsaFactory : public isa32::RiscVCheriotInstructionSetFactory {
 public:
  std::unique_ptr<isa32::Riscv32CheriotSlot> CreateRiscv32CheriotSlot(
      ArchState *state) override {
    return std::make_unique<isa32::Riscv32CheriotSlot>(state);
  }
};

// This class implements the generic DecoderInterface and provides a bridge
// to the (isa specific) generated decoder classes.
class CheriotDecoder : public generic::DecoderInterface {
 public:
  using SlotEnum = isa32::SlotEnum;
  using OpcodeEnum = isa32::OpcodeEnum;

  CheriotDecoder(CheriotState *state, util::MemoryInterface *memory);
  CheriotDecoder() = delete;
  ~CheriotDecoder() override;

  // This will always return a valid instruction that can be executed. In the
  // case of a decode error, the semantic function in the instruction object
  // instance will raise an internal simulator error when executed.
  generic::Instruction *DecodeInstruction(uint64_t address) override;

  // Return the number of opcodes supported by this decoder.
  int GetNumOpcodes() const override {
    return static_cast<int>(OpcodeEnum::kPastMaxValue);
  }
  // Return the name of the opcode at the given index.
  const char *GetOpcodeName(int index) const override {
    return isa32::kOpcodeNames[index];
  }

  // Getter.
  isa32::RiscVCheriotEncoding *cheriot_encoding() const {
    return cheriot_encoding_;
  }

 private:
  CheriotState *state_;
  util::MemoryInterface *memory_;
  generic::DataBufferFactory db_factory_;
  generic::DataBuffer *inst_db_;
  isa32::RiscVCheriotEncoding *cheriot_encoding_;
  isa32::RiscVCheriotInstructionSetFactory *cheriot_isa_factory_;
  isa32::RiscVCheriotInstructionSet *cheriot_isa_;
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT_CHERIOT_DECODER_H_
