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

#ifndef MPACT_CHERIOT__CHERIOT_INSTRUMENTATION_CONTROL_H_
#define MPACT_CHERIOT__CHERIOT_INSTRUMENTATION_CONTROL_H_

#include <string>

#include "absl/strings/string_view.h"
#include "cheriot/cheriot_top.h"
#include "cheriot/debug_command_shell.h"
#include "mpact/sim/util/memory/memory_use_profiler.h"
#include "re2/re2.h"

namespace mpact::sim::cheriot {

using ::mpact::sim::util::TaggedMemoryUseProfiler;

class CheriotInstrumentationControl {
 public:
  CheriotInstrumentationControl(DebugCommandShell *shell,
                                CheriotTop *cheriot_top,
                                TaggedMemoryUseProfiler *mem_profiler);

  bool PerformShellCommand(absl::string_view input,
                           const DebugCommandShell::CoreAccess &core_access,
                           std::string &output);

  std::string Usage() const;

 private:
  DebugCommandShell *shell_;
  CheriotTop *top_ = nullptr;
  TaggedMemoryUseProfiler *mem_profiler_;
  LazyRE2 pattern_re_;
};

}  // namespace mpact::sim::cheriot

#endif  // MPACT_CHERIOT__CHERIOT_INSTRUMENTATION_CONTROL_H_
