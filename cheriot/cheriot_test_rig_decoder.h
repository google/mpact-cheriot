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

#ifndef MPACT_CHERIOT__CHERIOT_TEST_RIG_DECODER_H_
#define MPACT_CHERIOT__CHERIOT_TEST_RIG_DECODER_H_

#include <cstdint>
#include <memory>

#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_decoder.h"
#include "cheriot/riscv_cheriot_encoding.h"
#include "cheriot/riscv_cheriot_enums.h"
#include "mpact/sim/generic/arch_state.h"
#include "mpact/sim/generic/decoder_interface.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/program_error.h"

// This is a specialized instruction decoder class for CherIoT TestRIG. It is
// specialized due to the fact that the instruction words for TestRIG are
// supplied from a socket, and not from an image stored in memory. The decode
// function has also been extended to capture register source and destination
// numbers.

namespace mpact {
namespace sim {
namespace cheriot {

class CheriotTestRigIsaFactory
    : public isa32::RiscVCheriotInstructionSetFactory {
 public:
  std::unique_ptr<isa32::Riscv32CheriotSlot> CreateRiscv32CheriotSlot(
      generic::ArchState* state) override {
    return std::make_unique<isa32::Riscv32CheriotSlot>(state);
  }
};

class CheriotTestRigDecoder {
 public:
  using SlotEnum = isa32::SlotEnum;
  using OpcodeEnum = isa32::OpcodeEnum;

  struct DecodeInfo {
    int rd;
    int rs1;
    int rs2;
  };

  explicit CheriotTestRigDecoder(CheriotState* state);
  CheriotTestRigDecoder() = delete;
  CheriotTestRigDecoder(const CheriotTestRigDecoder&) = delete;
  CheriotTestRigDecoder& operator=(const CheriotTestRigDecoder&) = delete;
  virtual ~CheriotTestRigDecoder();

  // Decode a single instruction and fill in decode time information in the
  // TestRIG execution packet.
  generic::Instruction* DecodeInstruction(uint64_t address, uint32_t inst_word,
                                          DecodeInfo& decode_info);

 private:
  CheriotState* state_;
  std::unique_ptr<generic::ProgramError> decode_error_;
  isa32::RiscVCheriotEncoding* cheriot_encoding_;
  isa32::RiscVCheriotInstructionSetFactory* cheriot_isa_factory_;
  isa32::RiscVCheriotInstructionSet* cheriot_isa_;
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__CHERIOT_TEST_RIG_DECODER_H_
