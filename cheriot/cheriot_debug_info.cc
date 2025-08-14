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

#include "cheriot/cheriot_debug_info.h"

#include "mpact/sim/generic/type_helpers.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::operator*;  // NOLINT: is used below (clang error).

constexpr char kPcName[] = "pcc";

constexpr char kC0Name[] = "c0";
constexpr char kC1Name[] = "c1";
constexpr char kC2Name[] = "c2";
constexpr char kC3Name[] = "c3";
constexpr char kC4Name[] = "c4";
constexpr char kC5Name[] = "c5";
constexpr char kC6Name[] = "c6";
constexpr char kC7Name[] = "c7";
constexpr char kC8Name[] = "c8";
constexpr char kC9Name[] = "c9";
constexpr char kC10Name[] = "c10";
constexpr char kC11Name[] = "c11";
constexpr char kC12Name[] = "c12";
constexpr char kC13Name[] = "c13";
constexpr char kC14Name[] = "c14";
constexpr char kC15Name[] = "c15";
constexpr char kC16Name[] = "c16";
constexpr char kC17Name[] = "c17";
constexpr char kC18Name[] = "c18";
constexpr char kC19Name[] = "c19";
constexpr char kC20Name[] = "c20";
constexpr char kC21Name[] = "c21";
constexpr char kC22Name[] = "c22";
constexpr char kC23Name[] = "c23";
constexpr char kC24Name[] = "c24";
constexpr char kC25Name[] = "c25";
constexpr char kC26Name[] = "c26";
constexpr char kC27Name[] = "c27";
constexpr char kC28Name[] = "c28";
constexpr char kC29Name[] = "c29";
constexpr char kC30Name[] = "c30";
constexpr char kC31Name[] = "c31";

constexpr char kF0Name[] = "f0";
constexpr char kF1Name[] = "f1";
constexpr char kF2Name[] = "f2";
constexpr char kF3Name[] = "f3";
constexpr char kF4Name[] = "f4";
constexpr char kF5Name[] = "f5";
constexpr char kF6Name[] = "f6";
constexpr char kF7Name[] = "f7";
constexpr char kF8Name[] = "f8";
constexpr char kF9Name[] = "f9";
constexpr char kF10Name[] = "f10";
constexpr char kF11Name[] = "f11";
constexpr char kF12Name[] = "f12";
constexpr char kF13Name[] = "f13";
constexpr char kF14Name[] = "f14";
constexpr char kF15Name[] = "f15";
constexpr char kF16Name[] = "f16";
constexpr char kF17Name[] = "f17";
constexpr char kF18Name[] = "f18";
constexpr char kF19Name[] = "f19";
constexpr char kF20Name[] = "f20";
constexpr char kF21Name[] = "f21";
constexpr char kF22Name[] = "f22";
constexpr char kF23Name[] = "f23";
constexpr char kF24Name[] = "f24";
constexpr char kF25Name[] = "f25";
constexpr char kF26Name[] = "f26";
constexpr char kF27Name[] = "f27";
constexpr char kF28Name[] = "f28";
constexpr char kF29Name[] = "f29";
constexpr char kF30Name[] = "f30";
constexpr char kF31Name[] = "f31";

CheriotDebugInfo::CheriotDebugInfo()
    : debug_register_map_({{*DebugRegisterEnum::kPc, kPcName},
                           {*DebugRegisterEnum::kC0, kC0Name},
                           {*DebugRegisterEnum::kC1, kC1Name},
                           {*DebugRegisterEnum::kC2, kC2Name},
                           {*DebugRegisterEnum::kC3, kC3Name},
                           {*DebugRegisterEnum::kC4, kC4Name},
                           {*DebugRegisterEnum::kC5, kC5Name},
                           {*DebugRegisterEnum::kC6, kC6Name},
                           {*DebugRegisterEnum::kC7, kC7Name},
                           {*DebugRegisterEnum::kC8, kC8Name},
                           {*DebugRegisterEnum::kC9, kC9Name},
                           {*DebugRegisterEnum::kC10, kC10Name},
                           {*DebugRegisterEnum::kC11, kC11Name},
                           {*DebugRegisterEnum::kC12, kC12Name},
                           {*DebugRegisterEnum::kC13, kC13Name},
                           {*DebugRegisterEnum::kC14, kC14Name},
                           {*DebugRegisterEnum::kC15, kC15Name},
                           {*DebugRegisterEnum::kC16, kC16Name},
                           {*DebugRegisterEnum::kC17, kC17Name},
                           {*DebugRegisterEnum::kC18, kC18Name},
                           {*DebugRegisterEnum::kC19, kC19Name},
                           {*DebugRegisterEnum::kC20, kC20Name},
                           {*DebugRegisterEnum::kC21, kC21Name},
                           {*DebugRegisterEnum::kC22, kC22Name},
                           {*DebugRegisterEnum::kC23, kC23Name},
                           {*DebugRegisterEnum::kC24, kC24Name},
                           {*DebugRegisterEnum::kC25, kC25Name},
                           {*DebugRegisterEnum::kC26, kC26Name},
                           {*DebugRegisterEnum::kC27, kC27Name},
                           {*DebugRegisterEnum::kC28, kC28Name},
                           {*DebugRegisterEnum::kC29, kC29Name},
                           {*DebugRegisterEnum::kC30, kC30Name},
                           {*DebugRegisterEnum::kC31, kC31Name},
                           {*DebugRegisterEnum::kF0, kF0Name},
                           {*DebugRegisterEnum::kF1, kF1Name},
                           {*DebugRegisterEnum::kF2, kF2Name},
                           {*DebugRegisterEnum::kF3, kF3Name},
                           {*DebugRegisterEnum::kF4, kF4Name},
                           {*DebugRegisterEnum::kF5, kF5Name},
                           {*DebugRegisterEnum::kF6, kF6Name},
                           {*DebugRegisterEnum::kF7, kF7Name},
                           {*DebugRegisterEnum::kF8, kF8Name},
                           {*DebugRegisterEnum::kF9, kF9Name},
                           {*DebugRegisterEnum::kF10, kF10Name},
                           {*DebugRegisterEnum::kF11, kF11Name},
                           {*DebugRegisterEnum::kF12, kF12Name},
                           {*DebugRegisterEnum::kF13, kF13Name},
                           {*DebugRegisterEnum::kF14, kF14Name},
                           {*DebugRegisterEnum::kF15, kF15Name},
                           {*DebugRegisterEnum::kF16, kF16Name},
                           {*DebugRegisterEnum::kF17, kF17Name},
                           {*DebugRegisterEnum::kF18, kF18Name},
                           {*DebugRegisterEnum::kF19, kF19Name},
                           {*DebugRegisterEnum::kF20, kF20Name},
                           {*DebugRegisterEnum::kF21, kF21Name},
                           {*DebugRegisterEnum::kF22, kF22Name},
                           {*DebugRegisterEnum::kF23, kF23Name},
                           {*DebugRegisterEnum::kF24, kF24Name},
                           {*DebugRegisterEnum::kF25, kF25Name},
                           {*DebugRegisterEnum::kF26, kF26Name},
                           {*DebugRegisterEnum::kF27, kF27Name},
                           {*DebugRegisterEnum::kF28, kF28Name},
                           {*DebugRegisterEnum::kF29, kF29Name},
                           {*DebugRegisterEnum::kF30, kF30Name},
                           {*DebugRegisterEnum::kF31, kF31Name}}) {}

CheriotDebugInfo* CheriotDebugInfo::Instance() {
  static CheriotDebugInfo* instance_ = nullptr;
  if (instance_ == nullptr) {
    instance_ = new CheriotDebugInfo();
  }
  return instance_;
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
