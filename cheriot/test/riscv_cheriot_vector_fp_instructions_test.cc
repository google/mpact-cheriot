// Copyright 2023 Google LLC
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

#include "cheriot/riscv_cheriot_vector_fp_instructions.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <tuple>
#include <vector>

#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "cheriot/test/riscv_cheriot_vector_fp_test_utilities.h"
#include "cheriot/test/riscv_cheriot_vector_instructions_test_base.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/type_helpers.h"
#include "riscv//riscv_fp_host.h"
#include "riscv//riscv_fp_info.h"
#include "riscv//riscv_fp_state.h"
#include "riscv//riscv_register.h"

namespace {

using Instruction = ::mpact::sim::generic::Instruction;
using ::mpact::sim::generic::operator*;  // NOLINT: used below.

// Functions to test.
using ::mpact::sim::cheriot::Vfadd;
using ::mpact::sim::cheriot::Vfdiv;
using ::mpact::sim::cheriot::Vfmacc;
using ::mpact::sim::cheriot::Vfmadd;
using ::mpact::sim::cheriot::Vfmax;
using ::mpact::sim::cheriot::Vfmerge;
using ::mpact::sim::cheriot::Vfmin;
using ::mpact::sim::cheriot::Vfmsac;
using ::mpact::sim::cheriot::Vfmsub;
using ::mpact::sim::cheriot::Vfmul;
using ::mpact::sim::cheriot::Vfnmacc;
using ::mpact::sim::cheriot::Vfnmadd;
using ::mpact::sim::cheriot::Vfnmsac;
using ::mpact::sim::cheriot::Vfnmsub;
using ::mpact::sim::cheriot::Vfrdiv;
using ::mpact::sim::cheriot::Vfrsub;
using ::mpact::sim::cheriot::Vfsgnj;
using ::mpact::sim::cheriot::Vfsgnjn;
using ::mpact::sim::cheriot::Vfsgnjx;
using ::mpact::sim::cheriot::Vfsub;
using ::mpact::sim::cheriot::Vfwadd;
using ::mpact::sim::cheriot::Vfwaddw;
using ::mpact::sim::cheriot::Vfwmacc;
using ::mpact::sim::cheriot::Vfwmsac;
using ::mpact::sim::cheriot::Vfwmul;
using ::mpact::sim::cheriot::Vfwnmacc;
using ::mpact::sim::cheriot::Vfwnmsac;
using ::mpact::sim::cheriot::Vfwsub;
using ::mpact::sim::cheriot::Vfwsubw;

using ::absl::Span;
using ::mpact::sim::riscv::FPExceptions;
using ::mpact::sim::riscv::FPRoundingMode;
using ::mpact::sim::riscv::RVFpRegister;
using ::mpact::sim::riscv::ScopedFPStatus;

// Test fixture for binary fp instructions.
class RiscVCheriotFPInstructionsTest
    : public RiscVCheriotFPInstructionsTestBase {
 public:
  // Floating point test needs to ensure to use the fp special values (inf, NaN
  // etc.) during testing, not just random values.
  template <typename Vd, typename Vs2, typename Vs1>
  void TernaryOpFPTestHelperVV(absl::string_view name, int sew,
                               Instruction *inst, int delta_position,
                               std::function<Vd(Vs2, Vs1, Vd)> operation) {
    int byte_sew = sew / 8;
    if (byte_sew != sizeof(Vd) && byte_sew != sizeof(Vs2) &&
        byte_sew != sizeof(Vs1)) {
      FAIL() << name << ": selected element width != any operand types"
             << " sew: " << sew << " Vd: " << sizeof(Vd)
             << " Vs2: " << sizeof(Vs2) << " Vs1: " << sizeof(Vs1);
      return;
    }
    // Number of elements per vector register.
    constexpr int vs2_size = kVectorLengthInBytes / sizeof(Vs2);
    constexpr int vs1_size = kVectorLengthInBytes / sizeof(Vs1);
    constexpr int vd_size = kVectorLengthInBytes / sizeof(Vd);
    // Input values for 8 registers.
    Vs2 vs2_value[vs2_size * 8];
    Vs1 vs1_value[vs1_size * 8];
    Vd vd_value[vd_size * 8];
    auto vs2_span = Span<Vs2>(vs2_value);
    auto vs1_span = Span<Vs1>(vs1_value);
    auto vd_span = Span<Vd>(vd_value);
    AppendVectorRegisterOperands({kVs2, kVs1, kVd, kVmask}, {kVd});
    SetVectorRegisterValues<uint8_t>(
        {{kVmaskName, Span<const uint8_t>(kA5Mask)}});
    // Iterate across different lmul values.
    for (int lmul_index = 0; lmul_index < 7; lmul_index++) {
      // Initialize input values.
      FillArrayWithRandomFPValues<Vs2>(vs2_span);
      FillArrayWithRandomFPValues<Vs1>(vs1_span);
      FillArrayWithRandomFPValues<Vd>(vd_span);
      using Vs2Int = typename FPTypeInfo<Vs2>::IntType;
      using Vs1Int = typename FPTypeInfo<Vs1>::IntType;
      using VdInt = typename FPTypeInfo<Vd>::IntType;
      // Overwrite the first few values of the input data with infinities,
      // zeros, denormals and NaNs.
      *reinterpret_cast<Vs2Int *>(&vs2_span[0]) = FPTypeInfo<Vs2>::kQNaN;
      *reinterpret_cast<Vs2Int *>(&vs2_span[1]) = FPTypeInfo<Vs2>::kSNaN;
      *reinterpret_cast<Vs2Int *>(&vs2_span[2]) = FPTypeInfo<Vs2>::kPosInf;
      *reinterpret_cast<Vs2Int *>(&vs2_span[3]) = FPTypeInfo<Vs2>::kNegInf;
      *reinterpret_cast<Vs2Int *>(&vs2_span[4]) = FPTypeInfo<Vs2>::kPosZero;
      *reinterpret_cast<Vs2Int *>(&vs2_span[5]) = FPTypeInfo<Vs2>::kNegZero;
      *reinterpret_cast<Vs2Int *>(&vs2_span[6]) = FPTypeInfo<Vs2>::kPosDenorm;
      *reinterpret_cast<Vs2Int *>(&vs2_span[7]) = FPTypeInfo<Vs2>::kNegDenorm;
      *reinterpret_cast<VdInt *>(&vd_span[0]) = FPTypeInfo<Vd>::kQNaN;
      *reinterpret_cast<VdInt *>(&vd_span[1]) = FPTypeInfo<Vd>::kSNaN;
      *reinterpret_cast<VdInt *>(&vd_span[2]) = FPTypeInfo<Vd>::kPosInf;
      *reinterpret_cast<VdInt *>(&vd_span[3]) = FPTypeInfo<Vd>::kNegInf;
      *reinterpret_cast<VdInt *>(&vd_span[4]) = FPTypeInfo<Vd>::kPosZero;
      *reinterpret_cast<VdInt *>(&vd_span[5]) = FPTypeInfo<Vd>::kNegZero;
      *reinterpret_cast<VdInt *>(&vd_span[6]) = FPTypeInfo<Vd>::kPosDenorm;
      *reinterpret_cast<VdInt *>(&vd_span[7]) = FPTypeInfo<Vd>::kNegDenorm;
      if (lmul_index == 4) {
        *reinterpret_cast<Vs1Int *>(&vs1_span[0]) = FPTypeInfo<Vs1>::kQNaN;
        *reinterpret_cast<Vs1Int *>(&vs1_span[1]) = FPTypeInfo<Vs1>::kSNaN;
        *reinterpret_cast<Vs1Int *>(&vs1_span[2]) = FPTypeInfo<Vs1>::kPosInf;
        *reinterpret_cast<Vs1Int *>(&vs1_span[3]) = FPTypeInfo<Vs1>::kNegInf;
        *reinterpret_cast<Vs1Int *>(&vs1_span[4]) = FPTypeInfo<Vs1>::kPosZero;
        *reinterpret_cast<Vs1Int *>(&vs1_span[5]) = FPTypeInfo<Vs1>::kNegZero;
        *reinterpret_cast<Vs1Int *>(&vs1_span[6]) = FPTypeInfo<Vs1>::kPosDenorm;
        *reinterpret_cast<Vs1Int *>(&vs1_span[7]) = FPTypeInfo<Vs1>::kNegDenorm;
      } else if (lmul_index == 5) {
        *reinterpret_cast<Vs1Int *>(&vs1_span[7]) = FPTypeInfo<Vs1>::kQNaN;
        *reinterpret_cast<Vs1Int *>(&vs1_span[6]) = FPTypeInfo<Vs1>::kSNaN;
        *reinterpret_cast<Vs1Int *>(&vs1_span[5]) = FPTypeInfo<Vs1>::kPosInf;
        *reinterpret_cast<Vs1Int *>(&vs1_span[4]) = FPTypeInfo<Vs1>::kNegInf;
        *reinterpret_cast<Vs1Int *>(&vs1_span[3]) = FPTypeInfo<Vs1>::kPosZero;
        *reinterpret_cast<Vs1Int *>(&vs1_span[2]) = FPTypeInfo<Vs1>::kNegZero;
        *reinterpret_cast<Vs1Int *>(&vs1_span[1]) = FPTypeInfo<Vs1>::kPosDenorm;
        *reinterpret_cast<Vs1Int *>(&vs1_span[0]) = FPTypeInfo<Vs1>::kNegDenorm;
      } else if (lmul_index == 6) {
        *reinterpret_cast<Vs1Int *>(&vs1_span[0]) = FPTypeInfo<Vs1>::kQNaN;
        *reinterpret_cast<Vs1Int *>(&vs1_span[1]) = FPTypeInfo<Vs1>::kSNaN;
        *reinterpret_cast<Vs1Int *>(&vs1_span[2]) = FPTypeInfo<Vs1>::kNegInf;
        *reinterpret_cast<Vs1Int *>(&vs1_span[3]) = FPTypeInfo<Vs1>::kPosInf;
        *reinterpret_cast<Vs1Int *>(&vs1_span[4]) = FPTypeInfo<Vs1>::kNegZero;
        *reinterpret_cast<Vs1Int *>(&vs1_span[5]) = FPTypeInfo<Vs1>::kPosZero;
        *reinterpret_cast<Vs1Int *>(&vs1_span[6]) = FPTypeInfo<Vs1>::kNegDenorm;
        *reinterpret_cast<Vs1Int *>(&vs1_span[7]) = FPTypeInfo<Vs1>::kPosDenorm;
      }
      // Modify the first mask bits to use each of the special floating point
      // values.
      vreg_[kVmask]->data_buffer()->Set<uint8_t>(0, 0xff);
      // Set values for all 8 vector registers in the vector register group.
      for (int i = 0; i < 8; i++) {
        auto vs2_name = absl::StrCat("v", kVs2 + i);
        auto vs1_name = absl::StrCat("v", kVs1 + i);
        SetVectorRegisterValues<Vs2>(
            {{vs2_name, vs2_span.subspan(vs2_size * i, vs2_size)}});
        SetVectorRegisterValues<Vs1>(
            {{vs1_name, vs1_span.subspan(vs1_size * i, vs1_size)}});
      }
      int lmul8 = kLmul8Values[lmul_index];
      int lmul8_vd = lmul8 * sizeof(Vd) / byte_sew;
      int lmul8_vs2 = lmul8 * sizeof(Vs2) / byte_sew;
      int lmul8_vs1 = lmul8 * sizeof(Vs1) / byte_sew;
      int num_reg_values = lmul8 * kVectorLengthInBytes / (8 * byte_sew);
      // Configure vector unit for different lmul settings.
      uint32_t vtype =
          (kSewSettingsByByteSize[byte_sew] << 3) | kLmulSettings[lmul_index];
      int vstart = 0;
      // Try different vstart values (updated at the bottom of the loop).
      for (int vstart_count = 0; vstart_count < 4; vstart_count++) {
        int vlen = 1024;
        // Try different vector lengths (updated at the bottom of the loop).
        for (int vlen_count = 0; vlen_count < 4; vlen_count++) {
          ASSERT_TRUE(vlen > vstart);
          int num_values = std::min(num_reg_values, vlen);
          ConfigureVectorUnit(vtype, vlen);
          // Iterate across rounding modes.
          for (int rm : {0, 1, 2, 3, 4}) {
            rv_fp_->SetRoundingMode(static_cast<FPRoundingMode>(rm));
            rv_vector_->set_vstart(vstart);

            // Reset Vd values, since the previous instruction execution
            // overwrites them.
            for (int i = 0; i < 8; i++) {
              auto vd_name = absl::StrCat("v", kVd + i);
              SetVectorRegisterValues<Vd>(
                  {{vd_name, vd_span.subspan(vd_size * i, vd_size)}});
            }

            inst->Execute();
            if (lmul8_vd < 1 || lmul8_vd > 64) {
              EXPECT_TRUE(rv_vector_->vector_exception())
                  << "lmul8: vd: " << lmul8_vd;
              rv_vector_->clear_vector_exception();
              continue;
            }

            if (lmul8_vs2 < 1 || lmul8_vs2 > 64) {
              EXPECT_TRUE(rv_vector_->vector_exception())
                  << "lmul8: vs2: " << lmul8_vs2;
              rv_vector_->clear_vector_exception();
              continue;
            }

            if (lmul8_vs1 < 1 || lmul8_vs1 > 64) {
              EXPECT_TRUE(rv_vector_->vector_exception())
                  << "lmul8: vs1: " << lmul8_vs1;
              rv_vector_->clear_vector_exception();
              continue;
            }
            EXPECT_FALSE(rv_vector_->vector_exception());
            EXPECT_EQ(rv_vector_->vstart(), 0);
            int count = 0;
            for (int reg = kVd; reg < kVd + 8; reg++) {
              for (int i = 0; i < kVectorLengthInBytes / sizeof(Vd); i++) {
                int mask_index = count >> 3;
                int mask_offset = count & 0b111;
                bool mask_value = true;
                // The first 8 bits of the mask are set to true above, so only
                // read the mask value after the first byte.
                if (mask_index > 0) {
                  mask_value =
                      ((kA5Mask[mask_index] >> mask_offset) & 0b1) != 0;
                }
                auto reg_val = vreg_[reg]->data_buffer()->Get<Vd>(i);
                auto int_reg_val =
                    *reinterpret_cast<typename FPTypeInfo<Vd>::IntType *>(
                        &reg_val);
                auto int_vd_val =
                    *reinterpret_cast<typename FPTypeInfo<Vd>::IntType *>(
                        &vd_value[count]);
                if ((count >= vstart) && mask_value && (count < num_values)) {
                  ScopedFPStatus set_fpstatus(rv_fp_->host_fp_interface());
                  auto op_val = operation(vs2_value[count], vs1_value[count],
                                          vd_value[count]);
                  auto int_op_val =
                      *reinterpret_cast<typename FPTypeInfo<Vd>::IntType *>(
                          &op_val);
                  auto int_vs2_val =
                      *reinterpret_cast<typename FPTypeInfo<Vs2>::IntType *>(
                          &vs2_value[count]);
                  auto int_vs1_val =
                      *reinterpret_cast<typename FPTypeInfo<Vs1>::IntType *>(
                          &vs1_value[count]);
                  FPCompare<Vd>(
                      op_val, reg_val, delta_position,
                      absl::StrCat(
                          name, "[", count, "] op(", vs2_value[count], "[0x",
                          absl::Hex(int_vs2_val), "], ", vs1_value[count],
                          "[0x", absl::Hex(int_vs1_val), "], ", vd_value[count],
                          "[0x", absl::Hex(int_vd_val),
                          "]) = ", absl::Hex(int_op_val), " != reg[", reg, "][",
                          i, "]  (", reg_val, " [0x", absl::Hex(int_reg_val),
                          "]) lmul8(", lmul8,
                          ") rm = ", *(rv_fp_->GetRoundingMode())));
                } else {
                  EXPECT_THAT(int_vd_val, int_reg_val) << absl::StrCat(
                      name, "  0 != reg[", reg, "][", i, "]  (", reg_val,
                      " [0x", absl::Hex(int_reg_val), "]) lmul8(", lmul8, ")");
                }
                count++;
              }
              if (HasFailure()) return;
            }
          }
          vlen = absl::Uniform(absl::IntervalOpenClosed, bitgen_, vstart,
                               num_reg_values);
        }
        vstart = absl::Uniform(absl::IntervalOpen, bitgen_, 0, num_reg_values);
      }
    }
  }

  // Floating point test needs to ensure to use the fp special values (inf, NaN
  // etc.) during testing, not just random values. This function handles vector
  // scalar instructions.
  template <typename Vd, typename Vs2, typename Fs1,
            typename ScalarReg = CheriotRegister>
  void TernaryOpFPTestHelperVX(absl::string_view name, int sew,
                               Instruction *inst, int delta_position,
                               std::function<Vd(Vs2, Fs1, Vd)> operation) {
    int byte_sew = sew / 8;
    if (byte_sew != sizeof(Vd) && byte_sew != sizeof(Vs2) &&
        byte_sew != sizeof(Fs1)) {
      FAIL() << name << ": selected element width != any operand types"
             << "sew: " << sew << " Vd: " << sizeof(Vd)
             << " Vs2: " << sizeof(Vs2) << " Fs1: " << sizeof(Fs1);
      return;
    }
    // Number of elements per vector register.
    constexpr int vs2_size = kVectorLengthInBytes / sizeof(Vs2);
    constexpr int vd_size = kVectorLengthInBytes / sizeof(Vd);
    // Input values for 8 registers.
    Vs2 vs2_value[vs2_size * 8];
    Vd vd_value[vd_size * 8];
    auto vs2_span = Span<Vs2>(vs2_value);
    auto vd_span = Span<Vd>(vd_value);
    AppendVectorRegisterOperands({kVs2}, {kVd});
    AppendRegisterOperands<ScalarReg>({kFs1Name}, {});
    AppendVectorRegisterOperands({kVd, kVmask}, {kVd});
    SetVectorRegisterValues<uint8_t>(
        {{kVmaskName, Span<const uint8_t>(kA5Mask)}});
    // Iterate across different lmul values.
    for (int lmul_index = 0; lmul_index < 7; lmul_index++) {
      // Clear vd_span.
      for (auto &vd_val : vd_span) vd_val = 0;
      // Initialize input values.
      FillArrayWithRandomFPValues<Vs2>(vs2_span);
      using Vs2Int = typename FPTypeInfo<Vs2>::IntType;
      using VdInt = typename FPTypeInfo<Vd>::IntType;
      // Overwrite the first few values of the input data with infinities,
      // zeros, denormals and NaNs.
      *reinterpret_cast<Vs2Int *>(&vs2_span[0]) = FPTypeInfo<Vs2>::kQNaN;
      *reinterpret_cast<Vs2Int *>(&vs2_span[1]) = FPTypeInfo<Vs2>::kSNaN;
      *reinterpret_cast<Vs2Int *>(&vs2_span[2]) = FPTypeInfo<Vs2>::kPosInf;
      *reinterpret_cast<Vs2Int *>(&vs2_span[3]) = FPTypeInfo<Vs2>::kNegInf;
      *reinterpret_cast<Vs2Int *>(&vs2_span[4]) = FPTypeInfo<Vs2>::kPosZero;
      *reinterpret_cast<Vs2Int *>(&vs2_span[5]) = FPTypeInfo<Vs2>::kNegZero;
      *reinterpret_cast<Vs2Int *>(&vs2_span[6]) = FPTypeInfo<Vs2>::kPosDenorm;
      *reinterpret_cast<Vs2Int *>(&vs2_span[7]) = FPTypeInfo<Vs2>::kNegDenorm;
      *reinterpret_cast<VdInt *>(&vd_span[0]) = FPTypeInfo<Vd>::kQNaN;
      *reinterpret_cast<VdInt *>(&vd_span[1]) = FPTypeInfo<Vd>::kSNaN;
      *reinterpret_cast<VdInt *>(&vd_span[2]) = FPTypeInfo<Vd>::kPosInf;
      *reinterpret_cast<VdInt *>(&vd_span[3]) = FPTypeInfo<Vd>::kNegInf;
      *reinterpret_cast<VdInt *>(&vd_span[4]) = FPTypeInfo<Vd>::kPosZero;
      *reinterpret_cast<VdInt *>(&vd_span[5]) = FPTypeInfo<Vd>::kNegZero;
      *reinterpret_cast<VdInt *>(&vd_span[6]) = FPTypeInfo<Vd>::kPosDenorm;
      *reinterpret_cast<VdInt *>(&vd_span[7]) = FPTypeInfo<Vd>::kNegDenorm;
      // Modify the first mask bits to use each of the special floating point
      // values.
      vreg_[kVmask]->data_buffer()->Set<uint8_t>(0, 0xff);
      // Set values for all 8 vector registers in the vector register group.
      for (int i = 0; i < 8; i++) {
        auto vs2_name = absl::StrCat("v", kVs2 + i);
        SetVectorRegisterValues<Vs2>(
            {{vs2_name, vs2_span.subspan(vs2_size * i, vs2_size)}});
      }
      int lmul8 = kLmul8Values[lmul_index];
      int lmul8_vd = lmul8 * sizeof(Vd) / byte_sew;
      int lmul8_vs2 = lmul8 * sizeof(Vs2) / byte_sew;
      int lmul8_vs1 = lmul8 * sizeof(Fs1) / byte_sew;
      int num_reg_values = lmul8 * kVectorLengthInBytes / (8 * byte_sew);
      // Configure vector unit for different lmul settings.
      uint32_t vtype =
          (kSewSettingsByByteSize[byte_sew] << 3) | kLmulSettings[lmul_index];
      int vstart = 0;
      // Try different vstart values (updated at the bottom of the loop).
      for (int vstart_count = 0; vstart_count < 4; vstart_count++) {
        int vlen = 1024;
        // Try different vector lengths (updated at the bottom of the loop).
        for (int vlen_count = 0; vlen_count < 4; vlen_count++) {
          ASSERT_TRUE(vlen > vstart);
          int num_values = std::min(num_reg_values, vlen);
          ConfigureVectorUnit(vtype, vlen);
          // Generate a new rs1 value.
          Fs1 fs1_value = RandomFPValue<Fs1>();
          // Need to NaN box the value, that is, if the register value type is
          // wider than the data type for a floating point value, the upper bits
          // are all set to 1's.
          typename RVFpRegister::ValueType fs1_reg_value =
              NaNBox<Fs1, typename RVFpRegister::ValueType>(fs1_value);
          SetRegisterValues<typename RVFpRegister::ValueType, RVFpRegister>(
              {{kFs1Name, fs1_reg_value}});
          // Iterate across rounding modes.
          for (int rm : {0, 1, 2, 3, 4}) {
            rv_fp_->SetRoundingMode(static_cast<FPRoundingMode>(rm));
            rv_vector_->set_vstart(vstart);
            ClearVectorRegisterGroup(kVd, 8);

            // Reset Vd values, since the previous instruction execution
            // overwrites them.
            for (int i = 0; i < 8; i++) {
              auto vd_name = absl::StrCat("v", kVd + i);
              SetVectorRegisterValues<Vd>(
                  {{vd_name, vd_span.subspan(vd_size * i, vd_size)}});
            }

            inst->Execute();
            if (lmul8_vd < 1 || lmul8_vd > 64) {
              EXPECT_TRUE(rv_vector_->vector_exception())
                  << "lmul8: vd: " << lmul8_vd;
              rv_vector_->clear_vector_exception();
              continue;
            }

            if (lmul8_vs2 < 1 || lmul8_vs2 > 64) {
              EXPECT_TRUE(rv_vector_->vector_exception())
                  << "lmul8: vs2: " << lmul8_vs2;
              rv_vector_->clear_vector_exception();
              continue;
            }

            if (lmul8_vs1 < 1 || lmul8_vs1 > 64) {
              EXPECT_TRUE(rv_vector_->vector_exception())
                  << "lmul8: vs1: " << lmul8_vs1;
              rv_vector_->clear_vector_exception();
              continue;
            }
            EXPECT_FALSE(rv_vector_->vector_exception());
            EXPECT_EQ(rv_vector_->vstart(), 0);
            int count = 0;
            for (int reg = kVd; reg < kVd + 8; reg++) {
              for (int i = 0; i < kVectorLengthInBytes / sizeof(Vd); i++) {
                int mask_index = count >> 3;
                int mask_offset = count & 0b111;
                bool mask_value = true;
                // The first 8 bits of the mask are set to true above, so only
                // read the mask value after the first byte from the constant
                // mask values.
                if (mask_index > 0) {
                  mask_value =
                      ((kA5Mask[mask_index] >> mask_offset) & 0b1) != 0;
                }
                auto reg_val = vreg_[reg]->data_buffer()->Get<Vd>(i);
                auto int_reg_val =
                    *reinterpret_cast<typename FPTypeInfo<Vd>::IntType *>(
                        &reg_val);
                auto int_vd_val =
                    *reinterpret_cast<typename FPTypeInfo<Vd>::IntType *>(
                        &vd_value[count]);
                if ((count >= vstart) && mask_value && (count < num_values)) {
                  // Set rounding mode and perform the computation.

                  ScopedFPStatus set_fpstatus(rv_fp_->host_fp_interface());
                  auto op_val =
                      operation(vs2_value[count], fs1_value, vd_value[count]);
                  // Extract the integer view of the fp values.
                  auto int_op_val =
                      *reinterpret_cast<typename FPTypeInfo<Vd>::IntType *>(
                          &op_val);
                  auto int_vs2_val =
                      *reinterpret_cast<typename FPTypeInfo<Vs2>::IntType *>(
                          &vs2_value[count]);
                  auto int_fs1_val =
                      *reinterpret_cast<typename FPTypeInfo<Fs1>::IntType *>(
                          &fs1_value);
                  FPCompare<Vd>(
                      op_val, reg_val, delta_position,
                      absl::StrCat(
                          name, "[", count, "] op(", vs2_value[count], "[0x",
                          absl::Hex(int_vs2_val), "], ", fs1_value, "[0x",
                          absl::Hex(int_fs1_val), "], ", vd_value[count], "[0x",
                          absl::Hex(int_vd_val), "]) = ", op_val, "[0x",
                          absl::Hex(int_op_val), "] ", " != reg[", reg, "][", i,
                          "]  (", reg_val, " [0x", absl::Hex(int_reg_val),
                          "]) lmul8(", lmul8,
                          ") rm = ", *(rv_fp_->GetRoundingMode())));
                } else {
                  EXPECT_EQ(int_vd_val, int_reg_val) << absl::StrCat(
                      name, "  ", vd_value[count], " [0x",
                      absl::Hex(int_vd_val), "] != reg[", reg, "][", i, "]  (",
                      reg_val, " [0x", absl::Hex(int_reg_val), "]) lmul8(",
                      lmul8, ")");
                }
                count++;
              }
              if (HasFailure()) return;
            }
          }
          vlen = absl::Uniform(absl::IntervalOpenClosed, bitgen_, vstart,
                               num_reg_values);
        }
        vstart = absl::Uniform(absl::IntervalOpen, bitgen_, 0, num_reg_values);
      }
    }
  }
};

// Test fp add.
TEST_F(RiscVCheriotFPInstructionsTest, Vfadd) {
  // Vector-vector.
  SetSemanticFunction(&Vfadd);
  BinaryOpFPTestHelperVV<float, float, float>(
      "Vfadd_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float { return vs2 + vs1; });
  ResetInstruction();
  SetSemanticFunction(&Vfadd);
  BinaryOpFPTestHelperVV<double, double, double>(
      "Vfadd_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double { return vs2 + vs1; });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfadd);
  BinaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfadd_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float { return vs2 + vs1; });
  ResetInstruction();
  SetSemanticFunction(&Vfadd);
  BinaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfadd_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double { return vs2 + vs1; });
}

// Test fp sub.
TEST_F(RiscVCheriotFPInstructionsTest, Vfsub) {
  // Vector-vector.
  SetSemanticFunction(&Vfsub);
  BinaryOpFPTestHelperVV<float, float, float>(
      "Vfsub_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float { return vs2 - vs1; });
  ResetInstruction();
  SetSemanticFunction(&Vfsub);
  BinaryOpFPTestHelperVV<double, double, double>(
      "Vfsub_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double { return vs2 - vs1; });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfsub);
  BinaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfsub_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float { return vs2 - vs1; });
  ResetInstruction();
  SetSemanticFunction(&Vfsub);
  BinaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfsub_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double { return vs2 - vs1; });
}

// Test fp reverse sub.
TEST_F(RiscVCheriotFPInstructionsTest, Vfrsub) {
  // Vector-vector.
  SetSemanticFunction(&Vfrsub);
  BinaryOpFPTestHelperVV<float, float, float>(
      "Vfrsub_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float { return vs1 - vs2; });
  ResetInstruction();
  SetSemanticFunction(&Vfrsub);
  BinaryOpFPTestHelperVV<double, double, double>(
      "Vfrsub_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double { return vs1 - vs2; });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfrsub);
  BinaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfrsub_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float { return vs1 - vs2; });
  ResetInstruction();
  SetSemanticFunction(&Vfrsub);
  BinaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfrsub_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double { return vs1 - vs2; });
}

// Test fp widening add.
TEST_F(RiscVCheriotFPInstructionsTest, Vfwadd) {
  // Vector-vector.
  SetSemanticFunction(&Vfwadd);
  BinaryOpFPTestHelperVV<double, float, float>(
      "Vfwadd_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> double {
        return static_cast<double>(vs2) + static_cast<double>(vs1);
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfwadd);
  BinaryOpFPTestHelperVX<double, float, float, RVFpRegister>(
      "Vfwadd_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> double {
        return static_cast<double>(vs2) + static_cast<double>(vs1);
      });
}

// Test fp widening subtract.
TEST_F(RiscVCheriotFPInstructionsTest, Vfwsub) {
  // Vector-vector.
  SetSemanticFunction(&Vfwsub);
  BinaryOpFPTestHelperVV<double, float, float>(
      "Vfwsub_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> double {
        return static_cast<double>(vs2) - static_cast<double>(vs1);
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfwsub);
  BinaryOpFPTestHelperVX<double, float, float, RVFpRegister>(
      "Vfwsub_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> double {
        return static_cast<double>(vs2) - static_cast<double>(vs1);
      });
}

// Test fp widening add with wide operand.
TEST_F(RiscVCheriotFPInstructionsTest, Vfwaddw) {
  // Vector-vector.
  SetSemanticFunction(&Vfwaddw);
  BinaryOpFPTestHelperVV<double, double, float>(
      "Vfwaddw_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](double vs2, float vs1) -> double {
        return vs2 + static_cast<double>(vs1);
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfwaddw);
  BinaryOpFPTestHelperVX<double, double, float, RVFpRegister>(
      "Vfwaddw_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](double vs2, float vs1) -> double {
        return vs2 + static_cast<double>(vs1);
      });
}

// Test fp widening subtract with wide operand.
TEST_F(RiscVCheriotFPInstructionsTest, Vfwsubw) {
  // Vector-vector.
  SetSemanticFunction(&Vfwsubw);
  BinaryOpFPTestHelperVV<double, double, float>(
      "Vfwsubw_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](double vs2, float vs1) -> double {
        return vs2 - static_cast<double>(vs1);
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfwsubw);
  BinaryOpFPTestHelperVX<double, double, float, RVFpRegister>(
      "Vfwsubw_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](double vs2, float vs1) -> double {
        return vs2 - static_cast<double>(vs1);
      });
}

// Test fp multiply.
TEST_F(RiscVCheriotFPInstructionsTest, Vfmul) {
  // Vector-vector.
  SetSemanticFunction(&Vfmul);
  BinaryOpFPTestHelperVV<float, float, float>(
      "Vfmul_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float { return vs2 * vs1; });
  ResetInstruction();
  SetSemanticFunction(&Vfmul);
  BinaryOpFPTestHelperVV<double, double, double>(
      "Vfmul_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double { return vs2 * vs1; });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfmul);
  BinaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfmul_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float { return vs2 * vs1; });
  ResetInstruction();
  SetSemanticFunction(&Vfmul);
  BinaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfmul_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double { return vs2 * vs1; });
}

// Test fp divide.
TEST_F(RiscVCheriotFPInstructionsTest, Vfdiv) {
  // Vector-vector.
  SetSemanticFunction(&Vfdiv);
  BinaryOpFPTestHelperVV<float, float, float>(
      "Vfdiv_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float { return vs2 / vs1; });
  ResetInstruction();
  SetSemanticFunction(&Vfdiv);
  BinaryOpFPTestHelperVV<double, double, double>(
      "Vfdiv_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double { return vs2 / vs1; });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfdiv);
  BinaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfdiv_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float { return vs2 / vs1; });
  ResetInstruction();
  SetSemanticFunction(&Vfdiv);
  BinaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfdiv_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double { return vs2 / vs1; });
}

// Test fp reverse divide.
TEST_F(RiscVCheriotFPInstructionsTest, Vfrdiv) {
  // Vector-vector.
  SetSemanticFunction(&Vfrdiv);
  BinaryOpFPTestHelperVV<float, float, float>(
      "Vfrdiv_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float { return vs1 / vs2; });
  ResetInstruction();
  SetSemanticFunction(&Vfrdiv);
  BinaryOpFPTestHelperVV<double, double, double>(
      "Vfrdiv_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double { return vs1 / vs2; });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfrdiv);
  BinaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfrdiv_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float { return vs1 / vs2; });
  ResetInstruction();
  SetSemanticFunction(&Vfrdiv);
  BinaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfrdiv_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double { return vs1 / vs2; });
}

// Test fp widening multiply.
TEST_F(RiscVCheriotFPInstructionsTest, Vfwmul) {
  // Vector-vector.
  SetSemanticFunction(&Vfwmul);
  BinaryOpFPTestHelperVV<double, float, float>(
      "Vfwmul_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> double {
        return static_cast<double>(vs2) * static_cast<double>(vs1);
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfwmul);
  BinaryOpFPTestHelperVX<double, float, float, RVFpRegister>(
      "Vfwmul_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> double {
        return static_cast<double>(vs2) * static_cast<double>(vs1);
      });
}

// Test fp multiply add.
TEST_F(RiscVCheriotFPInstructionsTest, Vfmadd) {
  // Vector-vector.
  SetSemanticFunction(&Vfmadd);
  TernaryOpFPTestHelperVV<float, float, float>(
      "Vfmadd_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return std::fma(vs1, vd, vs2);
      });
  ResetInstruction();
  SetSemanticFunction(&Vfmadd);
  TernaryOpFPTestHelperVV<double, double, double>(
      "Vfmadd_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return std::fma(vs1, vd, vs2);
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfmadd);
  TernaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfmadd_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return std::fma(vs1, vd, vs2);
      });
  ResetInstruction();
  SetSemanticFunction(&Vfmadd);
  TernaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfmadd_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return std::fma(vs1, vd, vs2);
      });
}

// Test fp negated multiply add.
TEST_F(RiscVCheriotFPInstructionsTest, Vfnmadd) {
  // Vector-vector.
  SetSemanticFunction(&Vfnmadd);
  TernaryOpFPTestHelperVV<float, float, float>(
      "Vfnmadd_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(-vs1, vd, -vs2));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfnmadd);
  TernaryOpFPTestHelperVV<double, double, double>(
      "Vfnmadd_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(-vs1, vd, -vs2));
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfnmadd);
  TernaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfnmadd_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(-vs1, vd, -vs2));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfnmadd);
  TernaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfnmadd_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(-vs1, vd, -vs2));
      });
}

// Test fp multiply subtract.
TEST_F(RiscVCheriotFPInstructionsTest, Vfmsub) {
  // Vector-vector.
  SetSemanticFunction(&Vfmsub);
  TernaryOpFPTestHelperVV<float, float, float>(
      "Vfmsub_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(vs1, vd, -vs2));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfmsub);
  TernaryOpFPTestHelperVV<double, double, double>(
      "Vfmsub_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(vs1, vd, -vs2));
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfmsub);
  TernaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfmsub_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(vs1, vd, -vs2));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfmsub);
  TernaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfmsub_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(vs1, vd, -vs2));
      });
}

// Test fp negated multiply subtract.
TEST_F(RiscVCheriotFPInstructionsTest, Vfnmsub) {
  // Vector-vector.
  SetSemanticFunction(&Vfnmsub);
  TernaryOpFPTestHelperVV<float, float, float>(
      "Vfnmsub_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(-vs1, vd, vs2));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfnmsub);
  TernaryOpFPTestHelperVV<double, double, double>(
      "Vfnmsub_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(-vs1, vd, vs2));
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfnmsub);
  TernaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfnmsub_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(-vs1, vd, vs2));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfnmsub);
  TernaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfnmsub_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(-vs1, vd, vs2));
      });
}

// Test fp multiply accumulate.
TEST_F(RiscVCheriotFPInstructionsTest, Vfmacc) {
  // Vector-vector.
  SetSemanticFunction(&Vfmacc);
  TernaryOpFPTestHelperVV<float, float, float>(
      "Vfmacc_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(vs1, vs2, vd));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfmacc);
  TernaryOpFPTestHelperVV<double, double, double>(
      "Vfmacc_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(vs1, vs2, vd));
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfmacc);
  TernaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfmacc_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(vs1, vs2, vd));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfmacc);
  TernaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfmacc_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(vs1, vs2, vd));
      });
}

// Test fp negated multiply add.
TEST_F(RiscVCheriotFPInstructionsTest, Vfnmacc) {
  // Vector-vector.
  SetSemanticFunction(&Vfnmacc);
  TernaryOpFPTestHelperVV<float, float, float>(
      "Vfnmacc_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(-vs1, vs2, -vd));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfnmacc);
  TernaryOpFPTestHelperVV<double, double, double>(
      "Vfnmacc_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(-vs1, vs2, -vd));
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfnmacc);
  TernaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfnmacc_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(-vs1, vs2, -vd));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfnmacc);
  TernaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfnmacc_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(-vs1, vs2, -vd));
      });
}

// Test fp multiply subtract accumulate.
TEST_F(RiscVCheriotFPInstructionsTest, Vfmsac) {
  // Vector-vector.
  SetSemanticFunction(&Vfmsac);
  TernaryOpFPTestHelperVV<float, float, float>(
      "Vfmsac_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(vs1, vs2, -vd));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfmsac);
  TernaryOpFPTestHelperVV<double, double, double>(
      "Vfmsac_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(vs1, vs2, -vd));
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfmsac);
  TernaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfmsac_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(vs1, vs2, -vd));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfmsac);
  TernaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfmsac_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(vs1, vs2, -vd));
      });
}

// Test fp negated multiply subtract accumulate.
TEST_F(RiscVCheriotFPInstructionsTest, Vfnmsac) {
  // Vector-vector.
  SetSemanticFunction(&Vfnmsac);
  TernaryOpFPTestHelperVV<float, float, float>(
      "Vfnmsac_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(-vs1, vs2, vd));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfnmsac);
  TernaryOpFPTestHelperVV<double, double, double>(
      "Vfnmsac_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(-vs1, vs2, vd));
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfnmsac);
  TernaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfnmsac_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1, float vd) -> float {
        return OptimizationBarrier(std::fma(-vs1, vs2, vd));
      });
  ResetInstruction();
  SetSemanticFunction(&Vfnmsac);
  TernaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfnmsac_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1, double vd) -> double {
        return OptimizationBarrier(std::fma(-vs1, vs2, vd));
      });
}

// Test fp widening multiply accumulate.
TEST_F(RiscVCheriotFPInstructionsTest, Vfwmacc) {
  // Vector-vector.
  SetSemanticFunction(&Vfwmacc);
  TernaryOpFPTestHelperVV<double, float, float>(
      "Vfwmacc_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 64,
      [](float vs2, float vs1, double vd) -> double {
        double vs1d = static_cast<double>(vs1);
        double vs2d = static_cast<double>(vs2);
        return OptimizationBarrier(vs1d * vs2d) + vd;
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfwmacc);
  TernaryOpFPTestHelperVX<double, float, float, RVFpRegister>(
      "Vfwmacc_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 64,
      [](float vs2, float vs1, double vd) -> double {
        double vs1d = static_cast<double>(vs1);
        double vs2d = static_cast<double>(vs2);
        return OptimizationBarrier(vs1d * vs2d) + vd;
      });
}

// Test fp widening negated multiply add.
TEST_F(RiscVCheriotFPInstructionsTest, Vfwnmacc) {
  // Vector-vector.
  SetSemanticFunction(&Vfwnmacc);
  TernaryOpFPTestHelperVV<double, float, float>(
      "Vfwnmacc_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 64,
      [](float vs2, float vs1, double vd) -> double {
        double vs1d = static_cast<double>(vs1);
        double vs2d = static_cast<double>(vs2);
        return -OptimizationBarrier(vs1d * vs2d) - vd;
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfwnmacc);
  TernaryOpFPTestHelperVX<double, float, float, RVFpRegister>(
      "Vfwnmacc_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 64,
      [](float vs2, float vs1, double vd) -> double {
        double vs1d = static_cast<double>(vs1);
        double vs2d = static_cast<double>(vs2);
        return -OptimizationBarrier(vs1d * vs2d) - vd;
      });
}

// Test fp widening multiply subtract accumulate.
TEST_F(RiscVCheriotFPInstructionsTest, Vfwmsac) {
  // Vector-vector.
  SetSemanticFunction(&Vfwmsac);
  TernaryOpFPTestHelperVV<double, float, float>(
      "Vfwmsac_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 64,
      [](float vs2, float vs1, double vd) -> double {
        double vs1d = static_cast<double>(vs1);
        double vs2d = static_cast<double>(vs2);
        return OptimizationBarrier(vs1d * vs2d) - vd;
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfwmsac);
  TernaryOpFPTestHelperVX<double, float, float, RVFpRegister>(
      "Vfwmsac_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 64,
      [](float vs2, float vs1, double vd) -> double {
        double vs1d = static_cast<double>(vs1);
        double vs2d = static_cast<double>(vs2);
        return OptimizationBarrier(vs1d * vs2d) - vd;
      });
}

// Test fp widening negated multiply subtract accumulate.
TEST_F(RiscVCheriotFPInstructionsTest, Vfwnmsac) {
  // Vector-vector.
  SetSemanticFunction(&Vfwnmsac);
  TernaryOpFPTestHelperVV<double, float, float>(
      "Vfwnmsac_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 64,
      [](float vs2, float vs1, double vd) -> double {
        double vs1d = static_cast<double>(vs1);
        double vs2d = static_cast<double>(vs2);
        return -OptimizationBarrier(vs1d * vs2d) + vd;
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfwnmsac);
  TernaryOpFPTestHelperVX<double, float, float, RVFpRegister>(
      "Vfwnmsac_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 64,
      [](float vs2, float vs1, double vd) -> double {
        double vs1d = static_cast<double>(vs1);
        double vs2d = static_cast<double>(vs2);
        return -OptimizationBarrier(vs1d * vs2d) + vd;
      });
}

// Test vector floating point sign injection instructions. There are three
// of these. vfsgnj, vfsgnjn, and vfsgnjx. The instructions take the sign
// bit from vs1/fs1 and the other bits from vs2. The sign bit is either used
// as is, negated (n) or xor'ed (x).

template <typename T>
inline T SignHelper(
    T vs2, T vs1,
    std::function<typename FPTypeInfo<T>::IntType(
        typename FPTypeInfo<T>::IntType, typename FPTypeInfo<T>::IntType,
        typename FPTypeInfo<T>::IntType)>
        sign_op) {
  using Int = typename FPTypeInfo<T>::IntType;
  Int sign_mask = 1ULL << (FPTypeInfo<T>::kBitSize - 1);
  Int vs2i = *reinterpret_cast<Int *>(&vs2);
  Int vs1i = *reinterpret_cast<Int *>(&vs1);
  Int resi = sign_op(vs2i, vs1i, sign_mask);
  return *reinterpret_cast<T *>(&resi);
}

// The sign is that of vs1.
TEST_F(RiscVCheriotFPInstructionsTest, Vfsgnj) {
  // Vector-vector.
  SetSemanticFunction(&Vfsgnj);
  BinaryOpFPTestHelperVV<float, float, float>(
      "Vfsgnj_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float {
        using Int = typename FPTypeInfo<float>::IntType;
        return SignHelper(vs2, vs1, [](Int vs2i, Int vs1i, Int mask) -> Int {
          return (vs2i & ~mask) | (vs1i & mask);
        });
      });
  ResetInstruction();
  SetSemanticFunction(&Vfsgnj);
  BinaryOpFPTestHelperVV<double, double, double>(
      "Vfsgnj_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double {
        using Int = typename FPTypeInfo<double>::IntType;
        return SignHelper(vs2, vs1, [](Int vs2i, Int vs1i, Int mask) -> Int {
          return (vs2i & ~mask) | (vs1i & mask);
        });
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfsgnj);
  BinaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfsgnj_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float {
        using Int = typename FPTypeInfo<float>::IntType;
        return SignHelper(vs2, vs1, [](Int vs2i, Int vs1i, Int mask) -> Int {
          return (vs2i & ~mask) | (vs1i & mask);
        });
      });
  ResetInstruction();
  SetSemanticFunction(&Vfsgnj);
  BinaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfsgnj_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double {
        using Int = typename FPTypeInfo<double>::IntType;
        return SignHelper(vs2, vs1, [](Int vs2i, Int vs1i, Int mask) -> Int {
          return (vs2i & ~mask) | (vs1i & mask);
        });
      });
}

// The sign is the negation of that of vs1.
TEST_F(RiscVCheriotFPInstructionsTest, Vfsgnjn) {
  // Vector-vector.
  SetSemanticFunction(&Vfsgnjn);
  BinaryOpFPTestHelperVV<float, float, float>(
      "Vfsgnjn_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float {
        using Int = typename FPTypeInfo<float>::IntType;
        return SignHelper(vs2, vs1, [](Int vs2i, Int vs1i, Int mask) -> Int {
          return (vs2i & ~mask) | (~vs1i & mask);
        });
      });
  ResetInstruction();
  SetSemanticFunction(&Vfsgnjn);
  BinaryOpFPTestHelperVV<double, double, double>(
      "Vfsgnjn_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double {
        using Int = typename FPTypeInfo<double>::IntType;
        return SignHelper(vs2, vs1, [](Int vs2i, Int vs1i, Int mask) -> Int {
          return (vs2i & ~mask) | (~vs1i & mask);
        });
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfsgnjn);
  BinaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfsgnjn_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float {
        using Int = typename FPTypeInfo<float>::IntType;
        return SignHelper(vs2, vs1, [](Int vs2i, Int vs1i, Int mask) -> Int {
          return (vs2i & ~mask) | (~vs1i & mask);
        });
      });
  ResetInstruction();
  SetSemanticFunction(&Vfsgnjn);
  BinaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfsgnjn_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double {
        using Int = typename FPTypeInfo<double>::IntType;
        return SignHelper(vs2, vs1, [](Int vs2i, Int vs1i, Int mask) -> Int {
          return (vs2i & ~mask) | (~vs1i & mask);
        });
      });
}

// The sign is exclusive or of the signs of vs2 and vs1.
TEST_F(RiscVCheriotFPInstructionsTest, Vfsgnjx) {
  // Vector-vector.
  SetSemanticFunction(&Vfsgnjx);
  BinaryOpFPTestHelperVV<float, float, float>(
      "Vfsgnjx_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float {
        using Int = typename FPTypeInfo<float>::IntType;
        return SignHelper(vs2, vs1, [](Int vs2i, Int vs1i, Int mask) -> Int {
          return (vs2i & ~mask) | ((vs1i ^ vs2i) & mask);
        });
      });
  ResetInstruction();
  SetSemanticFunction(&Vfsgnjx);
  BinaryOpFPTestHelperVV<double, double, double>(
      "Vfsgnjx_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double {
        using Int = typename FPTypeInfo<double>::IntType;
        return SignHelper(vs2, vs1, [](Int vs2i, Int vs1i, Int mask) -> Int {
          return (vs2i & ~mask) | ((vs1i ^ vs2i) & mask);
        });
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfsgnjx);
  BinaryOpFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfsgnjx_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> float {
        using Int = typename FPTypeInfo<float>::IntType;
        return SignHelper(vs2, vs1, [](Int vs2i, Int vs1i, Int mask) -> Int {
          return (vs2i & ~mask) | ((vs1i ^ vs2i) & mask);
        });
      });
  ResetInstruction();
  SetSemanticFunction(&Vfsgnjx);
  BinaryOpFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfsgnjx_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> double {
        using Int = typename FPTypeInfo<double>::IntType;
        return SignHelper(vs2, vs1, [](Int vs2i, Int vs1i, Int mask) -> Int {
          return (vs2i & ~mask) | ((vs1i ^ vs2i) & mask);
        });
      });
}

template <typename T>
bool is_snan(T value) {
  using IntType = typename FPTypeInfo<T>::IntType;
  IntType int_value = *reinterpret_cast<IntType *>(&value);
  bool signal = (int_value & (1ULL << (FPTypeInfo<T>::kSigSize - 1))) == 0;
  return std::isnan(value) && signal;
}

template <typename T>
std::tuple<T, uint32_t> MaxMinHelper(T vs2, T vs1,
                                     std::function<T(T, T)> operation) {
  uint32_t flag = 0;
  if (is_snan(vs2) || is_snan(vs1)) {
    flag = static_cast<uint32_t>(FPExceptions::kInvalidOp);
  }
  if (std::isnan(vs2) && std::isnan(vs1)) {
    // Canonical NaN.
    auto canonical = FPTypeInfo<T>::kCanonicalNaN;
    T canonical_fp = *reinterpret_cast<T *>(&canonical);
    return std::tie(canonical_fp, flag);
  }
  if (std::isnan(vs2)) return std::tie(vs1, flag);
  if (std::isnan(vs1)) return std::tie(vs2, flag);
  if ((vs2 == 0.0) && (vs1 == 0.0)) {
    T tmp2 = std::signbit(vs2) ? -1.0 : 1;
    T tmp1 = std::signbit(vs1) ? -1.0 : 1;
    return std::make_tuple(operation(tmp2, tmp1) == tmp2 ? vs2 : vs1, 0);
  }
  return std::make_tuple(operation(vs2, vs1), 0);
}

TEST_F(RiscVCheriotFPInstructionsTest, Vfmax) {
  // Vector-vector.
  SetSemanticFunction(&Vfmax);
  BinaryOpWithFflagsFPTestHelperVV<float, float, float>(
      "Vfmax_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> std::tuple<float, uint32_t> {
        using T = float;
        auto tmp = MaxMinHelper<T>(vs2, vs1, [](T vs2, T vs1) -> T {
          return (vs1 > vs2) ? vs1 : vs2;
        });
        return tmp;
      });
  ResetInstruction();
  SetSemanticFunction(&Vfmax);
  BinaryOpWithFflagsFPTestHelperVV<double, double, double>(
      "Vfmax_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> std::tuple<double, uint32_t> {
        using T = double;
        return MaxMinHelper<T>(vs2, vs1, [](T vs2, T vs1) -> T {
          return (vs1 > vs2) ? vs1 : vs2;
        });
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfmax);
  BinaryOpWithFflagsFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfmax_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> std::tuple<float, uint32_t> {
        using T = float;
        return MaxMinHelper<T>(vs2, vs1, [](T vs2, T vs1) -> T {
          return (vs1 > vs2) ? vs1 : vs2;
        });
      });
  ResetInstruction();
  SetSemanticFunction(&Vfmax);
  BinaryOpWithFflagsFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfmax_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> std::tuple<double, uint32_t> {
        using T = double;
        return MaxMinHelper<T>(vs2, vs1, [](T vs2, T vs1) -> T {
          return (vs1 > vs2) ? vs1 : vs2;
        });
      });
}

TEST_F(RiscVCheriotFPInstructionsTest, Vfmin) {
  // Vector-vector.
  SetSemanticFunction(&Vfmin);
  BinaryOpWithFflagsFPTestHelperVV<float, float, float>(
      "Vfmin_vv32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> std::tuple<float, uint32_t> {
        using T = float;
        return MaxMinHelper<T>(vs2, vs1, [](T vs2, T vs1) -> T {
          return (vs1 < vs2) ? vs1 : vs2;
        });
      });
  ResetInstruction();
  SetSemanticFunction(&Vfmin);
  BinaryOpWithFflagsFPTestHelperVV<double, double, double>(
      "Vfmin_vv64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> std::tuple<double, uint32_t> {
        using T = double;
        return MaxMinHelper<T>(vs2, vs1, [](T vs2, T vs1) -> T {
          return (vs1 < vs2) ? vs1 : vs2;
        });
      });
  // Vector-scalar.
  ResetInstruction();
  SetSemanticFunction(&Vfmin);
  BinaryOpWithFflagsFPTestHelperVX<float, float, float, RVFpRegister>(
      "Vfmin_vx32", /*sew*/ 32, instruction_, /*delta_position*/ 32,
      [](float vs2, float vs1) -> std::tuple<float, uint32_t> {
        using T = float;
        return MaxMinHelper<T>(vs2, vs1, [](T vs2, T vs1) -> T {
          return (vs1 < vs2) ? vs1 : vs2;
        });
      });
  ResetInstruction();
  SetSemanticFunction(&Vfmin);
  BinaryOpWithFflagsFPTestHelperVX<double, double, double, RVFpRegister>(
      "Vfmin_vx64", /*sew*/ 64, instruction_, /*delta_position*/ 64,
      [](double vs2, double vs1) -> std::tuple<double, uint32_t> {
        using T = double;
        return MaxMinHelper<T>(vs2, vs1, [](T vs2, T vs1) -> T {
          return (vs1 < vs2) ? vs1 : vs2;
        });
      });
}

TEST_F(RiscVCheriotFPInstructionsTest, Vfmerge) {
  // Vector-scalar.
  SetSemanticFunction(&Vfmerge);
  BinaryOpFPWithMaskTestHelperVX<float, float, float, RVFpRegister>(
      "Vfmerge_vx32", /*sew*/ 32, instruction_, /*delta position*/ 32,
      [](float vs2, float vs1, bool mask) -> float {
        return mask ? vs1 : vs2;
      });
  BinaryOpFPWithMaskTestHelperVX<double, double, double, RVFpRegister>(
      "Vfmerge_vx64", /*sew*/ 64, instruction_, /*delta position*/ 64,
      [](double vs2, double vs1, bool mask) -> double {
        return mask ? vs1 : vs2;
      });
}

}  // namespace
