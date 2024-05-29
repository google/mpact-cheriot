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

#include "cheriot/cheriot_test_rig_decoder.h"

#include <cstdint>
#include <string>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_bin_decoder.h"
#include "cheriot/riscv_cheriot_decoder.h"
#include "cheriot/riscv_cheriot_encoding.h"
#include "cheriot/riscv_cheriot_enums.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/program_error.h"
#include "mpact/sim/generic/type_helpers.h"
#include "riscv//riscv_state.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::cheriot::encoding::FormatEnum;
using RV_EC = ::mpact::sim::riscv::ExceptionCode;
using ::mpact::sim::cheriot::isa32::OpcodeEnum;  // NOLINT: is used below.

CheriotTestRigDecoder::CheriotTestRigDecoder(CheriotState *state)
    : state_(state) {
  // Get a handle to the internal error in the program error controller.
  decode_error_ = state->program_error_controller()->GetProgramError(
      generic::ProgramErrorController::kInternalErrorName);
  // Allocate the isa factory class, the top level isa decoder instance, and
  // the encoding parser.
  cheriot_isa_factory_ = new CheriotTestRigIsaFactory();
  cheriot_isa_ =
      new isa32::RiscVCheriotInstructionSet(state, cheriot_isa_factory_);
  cheriot_encoding_ = new isa32::RiscVCheriotEncoding(state);
}

CheriotTestRigDecoder::~CheriotTestRigDecoder() {
  delete cheriot_isa_;
  delete cheriot_encoding_;
  delete cheriot_isa_factory_;
}

generic::Instruction *CheriotTestRigDecoder::DecodeInstruction(
    uint64_t address, uint32_t inst_word, DecodeInfo &decode_info) {
  uint16_t inst_word16 = static_cast<uint16_t>(inst_word & 0xffff);
  // First check that the address is aligned properly. If not, create and return
  // an instruction object that will raise an exception.
  if (address & 0x1) {
    auto *inst = new generic::Instruction(0, state_);
    inst->set_size(1);
    inst->SetDisassemblyString("Misaligned instruction address");
    inst->set_opcode(*isa32::OpcodeEnum::kNone);
    inst->set_address(address);
    inst->set_semantic_function([this](generic::Instruction *inst) {
      state_->Trap(/*is_interrupt*/ false, inst->address(),
                   *RV_EC::kInstructionAddressMisaligned, inst->address() ^ 0x1,
                   inst);
    });
    return inst;
  }
  // Parse the instruction in the encoding parser.
  cheriot_encoding_->ParseInstruction(inst_word);
  auto format = cheriot_encoding_->GetFormat(SlotEnum::kRiscv32Cheriot, 0);
  auto opcode = cheriot_encoding_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0);
  // Extract the numerical register specifies of the instruction.
  int rd = 0;
  int rs1 = 0;
  int rs2 = 0;
  switch (format) {
    case FormatEnum::kAType:
      // Atomic Instructions. All use rd, rs1, and rs2.
      rd = encoding::a_type::ExtractRd(inst_word);
      rs1 = encoding::a_type::ExtractRs1(inst_word);
      rs2 = encoding::a_type::ExtractRs2(inst_word);
      break;
    case FormatEnum::kBType:
      // 32 bit branch type instructions. All use rs1 and rs2.
      // beq, bne, blt, bge, bltu, bgeu
      rd = 0;
      rs1 = encoding::b_type::ExtractRs1(inst_word);
      rs2 = encoding::b_type::ExtractRs2(inst_word);
      break;
    case FormatEnum::kIType:  // 2 reg operands: rd and rs1.
      // addi, slti, sltiu, xori, ori, andi
      // cincaddrimm, cjalr, crj, lc, setboundsimm
      // lb, lh, lw, lb, lhu
      // csrrw/s/c, csrr[swc]_n[rw]
      rd = encoding::i_type::ExtractRd(inst_word);
      rs1 = encoding::i_type::ExtractRs1(inst_word);
      rs2 = 0;
      break;
    case FormatEnum::kI2Type:  // 1 register operand: rd.
      // cssr[wsc]i, csrr[wsc]_n[rw]
      rd = encoding::i2_type::ExtractRd(inst_word);
      rs1 = 0;
      rs2 = 0;
      break;
    case FormatEnum::kI5Type:  // 2 reg operands.
      rd = encoding::i5_type::ExtractRd(inst_word);
      rs1 = encoding::i5_type::ExtractRs1(inst_word);
      rs2 = 0;
      break;
    case FormatEnum::kJType: {  // Jump type - immediate.
      rd = encoding::j_type::ExtractRd(inst_word);
      rs1 = 0;
      rs2 = 0;
      break;
    }
      /*
    case FormatEnum::kR4Type:  // 4 reg operands, rd, rs1, rs3, and rs4.
      rd = encoding::r4_type::ExtractRd(inst_word);
      rs1 = encoding::r4_type::ExtractRs1(inst_word);
      rs2 = encoding::r4_type::ExtractRs2(inst_word);
      break;
      */
    case FormatEnum::kRType:  // 3 reg operands: rd, rs1, and rs2.
      rd = encoding::r_type::ExtractRd(inst_word);
      rs1 = encoding::r_type::ExtractRs1(inst_word);
      rs2 = encoding::r_type::ExtractRs2(inst_word);
      break;
    case FormatEnum::kR2Type:
      rd = encoding::r_type::ExtractRd(inst_word);
      rs1 = encoding::r_type::ExtractRs1(inst_word);
      rs2 = 0;
      break;
    case FormatEnum::kSType:  // 2 reg operands: rs1 and rs2.
      // sb, sh, sw, csc.
      rd = 0;
      rs1 = encoding::s_type::ExtractRs1(inst_word);
      rs2 = encoding::s_type::ExtractRs2(inst_word);
      break;
    case FormatEnum::kUType:
      // lui, cauicgp, cauipcc.
      rd = encoding::u_type::ExtractRd(inst_word);
      rs1 = 0;
      rs2 = 0;
      break;
    case FormatEnum::kCA:  // 3 reg operands: rd, rs1, and rs2.
      // csub, cxor, cor, cand.
      rd = encoding::c_a::ExtractRd(inst_word16);
      rs1 = encoding::c_a::ExtractRs1(inst_word16);
      rs2 = encoding::c_a::ExtractRs2(inst_word16);
      break;
    case FormatEnum::kCSH:  // rs1 is source and dest.
      // csrli, csrai, candi.
      rd = encoding::c_s_h::ExtractRd(inst_word16);
      rs1 = encoding::c_s_h::ExtractRs1(inst_word16);
      rs2 = 0;
      break;
    case FormatEnum::kCB:  // rs1 is source.
      // cbeqz, cbnez.
      rd = 0;
      rs1 = encoding::c_b::ExtractRs1(inst_word16);
      rs2 = 0;
      break;
    case FormatEnum::kCI:  // 2 reg operands: rd, rs1.
      // cnop, caddi, cli, caddi16sp, clui, cslli, clwsp, cldsp.
      rd = encoding::c_i::ExtractRd(inst_word16);
      if ((opcode == OpcodeEnum::kClwsp) || (opcode == OpcodeEnum::kCldsp)) {
        rs1 = 2;
      } else {
        rs1 = encoding::c_i::ExtractRs1(inst_word16);
      }
      rs2 = 0;
      break;
    case FormatEnum::kCIW:  // 1 reg operand: rd.
      // caddi4spn.
      rd = encoding::c_i_w::ExtractRd(inst_word16);
      rs1 = 2;
      rs2 = 0;
      break;
    case FormatEnum::kCJ:  // Depends on opcode. jal/jalr use x1.
      // cj, cjal.
      rd = opcode == OpcodeEnum::kCheriotCj ? 0 : 1;
      rs1 = 0;
      rs2 = 0;
      break;
    case FormatEnum::kCL:  // 2 reg operands: cl_rs1 and cl_rd.
      // clw, cld.
      rd = encoding::c_l::ExtractRd(inst_word16);
      rs1 = encoding::c_l::ExtractRs1(inst_word16);
      rs2 = 0;
      break;
    case FormatEnum::kCR:  // 3 reg operands: rd(s), crs2, and rd(d).
      // cmv, cebreak, cadd, cheriot_cjr, cheriot_cjalr.
      switch (opcode) {
        case OpcodeEnum::kCmv:
          rd = encoding::c_r::ExtractRd(inst_word16);
          rs1 = 0;
          rs2 = encoding::c_r::ExtractRs2(inst_word16);
          break;
        case OpcodeEnum::kCebreak:
          rd = 0;
          rs1 = 0;
          rs2 = 0;
          break;
        case OpcodeEnum::kCadd:
          rd = encoding::c_r::ExtractRd(inst_word16);
          rs1 = encoding::c_r::ExtractRs1(inst_word16);
          rs2 = encoding::c_r::ExtractRs2(inst_word16);
          break;
        case OpcodeEnum::kCheriotCjr:
          rd = 0;
          rs1 = encoding::c_r::ExtractRs1(inst_word16);
          rs2 = 0;
          break;
        case OpcodeEnum::kCheriotCjalrCra:
          rd = 1;
          rs1 = encoding::c_r::ExtractRs1(inst_word16);
          rs2 = 0;
          break;
        default:
          break;
      }
      break;
    case FormatEnum::kCS:  // 2 reg operands: rs1p and rs2p.
      // sw, sd.
      rd = 0;
      rs1 = encoding::c_s::ExtractRs1(inst_word16);
      rs2 = encoding::c_s::ExtractRs2(inst_word16);
      break;
    case FormatEnum::kCSS:  // 1 reg operand: rs2.
      // cswsp, csdsp
      rd = 0;
      rs1 = 0;
      rs2 = encoding::c_s_s::ExtractRs2(inst_word16);
      break;
    default:
      break;
  }
  // Call the isa decoder to obtain a new instruction object for the instruction
  // word that was parsed above.
  auto *instruction = cheriot_isa_->Decode(address, cheriot_encoding_);
  // For these formats, sail does not populate the rs1/rs2 address fields.
  if (format == FormatEnum::kIType || format == FormatEnum::kI5Type ||
      format == FormatEnum::kR2Type || format == FormatEnum::kCB ||
      format == FormatEnum::kCSH || format == FormatEnum::kCIW ||
      format == FormatEnum::kCI) {
    rs1 = 0;
  }
  if (format == FormatEnum::kRType || format == FormatEnum::kBType ||
      format == FormatEnum::kCR || format == FormatEnum::kCA ||
      format == FormatEnum::kSType) {
    rs1 = 0;
    rs2 = 0;
  }
  if (opcode == OpcodeEnum::kCslli) {
    rs1 = 0;
  }
  if ((opcode == OpcodeEnum::kHint) || (opcode == OpcodeEnum::kChint)) {
    rs1 = 0;
    rs2 = 0;
    rd = 0;
  }
  decode_info.rd = rd;
  decode_info.rs1 = rs1;
  decode_info.rs2 = rs2;
  return instruction;
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
