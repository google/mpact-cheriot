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

#ifndef MPACT_CHERIOT__RISCV_CHERIOT_ENCODING_H_
#define MPACT_CHERIOT__RISCV_CHERIOT_ENCODING_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_bin_decoder.h"
#include "cheriot/riscv_cheriot_decoder.h"
#include "cheriot/riscv_cheriot_encoding_common.h"
#include "cheriot/riscv_cheriot_enums.h"

namespace mpact {
namespace sim {
namespace cheriot {
namespace isa32 {

using ::mpact::sim::cheriot::encoding::FormatEnum;

// This class provides the interface between the generated instruction decoder
// framework (which is agnostic of the actual bit representation of
// instructions) and the instruction representation. This class provides methods
// to return the opcode, source operands, and destination operands for
// instructions according to the operand fields in the encoding.
class RiscVCheriotEncoding : public RiscVCheriotEncodingCommon,
                             public RiscVCheriotEncodingBase {
 public:
  explicit RiscVCheriotEncoding(CheriotState *state);

  // Parses an instruction and determines the opcode.
  void ParseInstruction(uint32_t inst_word);

  // RiscV32 CHERIoT has a single slot type and single entry, so the following
  // methods ignore those parameters.

  // Returns the opcode in the current instruction representation.
  OpcodeEnum GetOpcode(SlotEnum, int) override { return opcode_; }

  // Returns the instruction format in the current instruction representation.
  FormatEnum GetFormat(SlotEnum, int) { return format_; }

  // There is no predicate, so return nullptr.
  PredicateOperandInterface *GetPredicate(SlotEnum, int, OpcodeEnum,
                                          PredOpEnum) override {
    return nullptr;
  }

  // Currently no resources modeled for RiscV CHERIoT.
  ResourceOperandInterface *GetSimpleResourceOperand(
      SlotEnum, int, OpcodeEnum, SimpleResourceVector &resource_vec,
      int end) override {
    return nullptr;
  }

  ResourceOperandInterface *GetComplexResourceOperand(
      SlotEnum, int, OpcodeEnum, ComplexResourceEnum resource, int begin,
      int end) override {
    return nullptr;
  }

  // The following method returns a source operand that corresponds to the
  // particular operand field.
  SourceOperandInterface *GetSource(SlotEnum, int, OpcodeEnum, SourceOpEnum op,
                                    int source_no) override;

  // The following method returns a destination operand that corresponds to the
  // particular operand field.
  DestinationOperandInterface *GetDestination(SlotEnum, int, OpcodeEnum,
                                              DestOpEnum op, int dest_no,
                                              int latency) override;
  // This method returns latency for any destination operand for which the
  // latency specifier in the .isa file is '*'. Since there are none, just
  // return 0.
  int GetLatency(SlotEnum, int, OpcodeEnum, DestOpEnum, int) override {
    return 0;
  }

 private:
  using SourceOpGetterMap =
      absl::flat_hash_map<int, absl::AnyInvocable<SourceOperandInterface *()>>;
  using DestOpGetterMap = absl::flat_hash_map<
      int, absl::AnyInvocable<DestinationOperandInterface *(int)>>;

  SourceOpGetterMap source_op_getters_;
  DestOpGetterMap dest_op_getters_;
  OpcodeEnum opcode_;
  FormatEnum format_;
};

}  // namespace isa32
}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__RISCV_CHERIOT_ENCODING_H_
