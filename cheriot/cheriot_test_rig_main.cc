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

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>
#include <cstdint>
#include <memory>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "cheriot/cheriot_test_rig.h"
#include "cheriot/test_rig_packets.h"

using ::mpact::sim::cheriot::CheriotTestRig;
using ::mpact::sim::cheriot::test_rig::InstructionPacket;
using ::mpact::sim::cheriot::test_rig::TraceCommand;
using ::mpact::sim::cheriot::test_rig::VersionPacket;

ABSL_FLAG(int, trace_port, 0, "Trace port number");

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage(argv[0]);
  auto arg_vec = absl::ParseCommandLine(argc, argv);

  // Verify that the port number has been set.
  if (absl::GetFlag(FLAGS_trace_port) == 0) {
    LOG(ERROR) << "No trace target port specified\n";
    return -1;
  }

  // Connect to the sockets.
  auto trace_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (trace_socket == -1) {
    LOG(ERROR) << "Error creating socket\n";
    return -1;
  }
  // Set socket option SO_REUSEADDR and SO_REUSEPORT.
  int reuseaddr = 1;
  int reuseport = 1;
  if (setsockopt(trace_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                 sizeof(reuseaddr)) < 0) {
    LOG(ERROR) << "Failed to set socket option SO_REUSEADDR\n";
    return -1;
  }
  if (setsockopt(trace_socket, SOL_SOCKET, SO_REUSEPORT, &reuseport,
                 sizeof(reuseport)) < 0) {
    LOG(ERROR) << "Failed to set socket option SO_REUSEPORT\n";
    return -1;
  }
  if (struct timeval t = {0, 0};
      setsockopt(trace_socket, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) < 0) {
    LOG(ERROR) << "Failed to set socket option SO_RCVTIMEO\n";
    return -1;
  }
  // Bind the socket.
  const sockaddr_in trace_address_in = {
      AF_INET, htons(absl::GetFlag(FLAGS_trace_port)), {INADDR_ANY}};
  int res =
      bind(trace_socket, reinterpret_cast<const sockaddr*>(&trace_address_in),
           sizeof(trace_address_in));
  if (res != 0) {
    LOG(ERROR) << "Error connecting to trace_socket (" << res << ")\n";
    return -1;
  }
  // Accept connection.
  res = listen(trace_socket, 1);
  int trace_fd = accept(trace_socket, nullptr, nullptr);

  // Test rig engine.
  auto test_rig = std::make_unique<CheriotTestRig>();

  CHECK_OK(test_rig->SetVersion(1));
  InstructionPacket inst_packet;
  VersionPacket version_packet;

  bool error = false;
  uint32_t trace_version = 0;
  while (true) {
    auto res = read(trace_fd, &inst_packet, sizeof(inst_packet));
    // Check for error.
    if (res < 0) {
      LOG(ERROR) << "Error reading from trace socket (" << errno << ")\n";
      error = true;
      break;
    }
    // Zero bytes indicates end of file.
    if (res == 0) break;
    if (res != sizeof(inst_packet)) {
      LOG(ERROR) << "Error reading insufficient bytes from trace socket ("
                 << res << " != " << sizeof(inst_packet) << ")\n";
      error = true;
      break;
    }
    switch (inst_packet.rvfi_cmd) {
      case TraceCommand::kEndOfTrace: {
        // First check if this is a version negotiation packet.
        uint8_t halt;
        if (inst_packet.rvfi_insn == 0x56455253) {
          auto version = test_rig->GetMaxSupportedVersion();
          halt = 1 | version;
        } else {  // End of trace packet.
          halt = 1;
        }
        auto status = test_rig->Reset(halt, trace_fd);
        if (!status.ok()) {
          LOG(ERROR) << "Error: " << status.message() << "\n";
          error = true;
        }
        break;
      }
      case TraceCommand::kInstruction: {
        // Execute the trace packet.
        auto status = test_rig->Execute(inst_packet, trace_fd);
        if (!status.ok()) {
          LOG(ERROR) << "Error executing trace packet (" << status.message()
                     << ")\n";
          error = true;
        }
        break;
      }
      case TraceCommand::kSetVersion: {
        // Set the trace version to write.
        trace_version = inst_packet.rvfi_insn;
        auto status = test_rig->SetVersion(trace_version);
        if (!status.ok()) {
          LOG(ERROR) << "Error setting trace version (" << status.message()
                     << ")\n";
          error = true;
          break;
        }
        version_packet.version = trace_version;
        res = write(trace_fd, &version_packet, sizeof(version_packet));
        if (res != sizeof(version_packet)) {
          LOG(ERROR) << "Error writing to trace socket (" << res << ")\n";
          error = true;
        }
        break;
      }
      default:
        LOG(ERROR) << "Unknown command (ignored): " << (int)inst_packet.rvfi_cmd
                   << "\n";
        break;
    }
    if (error) break;
  }

  // Shutdown the socket.
  res = shutdown(trace_fd, SHUT_RDWR);
  res = shutdown(trace_socket, SHUT_RDWR);
}
