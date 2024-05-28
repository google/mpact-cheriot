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

#ifndef MPACT_CHERIOT__CHERIOT_TEST_RIG_H_
#define MPACT_CHERIOT__CHERIOT_TEST_RIG_H_

#include <cstdint>

#include "absl/status/status.h"
#include "cheriot/cheriot_register.h"
#include "cheriot/cheriot_state.h"
#include "cheriot/cheriot_test_rig_decoder.h"
#include "cheriot/riscv_cheriot_fp_state.h"
#include "cheriot/test_rig_packets.h"
#include "mpact/sim/generic/component.h"
#include "mpact/sim/generic/counters.h"
#include "mpact/sim/generic/data_buffer.h"
#include "mpact/sim/util/memory/tagged_memory_interface.h"
#include "mpact/sim/util/memory/tagged_memory_watcher.h"

// This file defines the class used to execute instructions provided by TestRig.
// TestRig is a framework for testing RiscV processors with Random Instruction
// Generation. See. http://github.com/CTSRD-CHERI/TestRIG.

namespace mpact::sim::cheriot {

class CheriotTestRig : public generic::Component {
 public:
  CheriotTestRig();
  ~CheriotTestRig() override;
  // Execute the instruction word specified in the instruction packet. Fill out
  // the fields in the execution packet accordingly.
  absl::Status Execute(const test_rig::InstructionPacket &inst_packet, int fd);
  // Return the highest version of RVFI supported.
  int GetMaxSupportedVersion() { return 2; }
  // Set the version of RVFI to use.
  absl::Status SetVersion(int version);

  // Reset the execution state and write out end packet.
  absl::Status Reset(uint8_t halt, int fd);

 private:
  // Version specific instances of execute and reset.
  absl::Status ExecuteV1(const test_rig::InstructionPacket &inst_packet,
                         int fd);
  absl::Status ExecuteV2(const test_rig::InstructionPacket &inst_packet,
                         int fd);
  absl::Status ResetV1(uint8_t halt, int fd);
  absl::Status ResetV2(uint8_t halt, int fd);
  // Perform necessary architectural reset.
  void ResetArch();
  // Callback when a trap is encountered.
  bool OnTrap(bool is_interrupt, uint64_t trap_value, uint64_t exception_code,
              uint64_t epc, const Instruction *inst);
  // Callback when a memory load/store encountered.
  void OnLoad(uint64_t address, int size);
  void OnStore(uint64_t address, int size);
  // Return the value of the register with the given id.
  uint32_t GetRegister(uint32_t reg_id);

  int trace_version_ = 1;
  CheriotState *state_;
  RiscVCheriotFPState *fp_state_;
  CheriotRegister *pcc_;
  CheriotTestRigDecoder *cheriot_decoder_ = nullptr;
  util::TaggedMemoryInterface *tagged_memory_ = nullptr;
  util::TaggedMemoryWatcher *tagged_memory_watcher_ = nullptr;
  // Instruction counter.
  generic::SimpleCounter<uint64_t> counter_num_instructions_;
  // Fields for capturing information during execution of an instruction that
  // then can be filled into the execution packet.
  bool trap_set_ = false;
  uint64_t mem_addr_;
  uint64_t mem_r_mask_;
  uint64_t mem_w_mask_;
  uint64_t mem_r_data_[4];
  uint64_t mem_w_data_[4];
  // Handling data buffers for ld/st.
  generic::DataBufferFactory db_factory_;
  generic::DataBuffer *db1_ = nullptr;
  generic::DataBuffer *db2_ = nullptr;
  generic::DataBuffer *db4_ = nullptr;
  generic::DataBuffer *db8_ = nullptr;
};

}  // namespace mpact::sim::cheriot

#endif  // MPACT_CHERIOT__CHERIOT_TEST_RIG_H_
