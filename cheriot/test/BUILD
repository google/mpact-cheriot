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

config_setting(
    name = "darwin_aarch64_cpu",
    values = {"cpu": "darwin_arm64"},
)

cc_test(
    name = "cheriot_register_test",
    size = "small",
    srcs = ["cheriot_register_test.cc"],
    deps = [
        "//cheriot:cheriot_state",
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
        "//cheriot:cheriot_state",
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
        "//cheriot:cheriot_state",
        "//cheriot:riscv_cheriot_instructions",
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
        "//cheriot:cheriot_state",
        "//cheriot:riscv_cheriot_instructions",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_test(
    name = "riscv_cheriot_m_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_m_instructions_test.cc",
    ],
    deps = [
        "//cheriot:cheriot_state",
        "//cheriot:riscv_cheriot_instructions",
        "@com_google_absl//absl/random",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
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
        "//cheriot:cheriot_state",
        "//cheriot:riscv_cheriot_instructions",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_test(
    name = "riscv_cheriot_encoding_test",
    size = "small",
    srcs = [
        "riscv_cheriot_encoding_test.cc",
    ],
    deps = [
        "//cheriot:cheriot_state",
        "//cheriot:riscv_cheriot_decoder",
        "//cheriot:riscv_cheriot_isa",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_test(
    name = "riscv_cheriot_rvv_encoding_test",
    size = "small",
    srcs = [
        "riscv_cheriot_rvv_encoding_test.cc",
    ],
    deps = [
        "//cheriot:cheriot_state",
        "//cheriot:riscv_cheriot_rvv_decoder",
        "//cheriot:riscv_cheriot_rvv_isa",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_test(
    name = "riscv_cheriot_a_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_a_instructions_test.cc",
    ],
    deps = [
        "//cheriot:cheriot_state",
        "//cheriot:riscv_cheriot_instructions",
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
        "//cheriot:cheriot_state",
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
    name = "librenode_mpact_cheriot.so_test",
    size = "small",
    srcs = ["librenode_mpact_cheriot_so_test.cc"],
    data = [
        "//cheriot:renode_mpact_cheriot",
    ],
    tags = ["notap"],
    deps = [
        "@com_google_absl//absl/debugging:leak_check",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "cheriot_rvv_fp_decoder_test",
    size = "small",
    srcs = ["cheriot_rvv_fp_decoder_test.cc"],
    deps = [
        "//cheriot:riscv_cheriot_rvv_fp_decoder",
        "@com_google_absl//absl/log:log_sink_registry",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/util/other:log_sink",
    ],
)

cc_test(
    name = "cheriot_rvv_decoder_test",
    size = "small",
    srcs = ["cheriot_rvv_decoder_test.cc"],
    deps = [
        "//cheriot:riscv_cheriot_rvv_decoder",
        "@com_google_absl//absl/log:log_sink_registry",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/util/other:log_sink",
    ],
)

cc_library(
    name = "riscv_cheriot_vector_instructions_test_base",
    testonly = True,
    hdrs = ["riscv_cheriot_vector_instructions_test_base.h"],
    deps = [
        "//cheriot:cheriot_state",
        "//cheriot:cheriot_vector_state",
        "//cheriot:riscv_cheriot_vector",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_library(
    name = "riscv_cheriot_vector_fp_test_utilities",
    testonly = True,
    hdrs = ["riscv_cheriot_vector_fp_test_utilities.h"],
    deps = [
        ":riscv_cheriot_vector_instructions_test_base",
        "//cheriot:cheriot_state",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_mpact-riscv//riscv:riscv_fp_state",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_test(
    name = "riscv_cheriot_vector_true_test",
    size = "small",
    srcs = [
        "riscv_cheriot_vector_true_test.cc",
    ],
    deps = [
        "//cheriot:cheriot_state",
        "//cheriot:cheriot_vector_state",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_test(
    name = "riscv_cheriot_vector_memory_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_vector_memory_instructions_test.cc",
    ],
    deps = [
        "//cheriot:cheriot_state",
        "//cheriot:cheriot_vector_state",
        "//cheriot:riscv_cheriot_vector",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_test(
    name = "riscv_cheriot_vector_opi_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_vector_opi_instructions_test.cc",
    ],
    deps = [
        ":riscv_cheriot_vector_instructions_test_base",
        "//cheriot:cheriot_state",
        "//cheriot:cheriot_vector_state",
        "//cheriot:riscv_cheriot_vector",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
    ],
)

cc_test(
    name = "riscv_cheriot_vector_opm_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_vector_opm_instructions_test.cc",
    ],
    deps = [
        ":riscv_cheriot_vector_instructions_test_base",
        "//cheriot:cheriot_state",
        "//cheriot:cheriot_vector_state",
        "//cheriot:riscv_cheriot_vector",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
    ],
)

cc_test(
    name = "riscv_cheriot_vector_reduction_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_vector_reduction_instructions_test.cc",
    ],
    deps = [
        ":riscv_cheriot_vector_instructions_test_base",
        "//cheriot:cheriot_state",
        "//cheriot:cheriot_vector_state",
        "//cheriot:riscv_cheriot_vector",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_test(
    name = "riscv_cheriot_vector_unary_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_vector_unary_instructions_test.cc",
    ],
    deps = [
        ":riscv_cheriot_vector_instructions_test_base",
        "//cheriot:cheriot_state",
        "//cheriot:riscv_cheriot_vector",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_test(
    name = "riscv_cheriot_vector_fp_unary_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_vector_fp_unary_instructions_test.cc",
    ],
    copts = select({
        "darwin_aarch64_cpu": ["-ffp-model=strict"],
        "//conditions:default": [
            "-ffp-model=strict",
            "-fprotect-parens",
        ],
    }),
    deps = [
        ":riscv_cheriot_vector_fp_test_utilities",
        ":riscv_cheriot_vector_instructions_test_base",
        "//cheriot:cheriot_state",
        "//cheriot:cheriot_vector_state",
        "//cheriot:riscv_cheriot_vector_fp",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_fp_state",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_test(
    name = "riscv_cheriot_vector_fp_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_vector_fp_instructions_test.cc",
    ],
    copts = select({
        "darwin_aarch64_cpu": ["-ffp-model=strict"],
        "//conditions:default": [
            "-ffp-model=strict",
            "-fprotect-parens",
        ],
    }),
    deps = [
        ":riscv_cheriot_vector_fp_test_utilities",
        ":riscv_cheriot_vector_instructions_test_base",
        "//cheriot:cheriot_state",
        "//cheriot:cheriot_vector_state",
        "//cheriot:riscv_cheriot_vector_fp",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_fp_state",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_test(
    name = "riscv_cheriot_vector_fp_compare_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_vector_fp_compare_instructions_test.cc",
    ],
    copts = select({
        "darwin_aarch64_cpu": ["-ffp-model=strict"],
        "//conditions:default": [
            "-ffp-model=strict",
            "-fprotect-parens",
        ],
    }),
    deps = [
        ":riscv_cheriot_vector_fp_test_utilities",
        ":riscv_cheriot_vector_instructions_test_base",
        "//cheriot:cheriot_state",
        "//cheriot:riscv_cheriot_vector_fp",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_fp_state",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
    ],
)

cc_test(
    name = "riscv_cheriot_vector_fp_reduction_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_vector_fp_reduction_instructions_test.cc",
    ],
    copts = select({
        "darwin_aarch64_cpu": ["-ffp-model=strict"],
        "//conditions:default": [
            "-ffp-model=strict",
            "-fprotect-parens",
        ],
    }),
    deps = [
        ":riscv_cheriot_vector_fp_test_utilities",
        ":riscv_cheriot_vector_instructions_test_base",
        "//cheriot:cheriot_state",
        "//cheriot:riscv_cheriot_vector_fp",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_fp_state",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
    ],
)

cc_test(
    name = "riscv_cheriot_vector_permute_instructions_test",
    size = "small",
    srcs = [
        "riscv_cheriot_vector_permute_instructions_test.cc",
    ],
    deps = [
        ":riscv_cheriot_vector_instructions_test_base",
        "//cheriot:cheriot_state",
        "//cheriot:riscv_cheriot_vector",
        "@com_google_absl//absl/random",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
    ],
)

cc_test(
    name = "cheriot_ibex_hw_revoker_test",
    size = "small",
    srcs = [
        "cheriot_ibex_hw_revoker_test.cc",
    ],
    deps = [
        "//cheriot:cheriot_ibex_hw_revoker",
        "//cheriot:cheriot_state",
        "@com_google_googletest//:gtest_main",
        "@com_google_mpact-riscv//riscv:riscv_plic",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)
