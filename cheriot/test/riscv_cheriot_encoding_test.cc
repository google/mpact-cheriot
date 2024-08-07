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

#include "cheriot/riscv_cheriot_encoding.h"

#include <cstdint>

#include "cheriot/cheriot_state.h"
#include "cheriot/riscv_cheriot_enums.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/type_helpers.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"

namespace {

using ::mpact::sim::cheriot::CheriotState;
using ::mpact::sim::cheriot::isa32::kOpcodeNames;
using ::mpact::sim::cheriot::isa32::RiscVCheriotEncoding;
using ::mpact::sim::generic::operator*;  // NOLINT: is used below (clang error).
using ::mpact::sim::util::TaggedFlatDemandMemory;
using SlotEnum = mpact::sim::cheriot::isa32::SlotEnum;
using OpcodeEnum = mpact::sim::cheriot::isa32::OpcodeEnum;

// Constexpr for opcodes for RV32 CHERIoT instructions grouped by isa group.

// RV32I
constexpr uint32_t kLui = 0b0000000000000000000000000'0110111;
constexpr uint32_t kBeq = 0b0000000'00000'00000'000'00000'1100011;
constexpr uint32_t kBne = 0b0000000'00000'00000'001'00000'1100011;
constexpr uint32_t kBlt = 0b0000000'00000'00000'100'00000'1100011;
constexpr uint32_t kBge = 0b0000000'00000'00000'101'00000'1100011;
constexpr uint32_t kBltu = 0b0000000'00000'00000'110'00000'1100011;
constexpr uint32_t kBgeu = 0b0000000'00000'00000'111'00000'1100011;
constexpr uint32_t kLb = 0b000000000000'00000'000'00000'0000011;
constexpr uint32_t kLh = 0b000000000000'00000'001'00000'0000011;
constexpr uint32_t kLw = 0b000000000000'00000'010'00000'0000011;
constexpr uint32_t kLbu = 0b000000000000'00000'100'00000'0000011;
constexpr uint32_t kLhu = 0b000000000000'00000'101'00000'0000011;
constexpr uint32_t kSb = 0b0000000'00000'00000'000'00000'0100011;
constexpr uint32_t kSh = 0b0000000'00000'00000'001'00000'0100011;
constexpr uint32_t kSw = 0b0000000'00000'00000'010'00000'0100011;
constexpr uint32_t kAddi = 0b000000000000'00000'000'00000'0010011;
constexpr uint32_t kSlti = 0b000000000000'00000'010'00000'0010011;
constexpr uint32_t kSltiu = 0b000000000000'00000'011'00000'0010011;
constexpr uint32_t kXori = 0b000000000000'00000'100'00000'0010011;
constexpr uint32_t kOri = 0b000000000000'00000'110'00000'0010011;
constexpr uint32_t kAndi = 0b000000000000'00000'111'00000'0010011;
constexpr uint32_t kSlli = 0b0000000'00000'00000'001'00000'0010011;
constexpr uint32_t kSrli = 0b0000000'00000'00000'101'00000'0010011;
constexpr uint32_t kSrai = 0b0100000'00000'00000'101'00000'0010011;
constexpr uint32_t kAdd = 0b0000000'00000'00000'000'00000'0110011;
constexpr uint32_t kSub = 0b0100000'00000'00000'000'00000'0110011;
constexpr uint32_t kSll = 0b0000000'00000'00000'001'00000'0110011;
constexpr uint32_t kSlt = 0b0000000'00000'00000'010'00000'0110011;
constexpr uint32_t kSltu = 0b0000000'00000'00000'011'00000'0110011;
constexpr uint32_t kXor = 0b0000000'00000'00000'100'00000'0110011;
constexpr uint32_t kSrl = 0b0000000'00000'00000'101'00000'0110011;
constexpr uint32_t kSra = 0b0100000'00000'00000'101'00000'0110011;
constexpr uint32_t kOr = 0b0000000'00000'00000'110'00000'0110011;
constexpr uint32_t kAnd = 0b0000000'00000'00000'111'00000'0110011;
// constexpr uint32_t kFence = 0b000000000000'00000'000'00000'0001111;
constexpr uint32_t kEcall = 0b000000000000'00000'000'00000'1110011;
constexpr uint32_t kEbreak = 0b000000000001'00000'000'00000'1110011;
// RV32 CHERIoT
constexpr uint32_t kCheriotAuicgp = 0b000000000000'00000'000'00000'1111011;
constexpr uint32_t kCheriotAuipcc = 0b000000000000'00000'000'00000'0010111;
constexpr uint32_t kCheriotAndperm = 0b0001101'00000'00000'000'00000'1011011;
constexpr uint32_t kCheriotCleartag = 0b1111111'01011'00000'000'00000'1011011;
constexpr uint32_t kCheriotGetaddr = 0b1111111'01111'00000'000'00000'1011011;
constexpr uint32_t kCheriotGetbase = 0b1111111'00010'00000'000'00000'1011011;
constexpr uint32_t kCheriotGethigh = 0b1111111'10111'00000'000'00000'1011011;
constexpr uint32_t kCheriotGetlen = 0b1111111'00011'00000'000'00000'1011011;
constexpr uint32_t kCheriotGetperm = 0b1111111'00000'00000'000'00000'1011011;
constexpr uint32_t kCheriotGettag = 0b1111111'00100'00000'000'00000'1011011;
constexpr uint32_t kCheriotGettop = 0b1111111'11000'00000'000'00000'1011011;
constexpr uint32_t kCheriotGettype = 0b1111111'00001'00000'000'00000'1011011;
constexpr uint32_t kCheriotIncaddr = 0b0010001'00000'00000'000'00000'1011011;
constexpr uint32_t kCheriotIncaddrimm = 0b000000000000'00000'001'00000'1011011;
constexpr uint32_t kCheriotJal = 0b00000000000000000000'00000'1101111;
constexpr uint32_t kCheriotJalr = 0b00000000000'00000'000'00000'1100111;
constexpr uint32_t kCheriotLc = 0b000000000000'00000'011'00000'0000011;
constexpr uint32_t kCheriotMove = 0b1111111'01010'00000'000'00000'1011011;
constexpr uint32_t kCheriotRepresentableAlignmentMask =
    0b1111111'01001'00000'000'00000'1011011;
constexpr uint32_t kCheriotRoundRepresentableLength =
    0b1111111'01000'00000'000'00000'1011011;
constexpr uint32_t kCheriotSc = 0b0000000'00000'00000'011'00000'0100011;
constexpr uint32_t kCheriotSeal = 0b0001011'00000'00000'000'00000'1011011;
constexpr uint32_t kCheriotSetaddr = 0b0010000'00000'00000'000'00000'1011011;
constexpr uint32_t kCheriotSetbounds = 0b0001000'00000'00000'000'00000'1011011;
constexpr uint32_t kCheriotSetboundsexact =
    0b0001001'00000'00000'000'00000'1011011;
constexpr uint32_t kCheriotSetboundsimm =
    0b000000000000'00000'010'00000'1011011;
constexpr uint32_t kCheriotSetequalexact =
    0b0100001'00000'00000'000'00000'1011011;
constexpr uint32_t kCheriotSethigh = 0b0010110'00000'00000'000'00000'1011011;
constexpr uint32_t kCheriotSpecialrw = 0b0000001'00000'00000'000'00000'1011011;
constexpr uint32_t kCheriotSub = 0b0010100'00000'00000'000'00000'1011011;
constexpr uint32_t kCheriotTestsubset = 0b0100000'00000'00000'000'00000'1011011;
constexpr uint32_t kCheriotUnseal = 0b0001100'00000'00000'000'00000'1011011;
// RV32 Zifencei
// constexpr uint32_t kFencei = 0b000000000000'00000'001'00000'0001111;
// RV32 Zicsr
constexpr uint32_t kCsrw = 0b000000000000'00000'001'00000'1110011;
constexpr uint32_t kCsrs = 0b000000000000'00000'010'00000'1110011;
constexpr uint32_t kCsrc = 0b000000000000'00000'011'00000'1110011;
constexpr uint32_t kCsrwi = 0b000000000000'00000'101'00000'1110011;
constexpr uint32_t kCsrsi = 0b000000000000'00000'110'00000'1110011;
constexpr uint32_t kCsrci = 0b000000000000'00000'111'00000'1110011;
// RV32M
constexpr uint32_t kMul = 0b0000001'00000'00000'000'00000'0110011;
constexpr uint32_t kMulh = 0b0000001'00000'00000'001'00000'0110011;
constexpr uint32_t kMulhsu = 0b0000001'00000'00000'010'00000'0110011;
constexpr uint32_t kMulhu = 0b0000001'00000'00000'011'00000'0110011;
constexpr uint32_t kDiv = 0b0000001'00000'00000'100'00000'0110011;
constexpr uint32_t kDivu = 0b0000001'00000'00000'101'00000'0110011;
constexpr uint32_t kRem = 0b0000001'00000'00000'110'00000'0110011;
constexpr uint32_t kRemu = 0b0000001'00000'00000'111'00000'0110011;
// RV32A
// constexpr uint32_t kLrw = 0b00010'0'0'00000'00000'010'00000'0101111;
// constexpr uint32_t kScw = 0b00011'0'0'00000'00000'010'00000'0101111;
// constexpr uint32_t kAmoswapw = 0b00001'0'0'00000'00000'010'00000'0101111;
// constexpr uint32_t kAmoaddw = 0b00000'0'0'00000'00000'010'00000'0101111;
// constexpr uint32_t kAmoxorw = 0b00100'0'0'00000'00000'010'00000'0101111;
// constexpr uint32_t kAmoandw = 0b01100'0'0'00000'00000'010'00000'0101111;
// constexpr uint32_t kAmoorw = 0b01000'0'0'00000'00000'010'00000'0101111;
// constexpr uint32_t kAmominw = 0b10000'0'0'00000'00000'010'00000'0101111;
// constexpr uint32_t kAmomaxw = 0b10100'0'0'00000'00000'010'00000'0101111;
// constexpr uint32_t kAmominuw = 0b11000'0'0'00000'00000'010'00000'0101111;
// constexpr uint32_t kAmomaxuw = 0b11100'0'0'00000'00000'010'00000'0101111;
// RV32C
constexpr uint32_t kClwsp = 0b010'0'00000'00000'10;
constexpr uint32_t kCldsp = 0b011'0'00000'00000'10;
// constexpr uint32_t kCdldsp = 0b001'0'00000'00000'10;
constexpr uint32_t kCswsp = 0b110'000000'00000'10;
constexpr uint32_t kCsdsp = 0b111'000000'00000'10;
// constexpr uint32_t kCdsdsp = 0b101'000000'00000'10;
constexpr uint32_t kClw = 0b010'000'000'00'000'00;
constexpr uint32_t kCld = 0b011'000'000'00'000'00;
// constexpr uint32_t kCdld = 0b001'000'000'00'000'00;
constexpr uint32_t kCsw = 0b110'000'000'00'000'00;
constexpr uint32_t kCsd = 0b111'000'000'00'000'00;
// constexpr uint32_t kCdsd = 0b101'000'000'00'000'00;
constexpr uint32_t kCheriotCj = 0b101'00000000000'01;
constexpr uint32_t kCheriotCjal = 0b001'00000000000'01;
constexpr uint32_t kCheriotCjr = 0b100'0'00000'00000'10;
constexpr uint32_t kCheriotCjalr = 0b100'1'00000'00000'10;
constexpr uint32_t kCbeqz = 0b110'000'000'00000'01;
constexpr uint32_t kCbnez = 0b111'000'000'00000'01;
constexpr uint32_t kCli = 0b010'0'00000'00000'01;
constexpr uint32_t kClui = 0b011'0'00000'00000'01;
constexpr uint32_t kCaddi = 0b000'0'00000'00000'01;
constexpr uint32_t kCaddi16sp = 0b011'0'00010'00000'01;
constexpr uint32_t kCaddi4spn = 0b000'00000000'000'00;
constexpr uint32_t kCslli = 0b000'0'00000'00000'10;
constexpr uint32_t kCsrli = 0b100'0'00'000'00000'01;
constexpr uint32_t kCsrai = 0b100'0'01'000'00000'01;
constexpr uint32_t kCandi = 0b100'0'10'000'00000'01;
constexpr uint32_t kCmv = 0b100'0'00000'00000'10;
constexpr uint32_t kCadd = 0b100'1'00000'00000'10;
constexpr uint32_t kCand = 0b100'0'11'000'11'000'01;
constexpr uint32_t kCor = 0b100'0'11'000'10'000'01;
constexpr uint32_t kCxor = 0b100'0'11'000'01'000'01;
constexpr uint32_t kCSub = 0b100'0'11'000'00'000'01;
constexpr uint32_t kCnop = 0b000'0'00000'00000'01;
constexpr uint32_t kCebreak = 0b100'1'00000'00000'10;
// constexpr uint32_t kMret = 0b001'1000'00010'00000'000'00000'111'0011;
// constexpr uint32_t kWfi = 0b000'1000'00101'00000'000'00000'111'0011;
// constexpr uint32_t kSfenceVmaZz = 0b000'1001'00000'00000'000'00000'111'0011;
// constexpr uint32_t kSfenceVmaZn = 0b000'1001'00001'00000'000'00000'111'0011;
// constexpr uint32_t kSfenceVmaNz = 0b000'1001'00000'00001'000'00000'111'0011;
// constexpr uint32_t kSfenceVmaNn = 0b000'1001'00001'00001'000'00000'111'0011;

class RiscVCheriotEncodingTest : public testing::Test {
 protected:
  RiscVCheriotEncodingTest() {
    mem_ = new TaggedFlatDemandMemory(8);
    state_ = new CheriotState("test", mem_, nullptr);
    enc_ = new RiscVCheriotEncoding(state_);
  }
  ~RiscVCheriotEncodingTest() override {
    delete enc_;
    delete mem_;
    delete state_;
  }

  TaggedFlatDemandMemory *mem_;
  CheriotState *state_;
  RiscVCheriotEncoding *enc_;
};

constexpr int kRdValue = 1;

static uint32_t SetRd(uint32_t iword, uint32_t rdval) {
  return (iword | ((rdval & 0x1f) << 7));
}

static uint32_t SetRs1(uint32_t iword, uint32_t rsval) {
  return (iword | ((rsval & 0x1f) << 15));
}

static uint32_t SetRs2(uint32_t iword, uint32_t rsval) {
  return (iword | ((rsval & 0x1f) << 20));
}

static uint32_t Set16Rd(uint32_t iword, uint32_t val) {
  return (iword | ((val & 0x1f) << 7));
}

static uint32_t Set16Rs2(uint32_t iword, uint32_t val) {
  return (iword | ((val & 0x1f) << 2));
}

TEST_F(RiscVCheriotEncodingTest, RV32IOpcodes) {
  enc_->ParseInstruction(SetRd(kLui, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kLui);
  enc_->ParseInstruction(SetRd(kCheriotJal, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotJal);
  enc_->ParseInstruction(SetRd(kCheriotJalr, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotJalrCra);
  enc_->ParseInstruction(kBeq);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kBeq);
  enc_->ParseInstruction(kBne);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kBne);
  enc_->ParseInstruction(kBlt);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kBlt);
  enc_->ParseInstruction(kBge);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kBge);
  enc_->ParseInstruction(kBltu);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kBltu);
  enc_->ParseInstruction(kBgeu);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kBgeu);
  enc_->ParseInstruction(SetRd(kLb, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kLb);
  enc_->ParseInstruction(SetRd(kLh, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kLh);
  enc_->ParseInstruction(SetRd(kLw, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kLw);
  enc_->ParseInstruction(SetRd(kLbu, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kLbu);
  enc_->ParseInstruction(SetRd(kLhu, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kLhu);
  enc_->ParseInstruction(SetRd(kSb, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSb);
  enc_->ParseInstruction(SetRd(kSh, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSh);
  enc_->ParseInstruction(SetRd(kSw, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSw);
  enc_->ParseInstruction(SetRd(kAddi, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kAddi);
  enc_->ParseInstruction(SetRd(kSlti, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSlti);
  enc_->ParseInstruction(SetRd(kSltiu, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSltiu);
  enc_->ParseInstruction(SetRd(kXori, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kXori);
  enc_->ParseInstruction(SetRd(kOri, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kOri);
  enc_->ParseInstruction(SetRd(kAndi, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kAndi);
  enc_->ParseInstruction(SetRd(kSlli, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSlli);
  enc_->ParseInstruction(SetRd(kSrli, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSrli);
  enc_->ParseInstruction(SetRd(kSrai, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSrai);
  enc_->ParseInstruction(SetRd(kAdd, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kAdd);
  enc_->ParseInstruction(SetRd(kSub, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSub);
  enc_->ParseInstruction(SetRd(kSll, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSll);
  enc_->ParseInstruction(SetRd(kSlt, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSlt);
  enc_->ParseInstruction(SetRd(kSltu, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSltu);
  enc_->ParseInstruction(SetRd(kXor, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kXor);
  enc_->ParseInstruction(SetRd(kSrl, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSrl);
  enc_->ParseInstruction(SetRd(kSra, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kSra);
  enc_->ParseInstruction(SetRd(kOr, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kOr);
  enc_->ParseInstruction(SetRd(kAnd, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kAnd);
  // enc_->ParseInstruction(SetSucc(SetPred(kFence, kPredValue), kSuccValue));
  // EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
  // OpcodeEnum::kFence);
  enc_->ParseInstruction(kEcall);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kEcall);
  enc_->ParseInstruction(kEbreak);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kEbreak);
}

// TEST_F(RiscVCheriotEncodingTest, ZifenceiOpcodes) {
// RV32 Zifencei
//   enc_->ParseInstruction(kFencei);
//   EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//   OpcodeEnum::kFencei);
// }

TEST_F(RiscVCheriotEncodingTest, ZicsrOpcodes) {
  // RV32 Zicsr
  enc_->ParseInstruction(SetRd(kCsrw, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCsrrw);
  enc_->ParseInstruction(SetRd(SetRs1(kCsrs, kRdValue), kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCsrrs);
  enc_->ParseInstruction(SetRd(SetRs1(kCsrc, kRdValue), kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCsrrc);
  enc_->ParseInstruction(kCsrw);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCsrrwNr);
  enc_->ParseInstruction(kCsrs);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCsrrsNw);
  enc_->ParseInstruction(kCsrc);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCsrrcNw);
  enc_->ParseInstruction(SetRd(kCsrwi, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCsrrwi);
  enc_->ParseInstruction(SetRd(SetRs1(kCsrsi, kRdValue), kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCsrrsi);
  enc_->ParseInstruction(SetRd(SetRs1(kCsrci, kRdValue), kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCsrrci);
  enc_->ParseInstruction(kCsrwi);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCsrrwiNr);
  enc_->ParseInstruction(kCsrsi);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCsrrsiNw);
  enc_->ParseInstruction(kCsrci);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCsrrciNw);
}

TEST_F(RiscVCheriotEncodingTest, RV32MOpcodes) {
  // RV32M
  enc_->ParseInstruction(kMul);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kMul);
  enc_->ParseInstruction(kMulh);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kMulh);
  enc_->ParseInstruction(kMulhsu);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kMulhsu);
  enc_->ParseInstruction(kMulhu);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kMulhu);
  enc_->ParseInstruction(kDiv);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kDiv);
  enc_->ParseInstruction(kDivu);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kDivu);
  enc_->ParseInstruction(kRem);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kRem);
  enc_->ParseInstruction(kRemu);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kRemu);
}

// TEST_F(RiscVCheriotEncodingTest, RV32AOpcodes) {
// RV32A
//  enc_->ParseInstruction(kLrw);
//  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kLrw);
//  enc_->ParseInstruction(kScw);
//  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kScw);
//  enc_->ParseInstruction(kAmoswapw);
//  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//  OpcodeEnum::kAmoswapw); enc_->ParseInstruction(kAmoaddw);
//  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//  OpcodeEnum::kAmoaddw); enc_->ParseInstruction(kAmoxorw);
//  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//  OpcodeEnum::kAmoxorw); enc_->ParseInstruction(kAmoandw);
//  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//  OpcodeEnum::kAmoandw); enc_->ParseInstruction(kAmoorw);
//  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//  OpcodeEnum::kAmoorw); enc_->ParseInstruction(kAmominw);
//  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//  OpcodeEnum::kAmominw); enc_->ParseInstruction(kAmomaxw);
//  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//  OpcodeEnum::kAmomaxw); enc_->ParseInstruction(kAmominuw);
//  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//  OpcodeEnum::kAmominuw); enc_->ParseInstruction(kAmomaxuw);
//  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//  OpcodeEnum::kAmomaxuw);
// }

// Test for decoding compact opcodes.
TEST_F(RiscVCheriotEncodingTest, RV32COpcodes) {
  enc_->ParseInstruction(Set16Rd(kClwsp, 1));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kClwsp);
  enc_->ParseInstruction(Set16Rd(kCldsp, 1));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kClcsp);
  //  enc_->ParseInstruction(kCdldsp);
  //  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
  //  OpcodeEnum::kCfldsp);
  enc_->ParseInstruction(kCswsp);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCswsp);
  enc_->ParseInstruction(kCsdsp);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCscsp);
  //  enc_->ParseInstruction(kCdsdsp);
  //  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
  //  OpcodeEnum::kCfsdsp);
  enc_->ParseInstruction(kClw);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kClw);
  enc_->ParseInstruction(kCld);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kClc);
  enc_->ParseInstruction(kCsw);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCsw);
  enc_->ParseInstruction(kCsd);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCsc);
  //  enc_->ParseInstruction(kCdsd);
  //  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
  //  OpcodeEnum::kCdsd);
  enc_->ParseInstruction(kCheriotCj);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotCj);
  enc_->ParseInstruction(kCheriotCjal);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotCjal);
  enc_->ParseInstruction(Set16Rd(kCheriotCjr, 1));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotCjrCra);
  enc_->ParseInstruction(Set16Rd(kCheriotCjalr, 1));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotCjalrCra);
  enc_->ParseInstruction(kCbeqz);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCbeqz);
  enc_->ParseInstruction(kCbnez);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCbnez);
  enc_->ParseInstruction(Set16Rd(kCli, 1));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCli);
  enc_->ParseInstruction(Set16Rs2(Set16Rd(kClui, 1), 5));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kClui);
  enc_->ParseInstruction(Set16Rs2(Set16Rd(kCaddi, 1), 5));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCaddi);
  enc_->ParseInstruction(Set16Rs2(kCaddi16sp, 5));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCaddi16sp);
  enc_->ParseInstruction(kCaddi4spn | 0b000'01010101'000'00);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCaddi4spn);
  enc_->ParseInstruction(Set16Rs2(Set16Rd(kCslli, 1), 5));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCslli);
  enc_->ParseInstruction(Set16Rs2(kCsrli, 5));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCsrli);
  enc_->ParseInstruction(Set16Rs2(kCsrai, 5));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCsrai);
  enc_->ParseInstruction(kCandi);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCandi);
  enc_->ParseInstruction(Set16Rs2(Set16Rd(kCmv, 1), 2));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCmv);
  enc_->ParseInstruction(Set16Rs2(Set16Rd(kCadd, 1), 2));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCadd);
  enc_->ParseInstruction(kCand);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCand);
  enc_->ParseInstruction(kCor);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCor);
  enc_->ParseInstruction(kCxor);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCxor);
  enc_->ParseInstruction(kCSub);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCsub);
  enc_->ParseInstruction(kCnop);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kCnop);
  enc_->ParseInstruction(kCebreak);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCebreak);
}

TEST_F(RiscVCheriotEncodingTest, RiscVCheriotOpcodes) {
  enc_->ParseInstruction(kCheriotAndperm);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotAndperm)
      << kOpcodeNames[*enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0)];
  enc_->ParseInstruction(kCheriotAuicgp);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotAuicgp);
  enc_->ParseInstruction(kCheriotAuipcc);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotAuipcc);
  enc_->ParseInstruction(kCheriotCleartag);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotCleartag);
  enc_->ParseInstruction(kCheriotGetaddr);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotGetaddr);
  enc_->ParseInstruction(kCheriotGetbase);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotGetbase);
  enc_->ParseInstruction(kCheriotGethigh);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotGethigh);
  enc_->ParseInstruction(kCheriotGetlen);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotGetlen);
  enc_->ParseInstruction(kCheriotGetperm);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotGetperm);
  enc_->ParseInstruction(kCheriotGettag);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotGettag);
  enc_->ParseInstruction(kCheriotGettop);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotGettop);
  enc_->ParseInstruction(kCheriotGettype);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotGettype);
  enc_->ParseInstruction(kCheriotIncaddr);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotIncaddr);
  enc_->ParseInstruction(kCheriotIncaddrimm);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotIncaddrimm);
  enc_->ParseInstruction(SetRd(kCheriotJal, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotJal);
  enc_->ParseInstruction(SetRd(kCheriotJalr, kRdValue));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotJalrCra);
  enc_->ParseInstruction(kCheriotJal);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotJ);
  enc_->ParseInstruction(kCheriotJalr);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotJalrZero);
  enc_->ParseInstruction(kCheriotLc);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotLc);
  enc_->ParseInstruction(kCheriotMove);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotMove);
  enc_->ParseInstruction(kCheriotRepresentableAlignmentMask);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotRepresentablealignmentmask);
  enc_->ParseInstruction(kCheriotRoundRepresentableLength);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotRoundrepresentablelength);
  enc_->ParseInstruction(kCheriotSc);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotSc);
  enc_->ParseInstruction(kCheriotSeal);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotSeal);
  enc_->ParseInstruction(kCheriotSetaddr);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotSetaddr);
  enc_->ParseInstruction(kCheriotSetbounds);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotSetbounds);
  enc_->ParseInstruction(kCheriotSetboundsexact);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotSetboundsexact);
  enc_->ParseInstruction(kCheriotSetboundsimm);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotSetboundsimm);
  enc_->ParseInstruction(kCheriotSetequalexact);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotSetequalexact);
  enc_->ParseInstruction(kCheriotSethigh);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotSethigh);
  enc_->ParseInstruction(SetRs2(kCheriotSpecialrw, 28));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotSpecialr)
      << kOpcodeNames[*enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0)];
  enc_->ParseInstruction(SetRs2(SetRs1(kCheriotSpecialrw, kRdValue), 28));
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotSpecialrw)
      << kOpcodeNames[*enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0)];
  enc_->ParseInstruction(kCheriotSub);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotSub);
  enc_->ParseInstruction(kCheriotTestsubset);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotTestsubset);
  enc_->ParseInstruction(kCheriotUnseal);
  EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
            OpcodeEnum::kCheriotUnseal);
}

// TEST_F(RiscVCheriotEncodingTest, RV32PrivilegedOpcodes) {
//   enc_->ParseInstruction(kUret);
//   EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//   OpcodeEnum::kUret); enc_->ParseInstruction(kSret);
//   EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//   OpcodeEnum::kSret); enc_->ParseInstruction(kMret);
//   EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//   OpcodeEnum::kMret); enc_->ParseInstruction(kWfi);
//   EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0), OpcodeEnum::kWfi);
//   enc_->ParseInstruction(kSfenceVmaZz);
//   EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//   OpcodeEnum::kSfenceVmaZz); enc_->ParseInstruction(kSfenceVmaZn);
//   EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//   OpcodeEnum::kSfenceVmaZn); enc_->ParseInstruction(kSfenceVmaNz);
//   EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//   OpcodeEnum::kSfenceVmaNz); enc_->ParseInstruction(kSfenceVmaNn);
//   EXPECT_EQ(enc_->GetOpcode(SlotEnum::kRiscv32Cheriot, 0),
//   OpcodeEnum::kSfenceVmaNn);
// }

}  // namespace
