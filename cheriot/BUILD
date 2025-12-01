# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# CHERI RiscV simulator code.

load("@com_google_mpact-sim//mpact/sim/decoder:mpact_sim_isa.bzl", "mpact_bin_fmt_decoder", "mpact_isa_decoder")
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(
    default_applicable_licenses = ["//third_party/mpact_cheriot//:license"],
    default_visibility = ["//visibility:public"],
)

exports_files([
    "riscv_cheriot.bin_fmt",
    "riscv_cheriot.isa",
    "riscv_cheriot_f.bin_fmt",
    "riscv_cheriot_f.isa",
    "riscv_cheriot_rvv.isa",
    "riscv_cheriot_vector.bin_fmt",
    "riscv_cheriot_vector_fp.bin_fmt",
    "riscv_cheriot_vector.isa",
])

config_setting(
    name = "darwin_aarch64_cpu",
    values = {"cpu": "darwin_arm64"},
)

mpact_isa_decoder(
    name = "riscv_cheriot_isa",
    src = "riscv_cheriot.isa",
    includes = [],
    isa_name = "RiscVCheriot",
    deps = [
        ":riscv_cheriot_instructions",
        "@com_google_absl//absl/functional:bind_front",
    ],
)

mpact_isa_decoder(
    name = "riscv_cheriot_rvv_isa",
    src = "riscv_cheriot_rvv.isa",
    includes = [
        "riscv_cheriot.isa",
        "riscv_cheriot_f.isa",
        "riscv_cheriot_vector.isa",
        "riscv_cheriot_vector_fp.isa",
    ],
    isa_name = "RiscVCheriotRVV",
    prefix = "riscv_cheriot_rvv",
    deps = [
        ":riscv_cheriot_instructions",
        ":riscv_cheriot_vector",
        "@com_google_absl//absl/functional:bind_front",
    ],
)

mpact_isa_decoder(
    name = "riscv_cheriot_rvv_fp_isa",
    src = "riscv_cheriot_rvv.isa",
    includes = [
        "riscv_cheriot.isa",
        "riscv_cheriot_f.isa",
        "riscv_cheriot_vector.isa",
        "riscv_cheriot_vector_fp.isa",
    ],
    isa_name = "RiscVCheriotRVVFp",
    prefix = "riscv_cheriot_rvv_fp",
    deps = [
        ":riscv_cheriot_f",
        ":riscv_cheriot_instructions",
        ":riscv_cheriot_vector",
        ":riscv_cheriot_vector_fp",
        "@com_google_absl//absl/functional:bind_front",
    ],
)

mpact_bin_fmt_decoder(
    name = "riscv_cheriot_bin_fmt",
    src = "riscv_cheriot.bin_fmt",
    decoder_name = "RiscVCheriot",
    includes = [
    ],
    deps = [
        ":riscv_cheriot_isa",
    ],
)

mpact_bin_fmt_decoder(
    name = "riscv_cheriot_rvv_bin_fmt",
    src = "riscv_cheriot_rvv.bin_fmt",
    decoder_name = "RiscVCheriotRVV",
    includes = [
        "riscv_cheriot.bin_fmt",
        "riscv_cheriot_f.bin_fmt",
        "riscv_cheriot_vector.bin_fmt",
        "riscv_cheriot_vector_fp.bin_fmt",
    ],
    deps = [
        ":riscv_cheriot_rvv_isa",
    ],
)

mpact_bin_fmt_decoder(
    name = "riscv_cheriot_rvv_fp_bin_fmt",
    src = "riscv_cheriot_rvv.bin_fmt",
    decoder_name = "RiscVCheriotRVVFp",
    includes = [
        "riscv_cheriot.bin_fmt",
        "riscv_cheriot_f.bin_fmt",
        "riscv_cheriot_vector.bin_fmt",
        "riscv_cheriot_vector_fp.bin_fmt",
    ],
    prefix = "riscv_cheriot_rvv_fp",
    deps = [
        ":riscv_cheriot_rvv_fp_isa",
    ],
)

cc_library(
    name = "riscv_cheriot_instructions",
    srcs = [
        "riscv_cheriot_a_instructions.cc",
        "riscv_cheriot_i_instructions.cc",
        "riscv_cheriot_instructions.cc",
        "riscv_cheriot_m_instructions.cc",
        "riscv_cheriot_priv_instructions.cc",
        "riscv_cheriot_zicsr_instructions.cc",
    ],
    hdrs = [
        "riscv_cheriot_a_instructions.h",
        "riscv_cheriot_i_instructions.h",
        "riscv_cheriot_instructions.h",
        "riscv_cheriot_m_instructions.h",
        "riscv_cheriot_priv_instructions.h",
        "riscv_cheriot_zicsr_instructions.h",
    ],
    tags = ["not_run:arm"],
    deps = [
        ":cheriot_state",
        ":instruction_helpers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-riscv//riscv:stoull_wrapper",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_library(
    name = "cheriot_state",
    srcs = [
        "cheriot_register.cc",
        "cheriot_state.cc",
        "cheriot_vector_true_operand.cc",
    ],
    hdrs = [
        "cheriot_register.h",
        "cheriot_state.h",
        "cheriot_vector_true_operand.h",
        "riscv_cheriot_csr_enum.h",
        "riscv_cheriot_register_aliases.h",
    ],
    tags = ["not_run:arm"],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_mpact-riscv//riscv:riscv_fp_state",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:counters",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_library(
    name = "riscv_cheriot_f",
    srcs = [
        "riscv_cheriot_f_instructions.cc",
    ],
    hdrs = [
        "riscv_cheriot_f_instructions.h",
    ],
    tags = ["not_run:arm"],
    deps = [
        ":cheriot_state",
        ":instruction_helpers",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_mpact-riscv//riscv:riscv_fp_state",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_library(
    name = "cheriot_vector_state",
    srcs = [
        "cheriot_vector_state.cc",
    ],
    hdrs = [
        "cheriot_vector_state.h",
    ],
    tags = ["not_run:arm"],
    deps = [
        ":cheriot_state",
        "@com_google_absl//absl/log",
        "@com_google_mpact-riscv//riscv:riscv_state",
    ],
)

cc_library(
    name = "riscv_cheriot_vector",
    srcs = [
        "riscv_cheriot_vector_memory_instructions.cc",
        "riscv_cheriot_vector_opi_instructions.cc",
        "riscv_cheriot_vector_opm_instructions.cc",
        "riscv_cheriot_vector_permute_instructions.cc",
        "riscv_cheriot_vector_reduction_instructions.cc",
        "riscv_cheriot_vector_unary_instructions.cc",
    ],
    hdrs = [
        "riscv_cheriot_vector_memory_instructions.h",
        "riscv_cheriot_vector_opi_instructions.h",
        "riscv_cheriot_vector_opm_instructions.h",
        "riscv_cheriot_vector_permute_instructions.h",
        "riscv_cheriot_vector_reduction_instructions.h",
        "riscv_cheriot_vector_unary_instructions.h",
    ],
    copts = select({
        "darwin_aarch64_cpu": [
            "-O3",
            "-ffp-model=strict",
        ],
        "//conditions:default": [
            "-O3",
            "-ffp-model=strict",
            "-fprotect-parens",
        ],
    }),
    deps = [
        ":cheriot_state",
        ":cheriot_vector_state",
        ":instruction_helpers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_library(
    name = "riscv_cheriot_vector_fp",
    srcs = [
        "riscv_cheriot_vector_fp_compare_instructions.cc",
        "riscv_cheriot_vector_fp_instructions.cc",
        "riscv_cheriot_vector_fp_reduction_instructions.cc",
        "riscv_cheriot_vector_fp_unary_instructions.cc",
    ],
    hdrs = [
        "riscv_cheriot_vector_fp_compare_instructions.h",
        "riscv_cheriot_vector_fp_instructions.h",
        "riscv_cheriot_vector_fp_reduction_instructions.h",
        "riscv_cheriot_vector_fp_unary_instructions.h",
    ],
    copts = select({
        "darwin_aarch64_cpu": [
            "-O3",
            "-ffp-model=strict",
        ],
        "//conditions:default": [
            "-O3",
            "-ffp-model=strict",
            "-fprotect-parens",
        ],
    }),
    deps = [
        ":cheriot_state",
        ":cheriot_vector_state",
        ":instruction_helpers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_mpact-riscv//riscv:riscv_fp_state",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_library(
    name = "instruction_helpers",
    hdrs = [
        "riscv_cheriot_instruction_helpers.h",
        "riscv_cheriot_vector_instruction_helpers.h",
    ],
    deps = [
        ":cheriot_state",
        ":cheriot_vector_state",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
        "@com_google_mpact-riscv//riscv:riscv_fp_state",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_library(
    name = "cheriot_getter_helpers",
    hdrs = [
        "cheriot_getter_helpers.h",
    ],
    deps = [
        ":cheriot_state",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
    ],
)

cc_library(
    name = "cheriot_getters",
    hdrs = [
        "cheriot_f_getters.h",
        "cheriot_getters.h",
        "cheriot_rvv_fp_getters.h",
        "cheriot_rvv_getters.h",
        "riscv_cheriot_encoding_common.h",
    ],
    deps = [
        ":cheriot_getter_helpers",
        ":cheriot_state",
        ":riscv_cheriot_bin_fmt",
        ":riscv_cheriot_isa",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
    ],
)

cc_library(
    name = "riscv_cheriot_decoder",
    srcs = [
        "cheriot_decoder.cc",
        "riscv_cheriot_encoding.cc",
    ],
    hdrs = [
        "cheriot_decoder.h",
        "riscv_cheriot_encoding.h",
    ],
    deps = [
        ":cheriot_getters",
        ":cheriot_state",
        ":riscv_cheriot_bin_fmt",
        ":riscv_cheriot_isa",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:program_error",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_library(
    name = "riscv_cheriot_rvv_decoder",
    srcs = [
        "cheriot_rvv_decoder.cc",
        "riscv_cheriot_rvv_encoding.cc",
    ],
    hdrs = [
        "cheriot_rvv_decoder.h",
        "riscv_cheriot_register_aliases.h",
        "riscv_cheriot_rvv_encoding.h",
    ],
    deps = [
        ":cheriot_getters",
        ":cheriot_state",
        ":riscv_cheriot_rvv_bin_fmt",
        ":riscv_cheriot_rvv_isa",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:program_error",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_library(
    name = "riscv_cheriot_rvv_fp_decoder",
    srcs = [
        "cheriot_rvv_fp_decoder.cc",
        "riscv_cheriot_rvv_fp_encoding.cc",
    ],
    hdrs = [
        "cheriot_rvv_fp_decoder.h",
        "riscv_cheriot_register_aliases.h",
        "riscv_cheriot_rvv_fp_encoding.h",
    ],
    deps = [
        ":cheriot_getters",
        ":cheriot_state",
        ":riscv_cheriot_rvv_fp_bin_fmt",
        ":riscv_cheriot_rvv_fp_isa",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:program_error",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_library(
    name = "cheriot_top",
    srcs = [
        "cheriot_top.cc",
    ],
    hdrs = [
        "cheriot_top.h",
    ],
    copts = ["-O3"],
    deps = [
        ":cheriot_debug_interface",
        ":cheriot_state",
        ":riscv_cheriot_isa",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_mpact-riscv//riscv:riscv_action_point_memory_interface",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:action_points",
        "@com_google_mpact-sim//mpact/sim/generic:component",
        "@com_google_mpact-sim//mpact/sim/generic:config",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:core_debug_interface",
        "@com_google_mpact-sim//mpact/sim/generic:counters",
        "@com_google_mpact-sim//mpact/sim/generic:decode_cache",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
        "@com_google_mpact-sim//mpact/sim/util/memory:cache",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "cheriot_debug_interface",
    hdrs = [
        "cheriot_debug_interface.h",
    ],
    deps = [
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_mpact-sim//mpact/sim/generic:core_debug_interface",
    ],
)

cc_library(
    name = "debug_command_shell",
    srcs = [
        "debug_command_shell.cc",
    ],
    hdrs = [
        "debug_command_shell.h",
    ],
    copts = ["-O3"],
    deps = [
        ":cheriot_debug_interface",
        ":cheriot_state",
        ":cheriot_top",
        ":riscv_cheriot_decoder",
        ":riscv_cheriot_isa",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_mpact-riscv//riscv:stoull_wrapper",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:core_debug_interface",
        "@com_google_mpact-sim//mpact/sim/generic:counters",
        "@com_google_mpact-sim//mpact/sim/generic:debug_command_shell_interface",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_binary(
    name = "mpact_cheriot",
    srcs = [
        "mpact_cheriot.cc",
    ],
    copts = ["-O3"],
    deps = [
        ":cheriot_state",
        ":cheriot_top",
        ":debug_command_shell",
        ":instrumentation",
        ":riscv_cheriot_decoder",
        ":riscv_cheriot_rvv_decoder",
        ":riscv_cheriot_rvv_fp_decoder",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_mpact-riscv//riscv:riscv_arm_semihost",
        "@com_google_mpact-riscv//riscv:riscv_clint",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-riscv//riscv:stoull_wrapper",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:core_debug_interface",
        "@com_google_mpact-sim//mpact/sim/generic:counters",
        "@com_google_mpact-sim//mpact/sim/generic:debug_command_shell_interface",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/proto:component_data_cc_proto",
        "@com_google_mpact-sim//mpact/sim/util/memory",
        "@com_google_mpact-sim//mpact/sim/util/other:instruction_profiler",
        "@com_google_mpact-sim//mpact/sim/util/other:simple_uart",
        "@com_google_mpact-sim//mpact/sim/util/program_loader:elf_loader",
        "@com_google_protobuf//:protobuf",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "cheriot_load_filter",
    srcs = [
        "cheriot_load_filter.cc",
    ],
    hdrs = [
        "cheriot_load_filter.h",
    ],
    deps = [
        ":cheriot_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:counters",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_library(
    name = "cheriot_debug_info",
    srcs = [
        "cheriot_debug_info.cc",
    ],
    hdrs = [
        "cheriot_debug_info.h",
    ],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
    ],
)

cc_library(
    name = "cheriot_ibex_hw_revoker",
    srcs = [
        "cheriot_ibex_hw_revoker.cc",
    ],
    hdrs = [
        "cheriot_ibex_hw_revoker.h",
    ],
    deps = [
        ":cheriot_state",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_mpact-riscv//riscv:riscv_plic",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:counters",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_library(
    name = "instrumentation",
    srcs = [
        "cheriot_instrumentation_control.cc",
    ],
    hdrs = [
        "cheriot_instrumentation_control.h",
    ],
    deps = [
        ":cheriot_top",
        ":debug_command_shell",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_mpact-riscv//riscv:stoull_wrapper",
        "@com_google_mpact-sim//mpact/sim/util/memory",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_binary(
    name = "renode_mpact_cheriot",
    srcs = [
        "cheriot_cli_forwarder.cc",
        "cheriot_cli_forwarder.h",
        "cheriot_renode.cc",
        "cheriot_renode.h",
        "cheriot_renode_cli_top.cc",
        "cheriot_renode_cli_top.h",
        "cheriot_renode_register_info.cc",
        "cheriot_renode_register_info.h",
    ],
    # List the symbols for the functions called by renode as undefined.
    linkopts = [
        "-u construct",
        "-u construct_with_sysbus",
        "-u connect",
        "-u connect_with_sysbus",
        "-u destruct",
        "-u get_reg_info_size",
        "-u get_reg_info",
        "-u load_elf",
        "-u read_register",
        "-u write_register",
        "-u read_memory",
        "-u write_memory",
        "-u reset",
        "-u step",
        "-u set_config",
        "-u set_irq_value",
    ],
    linkshared = True,
    linkstatic = True,
    deps = [
        ":cheriot_debug_info",
        ":cheriot_debug_interface",
        ":cheriot_state",
        ":cheriot_top",
        ":debug_command_shell",
        ":instrumentation",
        ":riscv_cheriot_decoder",
        ":riscv_cheriot_rvv_decoder",
        ":riscv_cheriot_rvv_fp_decoder",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_mpact-riscv//riscv:riscv_arm_semihost",
        "@com_google_mpact-riscv//riscv:riscv_clint",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-riscv//riscv:stoull_wrapper",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:core_debug_interface",
        "@com_google_mpact-sim//mpact/sim/generic:debug_command_shell_interface",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/proto:component_data_cc_proto",
        "@com_google_mpact-sim//mpact/sim/util/memory",
        "@com_google_mpact-sim//mpact/sim/util/other:instruction_profiler",
        "@com_google_mpact-sim//mpact/sim/util/program_loader:elf_loader",
        "@com_google_mpact-sim//mpact/sim/util/renode",
        "@com_google_mpact-sim//mpact/sim/util/renode:renode_debug_interface",
        "@com_google_mpact-sim//mpact/sim/util/renode:socket_cli",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cheriot_test_rig_lib",
    srcs = [
        "cheriot_test_rig.cc",
        "cheriot_test_rig_decoder.cc",
    ],
    hdrs = [
        "cheriot_test_rig.h",
        "cheriot_test_rig_decoder.h",
        "test_rig_packets.h",
    ],
    deps = [
        ":cheriot_state",
        ":riscv_cheriot_bin_fmt",
        ":riscv_cheriot_decoder",
        ":riscv_cheriot_isa",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:component",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:counters",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:program_error",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
    ],
)

cc_binary(
    name = "cheriot_test_rig",
    srcs = ["cheriot_test_rig_main.cc"],
    deps = [
        ":cheriot_test_rig_lib",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
    ],
)
