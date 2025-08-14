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

#include "cheriot/riscv_cheriot_vector_opm_instructions.h"

#include <cstdint>
#include <functional>
#include <ios>
#include <type_traits>

#include "absl/base/casts.h"
#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "cheriot/test/riscv_cheriot_vector_instructions_test_base.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/generic/type_helpers.h"

namespace {

using Instruction = ::mpact::sim::generic::Instruction;
using ::mpact::sim::generic::WideType;

using ::mpact::sim::cheriot::Vaadd;
using ::mpact::sim::cheriot::Vaaddu;
using ::mpact::sim::cheriot::Vasub;
using ::mpact::sim::cheriot::Vasubu;
using ::mpact::sim::cheriot::Vdiv;
using ::mpact::sim::cheriot::Vdivu;
using ::mpact::sim::cheriot::Vmacc;
using ::mpact::sim::cheriot::Vmadd;
using ::mpact::sim::cheriot::Vmand;
using ::mpact::sim::cheriot::Vmandnot;
using ::mpact::sim::cheriot::Vmnand;
using ::mpact::sim::cheriot::Vmnor;
using ::mpact::sim::cheriot::Vmor;
using ::mpact::sim::cheriot::Vmornot;
using ::mpact::sim::cheriot::Vmul;
using ::mpact::sim::cheriot::Vmulh;
using ::mpact::sim::cheriot::Vmulhsu;
using ::mpact::sim::cheriot::Vmulhu;
using ::mpact::sim::cheriot::Vmxnor;
using ::mpact::sim::cheriot::Vmxor;
using ::mpact::sim::cheriot::Vnmsac;
using ::mpact::sim::cheriot::Vnmsub;
using ::mpact::sim::cheriot::Vrem;
using ::mpact::sim::cheriot::Vremu;
using ::mpact::sim::cheriot::Vwadd;
using ::mpact::sim::cheriot::Vwaddu;
using ::mpact::sim::cheriot::Vwadduw;
using ::mpact::sim::cheriot::Vwaddw;
using ::mpact::sim::cheriot::Vwmacc;
using ::mpact::sim::cheriot::Vwmaccsu;
using ::mpact::sim::cheriot::Vwmaccu;
using ::mpact::sim::cheriot::Vwmaccus;
using ::mpact::sim::cheriot::Vwmul;
using ::mpact::sim::cheriot::Vwmulsu;
using ::mpact::sim::cheriot::Vwmulu;
using ::mpact::sim::cheriot::Vwsub;
using ::mpact::sim::cheriot::Vwsubu;
using ::mpact::sim::cheriot::Vwsubuw;
using ::mpact::sim::cheriot::Vwsubw;

// Derived test class - adds a test helper function for testing the logical
// mask operation instructions.
class RiscVCheriotVectorOpmInstructionsTest
    : public RiscVCheriotVectorInstructionsTestBase {
 protected:
  void BinaryLogicalMaskOpTestHelper(absl::string_view name,
                                     std::function<bool(bool, bool)> op) {
    uint8_t vs2_value[kVectorLengthInBytes];
    uint8_t vs1_value[kVectorLengthInBytes];
    uint8_t vd_value[kVectorLengthInBytes];
    FillArrayWithRandomValues<uint8_t>(vs2_value);
    FillArrayWithRandomValues<uint8_t>(vs1_value);
    FillArrayWithRandomValues<uint8_t>(vd_value);
    AppendVectorRegisterOperands({kVs2, kVs1}, {kVd});
    for (int vstart : {0, 7, 32, 100, 250, 384}) {
      for (int vlen_pct : {10, 20, 50, 100}) {
        int vlen =
            (kVectorLengthInBytes * 8 - vstart) * vlen_pct / 100 + vstart;
        CHECK_LE(vlen, kVectorLengthInBytes * 8);
        // Configure vector unit for different lmul settings.
        uint32_t vtype = (kSewSettingsByByteSize[1] << 3) | kLmulSettings[6];
        ConfigureVectorUnit(vtype, vlen);
        vlen = rv_vector_->vector_length();
        rv_vector_->set_vstart(vstart);
        SetVectorRegisterValues<uint8_t>({{kVs2Name, vs2_value},
                                          {kVs1Name, vs1_value},
                                          {kVdName, vd_value}});
        instruction_->Execute();
        auto dst_span = vreg_[kVd]->data_buffer()->Get<uint8_t>();
        for (int i = 0; i < kVectorLengthInBytes * 8; i++) {
          int mask_index = i >> 3;
          int mask_offset = i & 0b111;
          bool result = (dst_span[mask_index] >> mask_offset) & 0b1;
          if ((i < vstart) || (i >= vlen)) {
            bool vd = (vd_value[mask_index] >> mask_offset) & 0b1;
            EXPECT_EQ(result, vd) << "[" << i << "] " << std::hex
                                  << "vd: " << (int)vd_value[mask_index]
                                  << "  dst: " << (int)dst_span[mask_index];
          } else {
            bool vs2 = (vs2_value[mask_index] >> mask_offset) & 0b1;
            bool vs1 = (vs1_value[mask_index] >> mask_offset) & 0b1;
            EXPECT_EQ(result, op(vs2, vs1))
                << "[" << i << "]: " << "op(" << vs2 << ", " << vs1 << ")";
          }
        }
      }
    }
  }
};

// Helper functions for averaging add and subtract.
template <typename T>
T VaaddHelper(RiscVCheriotVectorOpmInstructionsTest* tester, T vs2, T vs1) {
  // Create two sums, lower nibble, and the upper part. Then combine after
  // rounding.
  T vs2_l = vs2 & 0xf;
  T vs1_l = vs1 & 0xf;
  T res_l = vs2_l + vs1_l;
  T res = (vs2 >> 4) + (vs1 >> 4);
  res_l += tester->RoundBits<T>(2, res_l) << 1;
  // Add carry.
  res += (res_l >> 4);
  // Use unsigned type to avoid undefined behavior of left-shifting negative
  // numbers.
  using UT = typename std::make_unsigned<T>::type;
  res = (absl::bit_cast<UT>(res) << 3) | ((res_l >> 1) & 0b111);
  return res;
}

template <typename T>
T VasubHelper(RiscVCheriotVectorOpmInstructionsTest* tester, T vs2, T vs1) {
  // Create two diffs, lower nibble, and the upper part. Then combine after
  // rounding.
  T vs2_l = vs2 & 0xf;
  T vs1_l = vs1 & 0xf;
  T res_l = vs2_l - vs1_l;
  T res_h = (vs2 >> 4) - (vs1 >> 4);
  // Subtract borrow.
  res_h -= ((res_l >> 4) & 0b1);
  // Use unsigned type to avoid undefined behavior of left-shifting negative
  // numbers.
  using UT = typename std::make_unsigned<T>::type;
  T res = (absl::bit_cast<UT>(res_h) << 3) | ((res_l >> 1) & 0b111);
  res += tester->RoundBits<T>(2, res_l);
  return res;
}

// Vaaddu vector-vector test helper function.
template <typename T>
inline void VaadduVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vaaddu);
  tester->BinaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vaaddu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [tester](T val0, T val1) -> T {
        return VaaddHelper(tester, val0, val1);
      });
}

// Vaaddu vector-scalar test helper function.
template <typename T>
inline void VaadduVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vaaddu);
  tester->BinaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vaaddu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [tester](T val0, T val1) -> T {
        return VaaddHelper(tester, val0, val1);
      });
}

// Test Vaaddu instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vaaddu) {
  // vector-vector.
  VaadduVVHelper<uint8_t>(this);
  ResetInstruction();
  VaadduVVHelper<uint16_t>(this);
  ResetInstruction();
  VaadduVVHelper<uint32_t>(this);
  ResetInstruction();
  VaadduVVHelper<uint64_t>(this);
  ResetInstruction();
  // vector-scalar.
  VaadduVXHelper<uint8_t>(this);
  ResetInstruction();
  VaadduVXHelper<uint16_t>(this);
  ResetInstruction();
  VaadduVXHelper<uint32_t>(this);
  ResetInstruction();
  VaadduVXHelper<uint64_t>(this);
}

// Vaadd vector-vector test helper function.
template <typename T>
inline void VaaddVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vaadd);
  tester->BinaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vaaddu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [tester](T val0, T val1) -> T {
        return VaaddHelper(tester, val0, val1);
      });
}

// Vaadd vector-vector test helper function.
template <typename T>
inline void VaaddVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vaadd);
  tester->BinaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vaaddu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [tester](T val0, T val1) -> T {
        return VaaddHelper(tester, val0, val1);
      });
}

// Test Vaadd (signed) instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vaadd) {
  // Vector-vector.
  VaaddVVHelper<int8_t>(this);
  ResetInstruction();
  VaaddVVHelper<int16_t>(this);
  ResetInstruction();
  VaaddVVHelper<int32_t>(this);
  ResetInstruction();
  VaaddVVHelper<int64_t>(this);
  // Vector-scalar.
  ResetInstruction();
  VaaddVXHelper<int8_t>(this);
  ResetInstruction();
  VaaddVXHelper<int16_t>(this);
  ResetInstruction();
  VaaddVXHelper<int32_t>(this);
  ResetInstruction();
  VaaddVXHelper<int64_t>(this);
}

// Vasubu vector-vector test helper function.
template <typename T>
inline void VasubuVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vasubu);
  tester->BinaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vasubu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [tester](T val0, T val1) -> T {
        return VasubHelper(tester, val0, val1);
      });
}
// Vasubu vector-scalar test helper function.
template <typename T>
inline void VasubuVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vasubu);
  tester->BinaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vasubu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [tester](T val0, T val1) -> T {
        return VasubHelper(tester, val0, val1);
      });
}

// Test Vasubu (unsigned) instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vasubu) {
  // Vector-vector.
  VasubuVVHelper<uint8_t>(this);
  ResetInstruction();
  VasubuVVHelper<uint16_t>(this);
  ResetInstruction();
  VasubuVVHelper<uint32_t>(this);
  ResetInstruction();
  VasubuVVHelper<uint64_t>(this);
  ResetInstruction();
  // Vector-scalar.
  VasubuVXHelper<uint8_t>(this);
  ResetInstruction();
  VasubuVXHelper<uint16_t>(this);
  ResetInstruction();
  VasubuVXHelper<uint32_t>(this);
  ResetInstruction();
  VasubuVXHelper<uint64_t>(this);
}

// Vasub vector-vector test helper function.
template <typename T>
inline void VasubVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vasub);
  tester->BinaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vasub", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [tester](T val0, T val1) -> T {
        return VasubHelper(tester, val0, val1);
      });
}
// Vasub vector-scalar test helper function.
template <typename T>
inline void VasubVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vasub);
  tester->BinaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vasub", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [tester](T val0, T val1) -> T {
        return VasubHelper(tester, val0, val1);
      });
}

// Test Vasub (signed) instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vasub) {
  // Vector-vector.
  VasubVVHelper<int8_t>(this);
  ResetInstruction();
  VasubVVHelper<int16_t>(this);
  ResetInstruction();
  VasubVVHelper<int32_t>(this);
  ResetInstruction();
  VasubVVHelper<int64_t>(this);
  ResetInstruction();
  // Vector-scalar.
  VasubVXHelper<int8_t>(this);
  ResetInstruction();
  VasubVXHelper<int16_t>(this);
  ResetInstruction();
  VasubVXHelper<int32_t>(this);
  ResetInstruction();
  VasubVXHelper<int64_t>(this);
}

// Testing instructions that perform logical operations on vector masks.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmandnot) {
  SetSemanticFunction(&Vmandnot);
  BinaryLogicalMaskOpTestHelper(
      "Vmandnot", [](bool vs2, bool vs1) -> bool { return vs2 && !vs1; });
}

TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmand) {
  SetSemanticFunction(&Vmand);
  BinaryLogicalMaskOpTestHelper(
      "Vmand", [](bool vs2, bool vs1) -> bool { return vs2 && vs1; });
}

TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmor) {
  SetSemanticFunction(&Vmor);
  BinaryLogicalMaskOpTestHelper(
      "Vmor", [](bool vs2, bool vs1) -> bool { return vs2 || vs1; });
}

TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmxor) {
  SetSemanticFunction(&Vmxor);
  BinaryLogicalMaskOpTestHelper("Vmxor", [](bool vs2, bool vs1) -> bool {
    return (vs1 && !vs2) || (!vs1 && vs2);
  });
}

TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmornot) {
  SetSemanticFunction(&Vmornot);
  BinaryLogicalMaskOpTestHelper(
      "Vmornot", [](bool vs2, bool vs1) -> bool { return vs2 || !vs1; });
}

TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmnand) {
  SetSemanticFunction(&Vmnand);
  BinaryLogicalMaskOpTestHelper(
      "Vmnand", [](bool vs2, bool vs1) -> bool { return !(vs2 && vs1); });
}

TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmnor) {
  SetSemanticFunction(&Vmnor);
  BinaryLogicalMaskOpTestHelper(
      "Vmnor", [](bool vs2, bool vs1) -> bool { return !(vs2 || vs1); });
}

TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmxnor) {
  SetSemanticFunction(&Vmxnor);
  BinaryLogicalMaskOpTestHelper("Vmxnor", [](bool vs2, bool vs1) -> bool {
    return !((vs1 && !vs2) || (!vs1 && vs2));
  });
}

// Vdivu vector-vector test helper function.
template <typename T>
inline void VdivuVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vdivu);
  tester->BinaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vdivu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        if (vs1 == 0) return ~vs1;
        return vs2 / vs1;
      });
}
// Vdivu vector-scalar test helper function.
template <typename T>
inline void VdivuVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vdivu);
  tester->BinaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vdivu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        if (vs1 == 0) return ~vs1;
        return vs2 / vs1;
      });
}

// Test vdivu instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vdivu) {
  // Vector-vector.
  VdivuVVHelper<uint8_t>(this);
  ResetInstruction();
  VdivuVVHelper<uint16_t>(this);
  ResetInstruction();
  VdivuVVHelper<uint32_t>(this);
  ResetInstruction();
  VdivuVVHelper<uint64_t>(this);
  ResetInstruction();
  // Vector-scalar.
  VdivuVXHelper<uint8_t>(this);
  ResetInstruction();
  VdivuVXHelper<uint16_t>(this);
  ResetInstruction();
  VdivuVXHelper<uint32_t>(this);
  ResetInstruction();
  VdivuVXHelper<uint64_t>(this);
}

// Vdiv vector-vector test helper function.
template <typename T>
inline void VdivVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vdiv);
  tester->BinaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vdiv", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        if (vs1 == 0) return ~vs1;
        return vs2 / vs1;
      });
}
// Vdiv vector-scalar test helper function.
template <typename T>
inline void VdivVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vdiv);
  tester->BinaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vdiv", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        if (vs1 == 0) return ~vs1;
        return vs2 / vs1;
      });
}

// Test vector-vector vdiv instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vdiv) {
  // Vector-vector.
  VdivVVHelper<int8_t>(this);
  ResetInstruction();
  VdivVVHelper<int16_t>(this);
  ResetInstruction();
  VdivVVHelper<int32_t>(this);
  ResetInstruction();
  VdivVVHelper<int64_t>(this);
  ResetInstruction();
  // Vector-scalar.
  VdivVXHelper<int8_t>(this);
  ResetInstruction();
  VdivVXHelper<int16_t>(this);
  ResetInstruction();
  VdivVXHelper<int32_t>(this);
  ResetInstruction();
  VdivVXHelper<int64_t>(this);
}

// Vremu vector-vector test helper function.
template <typename T>
inline void VremuVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vremu);
  tester->BinaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vremu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        if (vs1 == 0) return vs2;
        return vs2 % vs1;
      });
}
// Vremu vector-scalar test helper function.
template <typename T>
inline void VremuVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vremu);
  tester->BinaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vremu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        if (vs1 == 0) return vs2;
        return vs2 % vs1;
      });
}

// Test vremu instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vremu) {
  // Vector-vector.
  VremuVVHelper<uint8_t>(this);
  ResetInstruction();
  VremuVVHelper<uint16_t>(this);
  ResetInstruction();
  VremuVVHelper<uint32_t>(this);
  ResetInstruction();
  VremuVVHelper<uint64_t>(this);
  ResetInstruction();
  // Vector-scalar.
  VremuVXHelper<uint8_t>(this);
  ResetInstruction();
  VremuVXHelper<uint16_t>(this);
  ResetInstruction();
  VremuVXHelper<uint32_t>(this);
  ResetInstruction();
  VremuVXHelper<uint64_t>(this);
}

// Vrem vector-vector test helper function.
template <typename T>
inline void VremVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vrem);
  tester->BinaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vrem", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        if (vs1 == 0) return vs2;
        return vs2 % vs1;
      });
}
// Vrem vector-scalar test helper function.
template <typename T>
inline void VremVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vrem);
  tester->BinaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vrem", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        if (vs1 == 0) return vs2;
        return vs2 % vs1;
      });
}

// Test vector-vector vrem instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vrem) {
  // vector-vector.
  VremVVHelper<int8_t>(this);
  ResetInstruction();
  VremVVHelper<int16_t>(this);
  ResetInstruction();
  VremVVHelper<int32_t>(this);
  ResetInstruction();
  VremVVHelper<int64_t>(this);
  ResetInstruction();
  // vector-scalar.
  VremVXHelper<int8_t>(this);
  ResetInstruction();
  VremVXHelper<int16_t>(this);
  ResetInstruction();
  VremVXHelper<int32_t>(this);
  ResetInstruction();
  VremVXHelper<int64_t>(this);
}

// Vmulhu vector-vector test helper function.
template <typename T>
inline void VmulhuVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vmulhu);
  tester->BinaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vmulhu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        absl::uint128 vs2_w = static_cast<absl::uint128>(vs2);
        absl::uint128 vs1_w = static_cast<absl::uint128>(vs1);
        absl::uint128 vd_w = (vs2_w * vs1_w) >> (sizeof(T) * 8);
        return static_cast<T>(vd_w);
      });
}
// Vmulhu vector-scalar test helper function.
template <typename T>
inline void VmulhuVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vmulhu);
  tester->BinaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vmulhu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        absl::uint128 vs2_w = static_cast<absl::uint128>(vs2);
        absl::uint128 vs1_w = static_cast<absl::uint128>(vs1);
        absl::uint128 vd_w = (vs2_w * vs1_w) >> (sizeof(T) * 8);
        return static_cast<T>(vd_w);
      });
}

// Test vector-vector vmulhu instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmulhu) {
  // vector-vector.
  VmulhuVVHelper<uint8_t>(this);
  ResetInstruction();
  VmulhuVVHelper<uint16_t>(this);
  ResetInstruction();
  VmulhuVVHelper<uint32_t>(this);
  ResetInstruction();
  VmulhuVVHelper<uint64_t>(this);
  ResetInstruction();
  // vector-scalar.
  VmulhuVXHelper<uint8_t>(this);
  ResetInstruction();
  VmulhuVXHelper<uint16_t>(this);
  ResetInstruction();
  VmulhuVXHelper<uint32_t>(this);
  ResetInstruction();
  VmulhuVXHelper<uint64_t>(this);
}

// Vmulh vector-vector test helper function.
template <typename T>
inline void VmulhVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vmulh);
  tester->BinaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vmulh", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        absl::int128 vs2_w = static_cast<absl::int128>(vs2);
        absl::int128 vs1_w = static_cast<absl::int128>(vs1);
        absl::int128 vd_w = (vs2_w * vs1_w) >> (sizeof(T) * 8);
        return static_cast<T>(vd_w);
      });
}

// Vmulh vector-scalar test helper function.
template <typename T>
inline void VmulhVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vmulh);
  tester->BinaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vmulh", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        absl::int128 vs2_w = static_cast<absl::int128>(vs2);
        absl::int128 vs1_w = static_cast<absl::int128>(vs1);
        absl::int128 vd_w = (vs2_w * vs1_w) >> (sizeof(T) * 8);
        return static_cast<T>(vd_w);
      });
}

// Test vector-vector vmulh instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmulh) {
  // vector-vector.
  VmulhVVHelper<int8_t>(this);
  ResetInstruction();
  VmulhVVHelper<int16_t>(this);
  ResetInstruction();
  VmulhVVHelper<int32_t>(this);
  ResetInstruction();
  VmulhVVHelper<int64_t>(this);
  ResetInstruction();
  // vector-scalar.
  VmulhVXHelper<int8_t>(this);
  ResetInstruction();
  VmulhVXHelper<int16_t>(this);
  ResetInstruction();
  VmulhVXHelper<int32_t>(this);
  ResetInstruction();
  VmulhVXHelper<int64_t>(this);
}

// Vmul vector-vector test helper function.
template <typename T>
inline void VmulVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vmul);
  tester->BinaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vmul", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        return static_cast<T>(static_cast<WT>(vs2) * static_cast<WT>(vs1));
      });
}

// Vmul vector-scalar test helper function.
template <typename T>
inline void VmulVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vmul);
  tester->BinaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vmul", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> T {
        return static_cast<T>(static_cast<WT>(vs2) * static_cast<WT>(vs1));
      });
}

// Test vmulh instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmul) {
  // vector-vector.
  VmulVVHelper<int8_t>(this);
  ResetInstruction();
  VmulVVHelper<int16_t>(this);
  ResetInstruction();
  VmulVVHelper<int32_t>(this);
  ResetInstruction();
  VmulVVHelper<int64_t>(this);
  ResetInstruction();
  // vector-scalar.
  VmulVXHelper<int8_t>(this);
  ResetInstruction();
  VmulVXHelper<int16_t>(this);
  ResetInstruction();
  VmulVXHelper<int32_t>(this);
  ResetInstruction();
  VmulVXHelper<int64_t>(this);
}

// Vmulhsu vector-vector test helper function.
template <typename T>
inline void VmulhsuVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using ST = typename std::make_signed<T>::type;
  tester->SetSemanticFunction(&Vmulhsu);
  tester->BinaryOpTestHelperVV<T, ST, T>(
      absl::StrCat("Vmulhsu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](ST vs2, T vs1) -> T {
        absl::int128 vs2_w = static_cast<absl::int128>(vs2);
        absl::int128 vs1_w = static_cast<absl::int128>(vs1);
        absl::int128 res = (vs2_w * vs1_w) >> (sizeof(T) * 8);
        return static_cast<ST>(res);
      });
}

// Vmulhsu vector-scalar test helper function.
template <typename T>
inline void VmulhsuVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using ST = typename std::make_signed<T>::type;
  tester->SetSemanticFunction(&Vmulhsu);
  tester->BinaryOpTestHelperVX<T, ST, T>(
      absl::StrCat("Vmulhsu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](ST vs2, T vs1) -> T {
        absl::int128 vs2_w = static_cast<absl::int128>(vs2);
        absl::int128 vs1_w = static_cast<absl::int128>(vs1);
        absl::int128 res = (vs2_w * vs1_w) >> (sizeof(T) * 8);
        return static_cast<ST>(res);
      });
}

// Test vmulhsu instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmulhsu) {
  // vector-vector
  VmulhsuVVHelper<uint8_t>(this);
  ResetInstruction();
  VmulhsuVVHelper<uint16_t>(this);
  ResetInstruction();
  VmulhsuVVHelper<uint32_t>(this);
  ResetInstruction();
  VmulhsuVVHelper<uint64_t>(this);
  ResetInstruction();
  // vector-scalar
  VmulhsuVXHelper<uint8_t>(this);
  ResetInstruction();
  VmulhsuVXHelper<uint16_t>(this);
  ResetInstruction();
  VmulhsuVXHelper<uint32_t>(this);
  ResetInstruction();
  VmulhsuVXHelper<uint64_t>(this);
}

// Vmadd vector-vector test helper function.
template <typename T>
inline void VmaddVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vmadd);
  tester->TernaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vmadd", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1, T vd) {
        if (sizeof(T) < 4) {
          uint32_t vs1_32 = vs1;
          uint32_t vs2_32 = vs2;
          uint32_t vd_32 = vd;
          return static_cast<T>((vs1_32 * vd_32) + vs2_32);
        }
        T res = vs1 * vd + vs2;
        return res;
      });
}

// Vmadd vector-scalar test helper function.
template <typename T>
inline void VmaddVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vmadd);
  tester->TernaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vmadd", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1, T vd) {
        if (sizeof(T) < 4) {
          uint32_t vs1_32 = vs1;
          uint32_t vs2_32 = vs2;
          uint32_t vd_32 = vd;
          return static_cast<T>((vs1_32 * vd_32) + vs2_32);
        }
        T res = vs1 * vd + vs2;
        return res;
      });
}

// Test vmadd instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmadd) {
  // vector-vector
  VmaddVVHelper<uint8_t>(this);
  ResetInstruction();
  VmaddVVHelper<uint16_t>(this);
  ResetInstruction();
  VmaddVVHelper<uint32_t>(this);
  ResetInstruction();
  VmaddVVHelper<uint64_t>(this);
  ResetInstruction();
  // vector-scalar
  VmaddVXHelper<uint8_t>(this);
  ResetInstruction();
  VmaddVXHelper<uint16_t>(this);
  ResetInstruction();
  VmaddVXHelper<uint32_t>(this);
  ResetInstruction();
  VmaddVXHelper<uint64_t>(this);
}

// Vnmsub vector-vector test helper function.
template <typename T>
inline void VnmsubVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vnmsub);
  tester->TernaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vnmsub", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1, T vd) {
        if (sizeof(T) < 4) {
          uint32_t vs1_32 = vs1;
          uint32_t vs2_32 = vs2;
          uint32_t vd_32 = vd;
          return static_cast<T>(-(vs1_32 * vd_32) + vs2_32);
        }
        T res = -(vs1 * vd) + vs2;
        return res;
      });
}

// Vnmsub vector-scalar test helper function.
template <typename T>
inline void VnmsubVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vnmsub);
  tester->TernaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vnmsub", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1, T vd) {
        if (sizeof(T) < 4) {
          uint32_t vs1_32 = vs1;
          uint32_t vs2_32 = vs2;
          uint32_t vd_32 = vd;
          return static_cast<T>(-(vs1_32 * vd_32) + vs2_32);
        }
        T res = -(vs1 * vd) + vs2;
        return res;
      });
}

// Test vnmsub instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vnmsub) {
  // vector-vector
  VnmsubVVHelper<uint8_t>(this);
  ResetInstruction();
  VnmsubVVHelper<uint16_t>(this);
  ResetInstruction();
  VnmsubVVHelper<uint32_t>(this);
  ResetInstruction();
  VnmsubVVHelper<uint64_t>(this);
  ResetInstruction();
  // vector-scalar
  VnmsubVXHelper<uint8_t>(this);
  ResetInstruction();
  VnmsubVXHelper<uint16_t>(this);
  ResetInstruction();
  VnmsubVXHelper<uint32_t>(this);
  ResetInstruction();
  VnmsubVXHelper<uint64_t>(this);
}

// Vmacc vector-vector test helper function.
template <typename T>
inline void VmaccVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vmacc);
  tester->TernaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vmacc", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1, T vd) {
        if (sizeof(T) < 4) {
          uint32_t vs1_32 = vs1;
          uint32_t vs2_32 = vs2;
          uint32_t vd_32 = vd;
          return static_cast<T>((vs1_32 * vs2_32) + vd_32);
        }
        T res = (vs1 * vs2) + vd;
        return res;
      });
}

// Vmacc vector-scalar test helper function.
template <typename T>
inline void VmaccVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vmacc);
  tester->TernaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vmacc", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1, T vd) {
        if (sizeof(T) < 4) {
          uint32_t vs1_32 = vs1;
          uint32_t vs2_32 = vs2;
          uint32_t vd_32 = vd;
          return static_cast<T>((vs1_32 * vs2_32) + vd_32);
        }
        T res = (vs1 * vs2) + vd;
        return res;
      });
}

// Test vmacc instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vmacc) {
  // vector-vector
  VmaccVVHelper<uint8_t>(this);
  ResetInstruction();
  VmaccVVHelper<uint16_t>(this);
  ResetInstruction();
  VmaccVVHelper<uint32_t>(this);
  ResetInstruction();
  VmaccVVHelper<uint64_t>(this);
  ResetInstruction();
  // vector-scalar
  VmaccVXHelper<uint8_t>(this);
  ResetInstruction();
  VmaccVXHelper<uint16_t>(this);
  ResetInstruction();
  VmaccVXHelper<uint32_t>(this);
  ResetInstruction();
  VmaccVXHelper<uint64_t>(this);
}

// Vnmsac vector-vector test helper function.
template <typename T>
inline void VnmsacVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vnmsac);
  tester->TernaryOpTestHelperVV<T, T, T>(
      absl::StrCat("Vnmsac", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1, T vd) {
        if (sizeof(T) < 4) {
          uint32_t vs1_32 = vs1;
          uint32_t vs2_32 = vs2;
          uint32_t vd_32 = vd;
          return static_cast<T>(-(vs1_32 * vs2_32) + vd_32);
        }
        T res = -(vs1 * vs2) + vd;
        return res;
      });
}

// Vnmsac vector-scalar test helper function.
template <typename T>
inline void VnmsacVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  tester->SetSemanticFunction(&Vnmsac);
  tester->TernaryOpTestHelperVX<T, T, T>(
      absl::StrCat("Vnmsac", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1, T vd) {
        if (sizeof(T) < 4) {
          uint32_t vs1_32 = vs1;
          uint32_t vs2_32 = vs2;
          uint32_t vd_32 = vd;
          return static_cast<T>(-(vs1_32 * vs2_32) + vd_32);
        }
        T res = -(vs1 * vs2) + vd;
        return res;
      });
}

// Test vnmsac instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vnmsac) {
  // vector-vector
  VnmsacVVHelper<uint8_t>(this);
  ResetInstruction();
  VnmsacVVHelper<uint16_t>(this);
  ResetInstruction();
  VnmsacVVHelper<uint32_t>(this);
  ResetInstruction();
  VnmsacVVHelper<uint64_t>(this);
  ResetInstruction();
  // vector-scalar
  VnmsacVXHelper<uint8_t>(this);
  ResetInstruction();
  VnmsacVXHelper<uint16_t>(this);
  ResetInstruction();
  VnmsacVXHelper<uint32_t>(this);
  ResetInstruction();
  VnmsacVXHelper<uint64_t>(this);
}

// Vwaddu vector-vector test helper function.
template <typename T>
inline void VwadduVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwaddu);
  tester->BinaryOpTestHelperVV<WT, T, T>(
      absl::StrCat("Vwaddu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> WT {
        return static_cast<WT>(vs2) + static_cast<WT>(vs1);
      });
}

// Vwaddu vector-scalar test helper function.
template <typename T>
inline void VwadduVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwaddu);
  tester->BinaryOpTestHelperVX<WT, T, T>(
      absl::StrCat("Vwaddu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> WT {
        return static_cast<WT>(vs2) + static_cast<WT>(vs1);
      });
}

// Vector widening unsigned add. (sew * 2) = sew + sew
// There is no test for sew == 64 bits, as this is a widening operation,
// and 64 bit values are the max sized vector elements.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwaddu) {
  // vector-vector.
  VwadduVVHelper<uint8_t>(this);
  ResetInstruction();
  VwadduVVHelper<uint16_t>(this);
  ResetInstruction();
  VwadduVVHelper<uint32_t>(this);
  ResetInstruction();
  // vector-scalar.
  VwadduVXHelper<uint8_t>(this);
  ResetInstruction();
  VwadduVXHelper<uint16_t>(this);
  ResetInstruction();
  VwadduVXHelper<uint32_t>(this);
}

// Vwsubu vector-vector test helper function.
template <typename T>
inline void VwsubuVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwsubu);
  tester->BinaryOpTestHelperVV<WT, T, T>(
      absl::StrCat("Vwsubu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> WT {
        return static_cast<WT>(vs2) - static_cast<WT>(vs1);
      });
}

// Vwsubu vector-scalar test helper function.
template <typename T>
inline void VwsubuVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwsubu);
  tester->BinaryOpTestHelperVX<WT, T, T>(
      absl::StrCat("Vwsubu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> WT {
        return static_cast<WT>(vs2) - static_cast<WT>(vs1);
      });
}

// Vector widening unsigned subtract. (sew * 2) = sew + sew
// There is no test for sew == 64 bits, as this is a widening operation,
// and 64 bit values are the max sized vector elements.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwsubu) {
  // vector-vector.
  VwsubuVVHelper<uint8_t>(this);
  ResetInstruction();
  VwsubuVVHelper<uint16_t>(this);
  ResetInstruction();
  VwsubuVVHelper<uint32_t>(this);
  ResetInstruction();
  // vector-scalar.
  VwsubuVXHelper<uint8_t>(this);
  ResetInstruction();
  VwsubuVXHelper<uint16_t>(this);
  ResetInstruction();
  VwsubuVXHelper<uint32_t>(this);
}

// Vwadd vector-vector test helper function.
template <typename T>
inline void VwaddVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwadd);
  tester->BinaryOpTestHelperVV<WT, T, T>(
      absl::StrCat("Vwadd", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> WT {
        return static_cast<WT>(vs2) + static_cast<WT>(vs1);
      });
}

// Vwadd vector-scalar test helper function.
template <typename T>
inline void VwaddVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwadd);
  tester->BinaryOpTestHelperVX<WT, T, T>(
      absl::StrCat("Vwadd", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> WT {
        return static_cast<WT>(vs2) + static_cast<WT>(vs1);
      });
}

// Vector videning signed addition. (sew * 2) = sew + sew.
// There is no test for sew == 64 bits, as this is a widening operation,
// and 64 bit values are the max sized vector elements.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwadd) {
  // vector-vector.
  VwaddVVHelper<int8_t>(this);
  ResetInstruction();
  VwaddVVHelper<int16_t>(this);
  ResetInstruction();
  VwaddVVHelper<int32_t>(this);
  ResetInstruction();
  // vector-scalar.
  VwaddVXHelper<int8_t>(this);
  ResetInstruction();
  VwaddVXHelper<int16_t>(this);
  ResetInstruction();
  VwaddVXHelper<int32_t>(this);
}

// Vwsub vector-vector test helper function.
template <typename T>
inline void VwsubVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwsub);
  tester->BinaryOpTestHelperVV<WT, T, T>(
      absl::StrCat("Vwsub", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> WT {
        WT vs2_w = vs2;
        WT vs1_w = vs1;
        WT res = vs2_w - vs1_w;
        return res;
      });
}

// Vwsub vector-scalar test helper function.
template <typename T>
inline void VwsubVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwsub);
  tester->BinaryOpTestHelperVX<WT, T, T>(
      absl::StrCat("Vwsub", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> WT {
        WT vs2_w = vs2;
        WT vs1_w = vs1;
        WT res = vs2_w - vs1_w;
        return res;
      });
}

// Vector widening unsigned subtract. (sew * 2) = sew + sew
// There is no test for sew == 64 bits, as this is a widening operation,
// and 64 bit values are the max sized vector elements.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwsub) {
  // vector-vector.
  VwsubVVHelper<int8_t>(this);
  ResetInstruction();
  VwsubVVHelper<int16_t>(this);
  ResetInstruction();
  VwsubVVHelper<int32_t>(this);
  ResetInstruction();
  // vector-scalar.
  VwsubVXHelper<int8_t>(this);
  ResetInstruction();
  VwsubVXHelper<int16_t>(this);
  ResetInstruction();
  VwsubVXHelper<int32_t>(this);
}

// Vwadduw vector-vector test helper function.
template <typename T>
inline void VwadduwVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwadduw);
  tester->BinaryOpTestHelperVV<WT, WT, T>(
      absl::StrCat("Vwadduw", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(),
      [](WT vs2, T vs1) -> WT { return vs2 + static_cast<WT>(vs1); });
}

// Vwadduw vector-scalar test helper function.
template <typename T>
inline void VwadduwVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwadduw);
  tester->BinaryOpTestHelperVX<WT, WT, T>(
      absl::StrCat("Vwadduw", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(),
      [](WT vs2, T vs1) -> WT { return vs2 + static_cast<WT>(vs1); });
}

// Vector widening unsigned add. (sew * 2) = (sew * 2) + sew
// There is no test for sew == 64 bits, as this is a widening operation,
// and 64 bit values are the max sized vector elements.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwadduw) {
  // vector-vector.
  VwadduwVVHelper<uint8_t>(this);
  ResetInstruction();
  VwadduwVVHelper<uint16_t>(this);
  ResetInstruction();
  VwadduwVVHelper<uint32_t>(this);
  ResetInstruction();
  // vector-scalar.
  VwadduwVXHelper<uint8_t>(this);
  ResetInstruction();
  VwadduwVXHelper<uint16_t>(this);
  ResetInstruction();
  VwadduwVXHelper<uint32_t>(this);
}

// Vwsubuw vector-vector test helper function.
template <typename T>
inline void VwsubuwVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwsubuw);
  tester->BinaryOpTestHelperVV<WT, WT, T>(
      absl::StrCat("Vwsubuw", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(),
      [](WT vs2, T vs1) -> WT { return vs2 - static_cast<WT>(vs1); });
}

// Vwsubuw vector-scalar test helper function.
template <typename T>
inline void VwsubuwVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwsubuw);
  tester->BinaryOpTestHelperVX<WT, WT, T>(
      absl::StrCat("Vwsubuw", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(),
      [](WT vs2, T vs1) -> WT { return vs2 - static_cast<WT>(vs1); });
}

// Vector widening unsigned subtract. (sew * 2) = (sew * 2) + sew
// There is no test for sew == 64 bits, as this is a widening operation,
// and 64 bit values are the max sized vector elements.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwsubuw) {
  // vector-vector.
  VwsubuwVVHelper<uint8_t>(this);
  ResetInstruction();
  VwsubuwVVHelper<uint16_t>(this);
  ResetInstruction();
  VwsubuwVVHelper<uint32_t>(this);
  ResetInstruction();
  // vector-scalar.
  VwsubuwVXHelper<uint8_t>(this);
  ResetInstruction();
  VwsubuwVXHelper<uint16_t>(this);
  ResetInstruction();
  VwsubuwVXHelper<uint32_t>(this);
}

// Vwaddw vector-vector test helper function.
template <typename T>
inline void VwaddwVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwaddw);
  tester->BinaryOpTestHelperVV<WT, WT, T>(
      absl::StrCat("Vwaddw", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(),
      [](WT vs2, T vs1) -> WT { return vs2 + static_cast<WT>(vs1); });
}

// Vwaddw vector-scalar test helper function.
template <typename T>
inline void VwaddwVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwaddw);
  tester->BinaryOpTestHelperVX<WT, WT, T>(
      absl::StrCat("Vwaddw", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(),
      [](WT vs2, T vs1) -> WT { return vs2 + static_cast<WT>(vs1); });
}

// Vector widening signed add. (sew * 2) = (sew * 2) + sew
// There is no test for sew == 64 bits, as this is a widening operation,
// and 64 bit values are the max sized vector elements.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwaddw) {
  // vector-vector.
  VwaddwVVHelper<int8_t>(this);
  ResetInstruction();
  VwaddwVVHelper<int16_t>(this);
  ResetInstruction();
  VwaddwVVHelper<int32_t>(this);
  ResetInstruction();
  // vector-scalar.
  VwaddwVXHelper<int8_t>(this);
  ResetInstruction();
  VwaddwVXHelper<int16_t>(this);
  ResetInstruction();
  VwaddwVXHelper<int32_t>(this);
}

// Vwsubw vector-vector test helper function.
template <typename T>
inline void VwsubwVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwsubw);
  tester->BinaryOpTestHelperVV<WT, WT, T>(
      absl::StrCat("Vwsubw", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(),
      [](WT vs2, T vs1) -> WT { return vs2 - static_cast<WT>(vs1); });
}

// Vwsubw vector-scalar test helper function.
template <typename T>
inline void VwsubwVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwsubw);
  tester->BinaryOpTestHelperVX<WT, WT, T>(
      absl::StrCat("Vwsubw", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(),
      [](WT vs2, T vs1) -> WT { return vs2 - static_cast<WT>(vs1); });
}

// Vector widening signed subtract. (sew * 2) = (sew * 2) + sew
// There is no test for sew == 64 bits, as this is a widening operation,
// and 64 bit values are the max sized vector elements.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwsubw) {
  // vector-vector.
  VwsubwVVHelper<int8_t>(this);
  ResetInstruction();
  VwsubwVVHelper<int16_t>(this);
  ResetInstruction();
  VwsubwVVHelper<int32_t>(this);
  ResetInstruction();
  // vector-scalar.
  VwsubwVXHelper<int8_t>(this);
  ResetInstruction();
  VwsubwVXHelper<int16_t>(this);
  ResetInstruction();
  VwsubwVXHelper<int32_t>(this);
}

// Vwmul vector-vector test helper function.
template <typename T>
inline void VwmuluVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwmulu);
  tester->BinaryOpTestHelperVV<WT, T, T>(
      absl::StrCat("Vwmulu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> WT {
        return static_cast<WT>(vs2) * static_cast<WT>(vs1);
      });
}

// Vwmulu vector-scalar test helper function.
template <typename T>
inline void VwmuluVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwmulu);
  tester->BinaryOpTestHelperVX<WT, T, T>(
      absl::StrCat("Vwmulu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> WT {
        return static_cast<WT>(vs2) * static_cast<WT>(vs1);
      });
}

// Vector widening signed multiply. (sew * 2) = sew + sew
// There is no test for sew == 64 bits, as this is a widening operation,
// and 64 bit values are the max sized vector elements.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwmulu) {
  // vector-vector.
  VwmuluVVHelper<uint8_t>(this);
  ResetInstruction();
  VwmuluVVHelper<uint16_t>(this);
  ResetInstruction();
  VwmuluVVHelper<uint32_t>(this);
  ResetInstruction();
  // vector-scalar.
  VwmuluVXHelper<uint8_t>(this);
  ResetInstruction();
  VwmuluVXHelper<uint16_t>(this);
  ResetInstruction();
  VwmuluVXHelper<uint32_t>(this);
}

// Vwmul vector-vector test helper function.
template <typename T>
inline void VwmulVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwmul);
  tester->BinaryOpTestHelperVV<WT, T, T>(
      absl::StrCat("Vwmul", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> WT {
        return static_cast<WT>(vs2) * static_cast<WT>(vs1);
      });
}

// Vwmul vector-scalar test helper function.
template <typename T>
inline void VwmulVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwmul);
  tester->BinaryOpTestHelperVX<WT, T, T>(
      absl::StrCat("Vwmul", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1) -> WT {
        return static_cast<WT>(vs2) * static_cast<WT>(vs1);
      });
}

// Vector widening signed multiply. (sew * 2) = sew + sew
// There is no test for sew == 64 bits, as this is a widening operation,
// and 64 bit values are the max sized vector elements.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwmul) {
  // vector-vector.
  VwmulVVHelper<int8_t>(this);
  ResetInstruction();
  VwmulVVHelper<int16_t>(this);
  ResetInstruction();
  VwmulVVHelper<int32_t>(this);
  ResetInstruction();
  // vector-scalar.
  VwmulVXHelper<int8_t>(this);
  ResetInstruction();
  VwmulVXHelper<int16_t>(this);
  ResetInstruction();
  VwmulVXHelper<int32_t>(this);
}

// Vwmul vector-vector test helper function.
template <typename T>
inline void VwmulsuVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  using UT = typename std::make_unsigned<T>::type;
  tester->SetSemanticFunction(&Vwmulsu);
  tester->BinaryOpTestHelperVV<WT, T, T>(
      absl::StrCat("Vwmulsu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, UT vs1) -> WT {
        return static_cast<WT>(vs2) * static_cast<WT>(vs1);
      });
}

// Vwmulsu vector-scalar test helper function.
template <typename T>
inline void VwmulsuVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  using UT = typename std::make_unsigned<T>::type;
  tester->SetSemanticFunction(&Vwmulsu);
  tester->BinaryOpTestHelperVX<WT, T, T>(
      absl::StrCat("Vwmulsu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, UT vs1) -> WT {
        return static_cast<WT>(vs2) * static_cast<WT>(vs1);
      });
}

// Vector widening signed multiply. (sew * 2) = sew + sew
// There is no test for sew == 64 bits, as this is a widening operation,
// and 64 bit values are the max sized vector elements.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwmulsu) {
  // vector-vector.
  VwmulsuVVHelper<int8_t>(this);
  ResetInstruction();
  VwmulsuVVHelper<int16_t>(this);
  ResetInstruction();
  VwmulsuVVHelper<int32_t>(this);
  ResetInstruction();
  // vector-scalar.
  VwmulsuVXHelper<int8_t>(this);
  ResetInstruction();
  VwmulsuVXHelper<int16_t>(this);
  ResetInstruction();
  VwmulsuVXHelper<int32_t>(this);
}

// Vmaccu vector-vector test helper function.
template <typename T>
inline void VwmaccuVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwmaccu);
  tester->TernaryOpTestHelperVV<WT, T, T>(
      absl::StrCat("Vwmaccu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1, WT vd) {
        return static_cast<WT>(vs2) * static_cast<WT>(vs1) + vd;
      });
}

// Vwmaccu vector-scalar test helper function.
template <typename T>
inline void VwmaccuVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwmaccu);
  tester->TernaryOpTestHelperVX<WT, T, T>(
      absl::StrCat("Vwmaccu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1, WT vd) {
        return static_cast<WT>(vs2) * static_cast<WT>(vs1) + vd;
      });
}

// Test vwmaccu instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwmaccu) {
  // vector-vector
  VwmaccuVVHelper<uint8_t>(this);
  ResetInstruction();
  VwmaccuVVHelper<uint16_t>(this);
  ResetInstruction();
  VwmaccuVVHelper<uint32_t>(this);
  ResetInstruction();
  // vector-scalar
  VwmaccuVXHelper<uint8_t>(this);
  ResetInstruction();
  VwmaccuVXHelper<uint16_t>(this);
  ResetInstruction();
  VwmaccuVXHelper<uint32_t>(this);
}

// Vmacc vector-vector test helper function.
template <typename T>
inline void VwmaccVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwmacc);
  tester->TernaryOpTestHelperVV<WT, T, T>(
      absl::StrCat("Vwmacc", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1, WT vd) -> WT {
        WT vs1_w = vs1;
        WT vs2_w = vs2;
        WT prod = vs1_w * vs2_w;
        using UWT = typename std::make_unsigned<WT>::type;
        WT res = absl::bit_cast<UWT>(prod) + absl::bit_cast<UWT>(vd);
        return res;
      });
}

// Vwmacc vector-scalar test helper function.
template <typename T>
inline void VwmaccVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  tester->SetSemanticFunction(&Vwmacc);
  tester->TernaryOpTestHelperVX<WT, T, T>(
      absl::StrCat("Vwmacc", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, T vs1, WT vd) -> WT {
        WT vs1_w = vs1;
        WT vs2_w = vs2;
        WT prod = vs1_w * vs2_w;
        using UWT = typename std::make_unsigned<WT>::type;
        WT res = absl::bit_cast<UWT>(prod) + absl::bit_cast<UWT>(vd);
        return res;
      });
}

// Test vwmacc instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwmacc) {
  // vector-vector
  VwmaccVVHelper<int8_t>(this);
  ResetInstruction();
  VwmaccVVHelper<int16_t>(this);
  ResetInstruction();
  VwmaccVVHelper<int32_t>(this);
  ResetInstruction();
  // vector-scalar
  VwmaccVXHelper<int8_t>(this);
  ResetInstruction();
  VwmaccVXHelper<int16_t>(this);
  ResetInstruction();
  VwmaccVXHelper<int32_t>(this);
}

// Vmaccus vector-vector test helper function.
template <typename T>
inline void VwmaccusVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  using UT = typename std::make_unsigned<T>::type;
  tester->SetSemanticFunction(&Vwmaccus);
  tester->TernaryOpTestHelperVV<WT, T, UT>(
      absl::StrCat("Vwmaccus", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, UT vs1, WT vd) -> WT {
        using UWT = typename std::make_unsigned<WT>::type;
        UWT vs1_w = vs1;
        WT vs2_w = vs2;
        WT prod = vs1_w * vs2_w;
        WT res = absl::bit_cast<UWT>(prod) + absl::bit_cast<UWT>(vd);
        return res;
      });
}

// Vwmaccus vector-scalar test helper function.
template <typename T>
inline void VwmaccusVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  using UT = typename std::make_unsigned<T>::type;
  tester->SetSemanticFunction(&Vwmaccus);
  tester->TernaryOpTestHelperVX<WT, T, UT>(
      absl::StrCat("Vwmaccus", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](T vs2, UT vs1, WT vd) -> WT {
        using UWT = typename std::make_unsigned<WT>::type;
        UWT vs1_w = vs1;
        WT vs2_w = vs2;
        WT prod = vs1_w * vs2_w;
        WT res = absl::bit_cast<UWT>(prod) + absl::bit_cast<UWT>(vd);
        return res;
      });
}

// Test vwmaccus instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwmaccus) {
  // vector-vector
  VwmaccusVVHelper<int8_t>(this);
  ResetInstruction();
  VwmaccusVVHelper<int16_t>(this);
  ResetInstruction();
  VwmaccusVVHelper<int32_t>(this);
  ResetInstruction();
  // vector-scalar
  VwmaccusVXHelper<int8_t>(this);
  ResetInstruction();
  VwmaccusVXHelper<int16_t>(this);
  ResetInstruction();
  VwmaccusVXHelper<int32_t>(this);
}

// Vmaccsu vector-vector test helper function.
template <typename T>
inline void VwmaccsuVVHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  using UT = typename std::make_unsigned<T>::type;
  tester->SetSemanticFunction(&Vwmaccsu);
  tester->TernaryOpTestHelperVV<WT, UT, T>(
      absl::StrCat("Vwmaccsu", sizeof(T) * 8, "vv"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](UT vs2, T vs1, WT vd) -> WT {
        using UWT = typename std::make_unsigned<WT>::type;
        WT vs1_w = vs1;
        UWT vs2_w = vs2;
        WT prod = vs1_w * vs2_w;
        WT res = absl::bit_cast<UWT>(prod) + absl::bit_cast<UWT>(vd);
        return res;
      });
}

// Vwmaccsu vector-scalar test helper function.
template <typename T>
inline void VwmaccsuVXHelper(RiscVCheriotVectorOpmInstructionsTest* tester) {
  using WT = typename WideType<T>::type;
  using UT = typename std::make_unsigned<T>::type;
  tester->SetSemanticFunction(&Vwmaccsu);
  tester->TernaryOpTestHelperVX<WT, UT, T>(
      absl::StrCat("Vwmaccsu", sizeof(T) * 8, "vx"), /*sew*/ sizeof(T) * 8,
      tester->instruction(), [](UT vs2, T vs1, WT vd) -> WT {
        using UWT = typename std::make_unsigned<WT>::type;
        WT vs1_w = vs1;
        UWT vs2_w = vs2;
        WT prod = vs1_w * vs2_w;
        WT res = absl::bit_cast<UWT>(prod) + absl::bit_cast<UWT>(vd);
        return res;
      });
}

// Test vwmaccsu instructions.
TEST_F(RiscVCheriotVectorOpmInstructionsTest, Vwmaccsu) {
  // vector-vector
  VwmaccsuVVHelper<int8_t>(this);
  ResetInstruction();
  VwmaccsuVVHelper<int16_t>(this);
  ResetInstruction();
  VwmaccsuVVHelper<int32_t>(this);
  ResetInstruction();
  // vector-scalar
  VwmaccsuVXHelper<int8_t>(this);
  ResetInstruction();
  VwmaccsuVXHelper<int16_t>(this);
  ResetInstruction();
  VwmaccsuVXHelper<int32_t>(this);
}

}  // namespace
