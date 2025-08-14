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

#include "cheriot/cheriot_state.h"

#include <cstdint>
#include <iostream>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "cheriot/cheriot_register.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/generic/instruction.h"
#include "mpact/sim/util/memory/tagged_flat_demand_memory.h"
#include "riscv//riscv_state.h"

namespace {

using ::mpact::sim::cheriot::CheriotRegister;
using ::mpact::sim::cheriot::CheriotState;
using ::mpact::sim::util::TaggedFlatDemandMemory;
using RVEC = ::mpact::sim::riscv::ExceptionCode;

constexpr int kPcValue = 0x1000;
constexpr int kMemAddr = 0x1200;
constexpr uint32_t kMemValue = 0xdeadbeef;

// Only limited testing of the CheriotState class for now as it has limited
// additional functionality over the ArchState class.

TEST(CheriotStateTest, Basic) {
  TaggedFlatDemandMemory mem(8);

  auto* state = new CheriotState("test", &mem, nullptr);
  // Make sure pc has been created.
  auto iter = state->registers()->find("pcc");
  auto* ptr = (iter != state->registers()->end()) ? iter->second : nullptr;
  CHECK_NE(ptr, nullptr);
  auto* pcc = static_cast<CheriotRegister*>(ptr);
  // Make pcc an executable root.
  pcc->CopyFrom(*state->executable_root());
  // Set pc to 0x1000, then read value back through pc operand.
  pcc->data_buffer()->Set<uint32_t>(0, kPcValue);
  auto* pc_op = state->pc_operand();
  EXPECT_EQ(pc_op->AsUint32(0), kPcValue);
  delete state;
}

TEST(CheriotStateTest, Memory) {
  TaggedFlatDemandMemory mem(8);

  auto* state = new CheriotState("test", &mem, nullptr);
  auto* db = state->db_factory()->Allocate<uint32_t>(1);
  state->LoadMemory(nullptr, kMemAddr, db, nullptr, nullptr);
  EXPECT_EQ(db->Get<uint32_t>(0), 0);
  db->Set<uint32_t>(0, kMemValue);
  state->StoreMemory(nullptr, kMemAddr, db);
  db->Set<uint32_t>(0, 0);
  state->LoadMemory(nullptr, kMemAddr, db, nullptr, nullptr);
  EXPECT_EQ(db->Get<uint32_t>(0), kMemValue);
  db->DecRef();
  delete state;
}

TEST(CheriotStateTest, OutOfBoundLoad) {
  TaggedFlatDemandMemory mem(8);

  auto* state = new CheriotState("test", &mem, nullptr);
  state->set_max_physical_address(kMemAddr - 4);
  state->set_on_trap([](bool is_interrupt, uint64_t trap_value,
                        uint64_t exception_code, uint64_t epc,
                        const mpact::sim::riscv::Instruction* inst) -> bool {
    if (exception_code ==
        static_cast<uint64_t>(
            mpact::sim::riscv::ExceptionCode::kLoadAccessFault)) {
      std::cerr << "Load Access Fault" << std::endl;
      return true;
    }
    return false;
  });
  auto* db = state->db_factory()->Allocate<uint32_t>(1);
  // Create a dummy instruction so trap can dereference the address.
  auto* dummy_inst = new mpact::sim::generic::Instruction(0x0, nullptr);
  dummy_inst->set_size(4);
  testing::internal::CaptureStderr();
  state->LoadMemory(dummy_inst, kMemAddr, db, nullptr, nullptr);
  const std::string stderr = testing::internal::GetCapturedStderr();
  EXPECT_THAT(stderr, testing::HasSubstr("Load Access Fault"));
  db->DecRef();
  dummy_inst->DecRef();
  delete state;
}

// Verify that the mshwm register decrements by 16.
TEST(CheriotStateTest, Mshwm) {
  auto* mem = new mpact::sim::util::TaggedFlatDemandMemory(8);
  auto* state = new CheriotState("test", mem);
  auto* byte_db = state->db_factory()->Allocate<uint8_t>(1);
  auto mshwmb_res = state->csr_set()->GetCsr("mshwmb");
  CHECK_OK(mshwmb_res);
  auto* mshwmb = mshwmb_res.value();
  auto mshwm_res = state->csr_set()->GetCsr("mshwm");
  CHECK_OK(mshwm_res);
  auto* mshwm = mshwm_res.value();
  mshwmb->Write(0x0U);
  mshwm->Write(0x80000000);
  state->StoreMemory(nullptr, 0x7fff'ffff, byte_db);
  EXPECT_EQ(mshwm->AsUint32(), 0x7fff'fff0) << absl::StrFormat(
      "mshwm == 0x%08x != 0x%08x", mshwm->AsUint32(), 0x7fff'fff0);
  EXPECT_EQ(mshwmb->AsUint32(), 0x0);
  byte_db->DecRef();
  delete state;
  delete mem;
}

}  // namespace
