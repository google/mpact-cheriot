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

#include "cheriot/riscv_cheriot_f_instructions.h"

#include <cmath>
#include <cstdint>
#include <tuple>

#include "cheriot/test/riscv_cheriot_fp_test_base.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/instruction.h"
#include "riscv//riscv_register.h"

namespace {

using ::mpact::sim::cheriot::test::FPTypeInfo;
using ::mpact::sim::cheriot::test::OptimizationBarrier;
using ::mpact::sim::cheriot::test::RiscVFPInstructionTestBase;
using ::mpact::sim::riscv::FPExceptions;
using ::mpact::sim::riscv::RVFpRegister;

using ::mpact::sim::cheriot::RiscVFAdd;
using ::mpact::sim::cheriot::RiscVFClass;
using ::mpact::sim::cheriot::RiscVFCmpeq;
using ::mpact::sim::cheriot::RiscVFCmple;
using ::mpact::sim::cheriot::RiscVFCmplt;
using ::mpact::sim::cheriot::RiscVFCvtSw;
using ::mpact::sim::cheriot::RiscVFCvtSwu;
using ::mpact::sim::cheriot::RiscVFCvtWs;
using ::mpact::sim::cheriot::RiscVFCvtWus;
using ::mpact::sim::cheriot::RiscVFDiv;
using ::mpact::sim::cheriot::RiscVFMadd;
using ::mpact::sim::cheriot::RiscVFMax;
using ::mpact::sim::cheriot::RiscVFMin;
using ::mpact::sim::cheriot::RiscVFMsub;
using ::mpact::sim::cheriot::RiscVFMul;
using ::mpact::sim::cheriot::RiscVFNmadd;
using ::mpact::sim::cheriot::RiscVFNmsub;
using ::mpact::sim::cheriot::RiscVFSgnj;
using ::mpact::sim::cheriot::RiscVFSgnjn;
using ::mpact::sim::cheriot::RiscVFSgnjx;
using ::mpact::sim::cheriot::RiscVFSqrt;
using ::mpact::sim::cheriot::RiscVFSub;

class RVCheriot32FInstructionTest : public RiscVFPInstructionTestBase {};

static bool is_snan(float a) {
  if (!std::isnan(a)) return false;
  uint32_t ua = *reinterpret_cast<uint32_t *>(&a);
  if ((ua & (1 << (FPTypeInfo<float>::kSigSize - 1))) == 0) return true;
  return false;
}

// Test basic arithmetic instructions.
TEST_F(RVCheriot32FInstructionTest, RiscVFadd) {
  SetSemanticFunction(&RiscVFAdd);
  BinaryOpFPTestHelper<float, float, float>(
      "fadd", instruction_, {"f", "f", "f"}, 32,
      [](float lhs, float rhs) -> float { return lhs + rhs; });
}

TEST_F(RVCheriot32FInstructionTest, RiscVFsub) {
  SetSemanticFunction(&RiscVFSub);
  BinaryOpFPTestHelper<float, float, float>(
      "fsub", instruction_, {"f", "f", "f"}, 32,
      [](float lhs, float rhs) -> float { return lhs - rhs; });
}

TEST_F(RVCheriot32FInstructionTest, RiscVFmul) {
  SetSemanticFunction(&RiscVFMul);
  BinaryOpFPTestHelper<float, float, float>(
      "fmul", instruction_, {"f", "f", "f"}, 32,
      [](float lhs, float rhs) -> float { return lhs * rhs; });
}

TEST_F(RVCheriot32FInstructionTest, RiscVFdiv) {
  SetSemanticFunction(&RiscVFDiv);
  BinaryOpFPTestHelper<float, float, float>(
      "fdiv", instruction_, {"f", "f", "f"}, 32,
      [](float lhs, float rhs) -> float { return lhs / rhs; });
}

// Test square root.
TEST_F(RVCheriot32FInstructionTest, RiscVFsqrt) {
  SetSemanticFunction(&RiscVFSqrt);
}

// Test Min/Max.
TEST_F(RVCheriot32FInstructionTest, RiscVFmin) {
  SetSemanticFunction(&RiscVFMin);
  BinaryOpWithFflagsFPTestHelper<float, float, float>(
      "fmin", instruction_, {"f", "f", "f"}, 32,
      [](float lhs, float rhs) -> std::tuple<float, uint32_t> {
        uint32_t flag = 0;
        if (is_snan(lhs) || is_snan(rhs)) {
          flag = static_cast<uint32_t>(FPExceptions::kInvalidOp);
        }
        if (std::isnan(lhs) && std::isnan(rhs)) {
          uint32_t val = FPTypeInfo<float>::kCanonicalNaN;
          return std::tie(*reinterpret_cast<const float *>(&val), flag);
        }
        if (std::isnan(lhs)) return std::tie(rhs, flag);
        if (std::isnan(rhs)) return std::tie(lhs, flag);
        if ((lhs == 0.0) && (rhs == 0.0)) {
          return std::tie(std::signbit(lhs) ? lhs : rhs, flag);
        }
        return std::tie(lhs > rhs ? rhs : lhs, flag);
      });
}

TEST_F(RVCheriot32FInstructionTest, RiscVFmax) {
  SetSemanticFunction(&RiscVFMax);
  BinaryOpWithFflagsFPTestHelper<float, float, float>(
      "fmax", instruction_, {"f", "f", "f"}, 32,
      [](float lhs, float rhs) -> std::tuple<float, uint32_t> {
        uint32_t flag = 0;
        if (is_snan(lhs) || is_snan(rhs)) {
          flag = static_cast<uint32_t>(FPExceptions::kInvalidOp);
        }
        if (std::isnan(lhs) && std::isnan(rhs)) {
          uint32_t val = FPTypeInfo<float>::kCanonicalNaN;
          return std::tie(*reinterpret_cast<const float *>(&val), flag);
        }
        if (std::isnan(lhs)) return std::tie(rhs, flag);
        if (std::isnan(rhs)) return std::tie(lhs, flag);
        if ((lhs == 0.0) && (rhs == 0.0)) {
          return std::tie(std::signbit(lhs) ? rhs : lhs, flag);
        }
        return std::tie(lhs < rhs ? rhs : lhs, flag);
      });
}

// Test MAC versions.
TEST_F(RVCheriot32FInstructionTest, RiscVFMadd) {
  SetSemanticFunction(&RiscVFMadd);
  TernaryOpFPTestHelper<float, float, float, float>(
      "fmadd", instruction_, {"f", "f", "f", "f"}, 32,
      [](float lhs, float mhs, float rhs) -> float {
        return OptimizationBarrier(lhs * mhs) + rhs;
      });
}
TEST_F(RVCheriot32FInstructionTest, RiscVFMsub) {
  SetSemanticFunction(&RiscVFMsub);
  TernaryOpFPTestHelper<float, float, float, float>(
      "fmsub", instruction_, {"f", "f", "f", "f"}, 32,
      [](float lhs, float mhs, float rhs) -> float {
        return OptimizationBarrier(lhs * mhs) - rhs;
      });
}
TEST_F(RVCheriot32FInstructionTest, RiscVFNmadd) {
  SetSemanticFunction(&RiscVFNmadd);
  TernaryOpFPTestHelper<float, float, float, float>(
      "fnmadd", instruction_, {"f", "f", "f", "f"}, 32,
      [](float lhs, float mhs, float rhs) -> float {
        return -OptimizationBarrier(lhs * mhs) - rhs;
      });
}
TEST_F(RVCheriot32FInstructionTest, RiscVFNmsub) {
  SetSemanticFunction(&RiscVFNmsub);
  TernaryOpFPTestHelper<float, float, float, float>(
      "fnmsub", instruction_, {"f", "f", "f", "f"}, 32,
      [](float lhs, float mhs, float rhs) -> float {
        return -OptimizationBarrier(lhs * mhs) + rhs;
      });
}

// Test conversion instructions.
// Double to signed 32 bit integer.
TEST_F(RVCheriot32FInstructionTest, RiscVFCvtWs) {
  SetSemanticFunction(&RiscVFCvtWs);
  UnaryOpWithFflagsFPTestHelper<int32_t, float>(
      "fcvt.w.s", instruction_, {"f", "x"}, 32,
      [&](float lhs) -> std::tuple<int32_t, uint32_t> {
        if (std::isnan(lhs))
          return std::make_tuple(
              0x7fff'ffff, static_cast<uint32_t>(FPExceptions::kInvalidOp));
        if (std::isinf(lhs))
          return std::make_tuple(
              lhs < 0 ? 0x8000'0000 : 0x7fff'ffff,
              static_cast<uint32_t>(FPExceptions::kInvalidOp));
        if (abs(lhs) > static_cast<float>(0x7fff'ffff))
          return std::make_tuple(
              lhs < 0 ? 0x8000'0000 : 0x7fff'ffff,
              static_cast<uint32_t>(FPExceptions::kInvalidOp));
        uint32_t flag = 0;
        if (ceil(lhs) != lhs) {
          flag |= static_cast<uint32_t>(FPExceptions::kInexact);
          lhs = RoundToInteger(lhs);
        }
        return std::make_tuple(static_cast<int32_t>(lhs), flag);
      });
}

// Signed 32 bit integer to float.
TEST_F(RVCheriot32FInstructionTest, RiscVFCvtSw) {
  SetSemanticFunction(&RiscVFCvtSw);
  UnaryOpFPTestHelper<float, int32_t>(
      "fcvt.s.w", instruction_, {"x", "f"}, 32,
      [](int32_t lhs) -> float { return static_cast<float>(lhs); });
}

// Double to unsigned 32 bit integer.
TEST_F(RVCheriot32FInstructionTest, RiscVFCvtWus) {
  SetSemanticFunction(&RiscVFCvtWus);
  UnaryOpWithFflagsFPTestHelper<uint32_t, float>(
      "fcvt.wu.s", instruction_, {"f", "x"}, 32,
      [&](float lhs) -> std::tuple<uint32_t, uint32_t> {
        if (std::isnan(lhs))
          return std::make_tuple(
              0xffff'ffff, static_cast<uint32_t>(FPExceptions::kInvalidOp));
        if (lhs < 0) {
          if ((lhs > -1.0) && (static_cast<int32_t>(lhs) == 0.0)) {
            return std::make_tuple(
                0, static_cast<uint32_t>(FPExceptions::kInexact));
          }
          return std::make_tuple(
              0, static_cast<uint32_t>(FPExceptions::kInvalidOp));
        }
        if (std::isinf(lhs) || lhs > static_cast<float>(0xffff'ffffUL)) {
          return std::make_tuple(
              0xffff'ffff, static_cast<uint32_t>(FPExceptions::kInvalidOp));
        }
        uint32_t flag = 0;
        if (ceil(lhs) != lhs) {
          flag |= static_cast<uint32_t>(FPExceptions::kInexact);
          lhs = RoundToInteger(lhs);
        }
        return std::make_tuple(static_cast<uint32_t>(lhs), flag);
      });
}

// Unsigned 32 bit integer to float.
TEST_F(RVCheriot32FInstructionTest, RiscVFCvtSwu) {
  SetSemanticFunction(&RiscVFCvtSwu);
  UnaryOpFPTestHelper<float, uint32_t>(
      "fcvt.s.w", instruction_, {"x", "f"}, 32,
      [](uint32_t lhs) -> float { return static_cast<float>(lhs); });
}

// Test sign manipulation instructions.
TEST_F(RVCheriot32FInstructionTest, RiscVFSgnj) {
  SetSemanticFunction(&RiscVFSgnj);
  BinaryOpFPTestHelper<float, float, float>(
      "fsgnj", instruction_, {"f", "f", "f"}, 32,
      [](float lhs, float rhs) -> float { return copysign(abs(lhs), rhs); });
}

TEST_F(RVCheriot32FInstructionTest, RiscVFSgnjn) {
  SetSemanticFunction(&RiscVFSgnjn);
  BinaryOpFPTestHelper<float, float, float>(
      "fsgnjn", instruction_, {"f", "f", "f"}, 32,
      [](float lhs, float rhs) -> float { return copysign(abs(lhs), -rhs); });
}

TEST_F(RVCheriot32FInstructionTest, RiscVFSgnjx) {
  SetSemanticFunction(&RiscVFSgnjx);
  BinaryOpFPTestHelper<float, float, float>(
      "fsgnjn", instruction_, {"f", "f", "f"}, 32,
      [](float lhs, float rhs) -> float {
        auto lhs_u = *reinterpret_cast<uint32_t *>(&lhs);
        auto rhs_u = *reinterpret_cast<uint32_t *>(&rhs);
        auto res_u = (lhs_u ^ rhs_u) & 0x8000'0000;
        auto res = *reinterpret_cast<float *>(&res_u);
        return copysign(abs(lhs), res);
      });
}

// Test compare instructions.
TEST_F(RVCheriot32FInstructionTest, RiscVFCmpeq) {
  SetSemanticFunction(&RiscVFCmpeq);
  BinaryOpWithFflagsFPTestHelper<uint32_t, float, float>(
      "fcmpeq", instruction_, {"f", "f", "x"}, 32,
      [](float lhs, float rhs) -> std::tuple<uint32_t, uint32_t> {
        uint32_t flag = 0;
        if (is_snan(lhs) || is_snan(rhs)) {
          flag = static_cast<uint32_t>(FPExceptions::kInvalidOp);
        }
        return std::make_tuple(lhs == rhs, flag);
      });
}

TEST_F(RVCheriot32FInstructionTest, RiscVFCmplt) {
  SetSemanticFunction(&RiscVFCmplt);
  BinaryOpWithFflagsFPTestHelper<uint32_t, float, float>(
      "fcmplt", instruction_, {"f", "f", "x"}, 32,
      [](float lhs, float rhs) -> std::tuple<uint32_t, uint32_t> {
        uint32_t flag = 0;
        if (std::isnan(lhs) || std::isnan(rhs)) {
          flag = static_cast<uint32_t>(FPExceptions::kInvalidOp);
        }
        return std::make_tuple(lhs < rhs, flag);
      });
}

TEST_F(RVCheriot32FInstructionTest, RiscVFCmple) {
  SetSemanticFunction(&RiscVFCmple);
  BinaryOpWithFflagsFPTestHelper<uint32_t, float, float>(
      "fcmple", instruction_, {"f", "f", "x"}, 32,
      [](float lhs, float rhs) -> std::tuple<uint32_t, uint32_t> {
        uint32_t flag = 0;
        if (std::isnan(lhs) || std::isnan(rhs)) {
          flag = static_cast<uint32_t>(FPExceptions::kInvalidOp);
        }
        return std::make_tuple(lhs <= rhs, flag);
      });
}

// Test class instruction.
TEST_F(RVCheriot32FInstructionTest, RiscVFClass) {
  SetSemanticFunction(&RiscVFClass);
  UnaryOpFPTestHelper<uint32_t, float>(
      "fclass.d", instruction_, {"f", "x"}, 32, [](float lhs) -> uint32_t {
        auto fp_class = std::fpclassify(lhs);
        switch (fp_class) {
          case FP_INFINITE:
            return std::signbit(lhs) ? 1 : 1 << 7;
          case FP_NAN: {
            auto uint_val =
                *reinterpret_cast<typename FPTypeInfo<float>::IntType *>(&lhs);
            bool quiet_nan =
                (uint_val >> (FPTypeInfo<float>::kSigSize - 1)) & 1;
            return quiet_nan ? 1 << 9 : 1 << 8;
          }
          case FP_ZERO:
            return std::signbit(lhs) ? 1 << 3 : 1 << 4;
          case FP_SUBNORMAL:
            return std::signbit(lhs) ? 1 << 2 : 1 << 5;
          case FP_NORMAL:
            return std::signbit(lhs) ? 1 << 1 : 1 << 6;
        }
        return 0;
      });
}

}  // namespace
