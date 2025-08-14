// Copyright 2024 Google LLC
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

#include "cheriot/riscv_cheriot_vector_fp_compare_instructions.h"

#include "absl/log/log.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/cheriot_vector_state.h"
#include "cheriot/riscv_cheriot_vector_instruction_helpers.h"

namespace mpact {
namespace sim {
namespace cheriot {

// Vector floating point compare equal.
void Vmfeq(const Instruction* inst) {
  auto* rv_vector = static_cast<CheriotState*>(inst->state())->rv_vector();
  int sew = rv_vector->selected_element_width();
  switch (sew) {
    case 4:
      return RiscVBinaryVectorMaskOp<float, float>(
          rv_vector, inst,
          [](float vs2, float vs1) -> bool { return vs2 == vs1; });
    case 8:
      return RiscVBinaryVectorMaskOp<double, double>(
          rv_vector, inst,
          [](double vs2, double vs1) -> bool { return vs2 == vs1; });
    default:
      LOG(ERROR) << "Vmfeq: Illegal sew (" << sew << ")";
      rv_vector->set_vector_exception();
      return;
  }
}

// Vector floating point compare less than or equal.
void Vmfle(const Instruction* inst) {
  auto* rv_vector = static_cast<CheriotState*>(inst->state())->rv_vector();
  int sew = rv_vector->selected_element_width();
  switch (sew) {
    case 4:
      return RiscVBinaryVectorMaskOp<float, float>(
          rv_vector, inst,
          [](float vs2, float vs1) -> bool { return vs2 <= vs1; });
    case 8:
      return RiscVBinaryVectorMaskOp<double, double>(
          rv_vector, inst,
          [](double vs2, double vs1) -> bool { return vs2 <= vs1; });
    default:
      LOG(ERROR) << "Vmfle: Illegal sew (" << sew << ")";
      rv_vector->set_vector_exception();
      return;
  }
}

// Vector floating compare less than.
void Vmflt(const Instruction* inst) {
  auto* rv_vector = static_cast<CheriotState*>(inst->state())->rv_vector();
  int sew = rv_vector->selected_element_width();
  switch (sew) {
    case 4:
      return RiscVBinaryVectorMaskOp<float, float>(
          rv_vector, inst,
          [](float vs2, float vs1) -> bool { return vs2 < vs1; });
    case 8:
      return RiscVBinaryVectorMaskOp<double, double>(
          rv_vector, inst,
          [](double vs2, double vs1) -> bool { return vs2 < vs1; });
    default:
      LOG(ERROR) << "Vmflt: Illegal sew (" << sew << ")";
      rv_vector->set_vector_exception();
      return;
  }
}

// Vector floating point compare not equal.
void Vmfne(const Instruction* inst) {
  auto* rv_vector = static_cast<CheriotState*>(inst->state())->rv_vector();
  int sew = rv_vector->selected_element_width();
  switch (sew) {
    case 4:
      return RiscVBinaryVectorMaskOp<float, float>(
          rv_vector, inst,
          [](float vs2, float vs1) -> bool { return vs2 != vs1; });
    case 8:
      return RiscVBinaryVectorMaskOp<double, double>(
          rv_vector, inst,
          [](double vs2, double vs1) -> bool { return vs2 != vs1; });
    default:
      LOG(ERROR) << "Vmfne: Illegal sew (" << sew << ")";
      rv_vector->set_vector_exception();
      return;
  }
}

// Vector floating point compare greater than.
void Vmfgt(const Instruction* inst) {
  auto* rv_vector = static_cast<CheriotState*>(inst->state())->rv_vector();
  int sew = rv_vector->selected_element_width();
  switch (sew) {
    case 4:
      return RiscVBinaryVectorMaskOp<float, float>(
          rv_vector, inst,
          [](float vs2, float vs1) -> bool { return vs2 > vs1; });
    case 8:
      return RiscVBinaryVectorMaskOp<double, double>(
          rv_vector, inst,
          [](double vs2, double vs1) -> bool { return vs2 > vs1; });
    default:
      LOG(ERROR) << "Vmfgt: Illegal sew (" << sew << ")";
      rv_vector->set_vector_exception();
      return;
  }
}

// Vector floating point compare greater than or equal.
void Vmfge(const Instruction* inst) {
  auto* rv_vector = static_cast<CheriotState*>(inst->state())->rv_vector();
  int sew = rv_vector->selected_element_width();
  switch (sew) {
    case 4:
      return RiscVBinaryVectorMaskOp<float, float>(
          rv_vector, inst,
          [](float vs2, float vs1) -> bool { return vs2 >= vs1; });
    case 8:
      return RiscVBinaryVectorMaskOp<double, double>(
          rv_vector, inst,
          [](double vs2, double vs1) -> bool { return vs2 >= vs1; });
    default:
      LOG(ERROR) << "Vmfge: Illegal sew (" << sew << ")";
      rv_vector->set_vector_exception();
      return;
  }
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
