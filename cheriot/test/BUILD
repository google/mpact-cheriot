# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Package for tests of CHERI RiscV simulator code.

package(default_applicable_licenses = ["//:license"])

cc_library(
    name = "riscv_cheriot_fp_test_base",
    testonly = True,
    hdrs = ["riscv_cheriot_fp_test_base.h"],
    copts = [
        "-ffp-model=strict",
        "-fprotect-parens",
    ],
    tags = ["not_run:arm"],
    deps = [
        "//cheriot:riscv_cheriot",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_for_library_testonly",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_test(
    name = "cheriot_register_test",
    size = "small",
    srcs = ["cheriot_register_test.cc"],
    deps = [
        "//cheriot:riscv_cheriot",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
    ],
)

cc_test(
    name = "cheriot_state_test",
    size = "small",
    srcs = ["cheriot_state_test.cc"],
    deps = [
        "//cheriot:riscv_cheriot",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_test(
    name = "riscv_cheriot_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_instructions_test.cc",
    ],
    deps = [
        "//cheriot:riscv_cheriot",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_test(
    name = "riscv_cheriot_i_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_i_instructions_test.cc",
    ],
    deps = [
        "//cheriot:riscv_cheriot",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_test(
    name = "riscv_cheriot_m_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_m_instructions_test.cc",
    ],
    deps = [
        "//cheriot:riscv_cheriot",
        "@com_google_absl//absl/random",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_test(
    name = "riscv_cheriot_zicsr_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_zicsr_instructions_test.cc",
    ],
    tags = ["not_run:arm"],
    deps = [
        "//cheriot:riscv_cheriot",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
    ],
)

cc_test(
    name = "riscv_cheriot_encoding_test",
    size = "small",
    srcs = [
        "riscv_cheriot_encoding_test.cc",
    ],
    deps = [
        "//cheriot:riscv_cheriot",
        "//cheriot:riscv_cheriot_decoder",
        "//cheriot:riscv_cheriot_isa",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_test(
    name = "riscv_cheriot_a_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_a_instructions_test.cc",
    ],
    deps = [
        "//cheriot:riscv_cheriot",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_test(
    name = "cheriot_load_filter_test",
    size = "small",
    srcs = [
        "cheriot_load_filter_test.cc",
    ],
    deps = [
        "//cheriot:cheriot_load_filter",
        "//cheriot:riscv_cheriot",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:counters",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_test(
    name = "cheriot_test_rig_test",
    size = "small",
    srcs = [
        "cheriot_test_rig_test.cc",
    ],
    deps = [
        "//cheriot:cheriot_test_rig_lib",
        "@com_google_absl//absl/log:check",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "memory_use_profiler_test",
    size = "small",
    srcs = ["memory_use_profiler_test.cc"],
    deps = [
        "//cheriot:instrumentation",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/generic:core",
    ],
)

cc_test(
    name = "librenode_mpact_cheriot.so_test",
    size = "small",
    srcs = ["librenode_mpact_cheriot_so_test.cc"],
    data = [
        "//cheriot:renode_mpact_cheriot",
    ],
    deps = [
        "@com_google_absl//absl/debugging:leak_check",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
