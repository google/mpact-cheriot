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

#include "cheriot/cheriot_renode_register_info.h"

#include "cheriot/cheriot_debug_info.h"
#include "mpact/sim/generic/type_helpers.h"

namespace mpact {
namespace sim {
namespace cheriot {

using ::mpact::sim::generic::operator*;  // NOLINT - used below.

CheriotRenodeRegisterInfo *CheriotRenodeRegisterInfo::instance_ = nullptr;

void CheriotRenodeRegisterInfo::InitializeRenodeRegisterInfo() {
  using DbgReg = DebugRegisterEnum;

  renode_register_info_ = {
      {*DbgReg::kPc, 32, true, false},   {*DbgReg::kC0, 32, true, true},
      {*DbgReg::kC1, 32, true, false},   {*DbgReg::kC2, 32, true, false},
      {*DbgReg::kC3, 32, true, false},   {*DbgReg::kC4, 32, true, false},
      {*DbgReg::kC5, 32, true, false},   {*DbgReg::kC6, 32, true, false},
      {*DbgReg::kC7, 32, true, false},   {*DbgReg::kC8, 32, true, false},
      {*DbgReg::kC9, 32, true, false},   {*DbgReg::kC10, 32, true, false},
      {*DbgReg::kC11, 32, true, false},  {*DbgReg::kC12, 32, true, false},
      {*DbgReg::kC13, 32, true, false},  {*DbgReg::kC14, 32, true, false},
      {*DbgReg::kC15, 32, true, false},  {*DbgReg::kC16, 32, true, false},
      {*DbgReg::kC17, 32, true, false},  {*DbgReg::kC18, 32, true, false},
      {*DbgReg::kC19, 32, true, false},  {*DbgReg::kC20, 32, true, false},
      {*DbgReg::kC21, 32, true, false},  {*DbgReg::kC22, 32, true, false},
      {*DbgReg::kC23, 32, true, false},  {*DbgReg::kC24, 32, true, false},
      {*DbgReg::kC25, 32, true, false},  {*DbgReg::kC26, 32, true, false},
      {*DbgReg::kC27, 32, true, false},  {*DbgReg::kC28, 32, true, false},
      {*DbgReg::kC29, 32, true, false},  {*DbgReg::kC30, 32, true, false},
      {*DbgReg::kC31, 32, true, false},  {*DbgReg::kF0, 32, false, false},
      {*DbgReg::kF1, 32, false, false},  {*DbgReg::kF2, 32, false, false},
      {*DbgReg::kF3, 32, false, false},  {*DbgReg::kF4, 32, false, false},
      {*DbgReg::kF5, 32, false, false},  {*DbgReg::kF6, 32, false, false},
      {*DbgReg::kF7, 32, false, false},  {*DbgReg::kF8, 32, false, false},
      {*DbgReg::kF9, 32, false, false},  {*DbgReg::kF10, 32, false, false},
      {*DbgReg::kF11, 32, false, false}, {*DbgReg::kF12, 32, false, false},
      {*DbgReg::kF13, 32, false, false}, {*DbgReg::kF14, 32, false, false},
      {*DbgReg::kF15, 32, false, false}, {*DbgReg::kF16, 32, false, false},
      {*DbgReg::kF17, 32, false, false}, {*DbgReg::kF18, 32, false, false},
      {*DbgReg::kF19, 32, false, false}, {*DbgReg::kF20, 32, false, false},
      {*DbgReg::kF21, 32, false, false}, {*DbgReg::kF22, 32, false, false},
      {*DbgReg::kF23, 32, false, false}, {*DbgReg::kF24, 32, false, false},
      {*DbgReg::kF25, 32, false, false}, {*DbgReg::kF26, 32, false, false},
      {*DbgReg::kF27, 32, false, false}, {*DbgReg::kF28, 32, false, false},
      {*DbgReg::kF29, 32, false, false}, {*DbgReg::kF30, 32, false, false},
      {*DbgReg::kF31, 32, false, false},
  };
}

CheriotRenodeRegisterInfo::CheriotRenodeRegisterInfo() {
  InitializeRenodeRegisterInfo();
}

const CheriotRenodeRegisterInfo::RenodeRegisterInfo &
CheriotRenodeRegisterInfo::GetRenodeRegisterInfo() {
  return Instance()->GetRenodeRegisterInfoPrivate();
}

CheriotRenodeRegisterInfo *CheriotRenodeRegisterInfo::Instance() {
  if (instance_ == nullptr) {
    instance_ = new CheriotRenodeRegisterInfo();
  }
  return instance_;
}

const CheriotRenodeRegisterInfo::RenodeRegisterInfo &
CheriotRenodeRegisterInfo::GetRenodeRegisterInfoPrivate() {
  return renode_register_info_;
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
