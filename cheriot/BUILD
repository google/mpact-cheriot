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

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:public"],
)

exports_files([
    "riscv_cheriot.bin_fmt",
    "riscv_cheriot.isa",
])

mpact_isa_decoder(
    name = "riscv_cheriot_isa",
    src = "riscv_cheriot.isa",
    includes = [],
    isa_name = "RiscVCheriot",
    deps = [
        ":riscv_cheriot",
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

cc_library(
    name = "riscv_cheriot",
    srcs = [
        "cheriot_register.cc",
        "cheriot_state.cc",
        "riscv_cheriot_a_instructions.cc",
        "riscv_cheriot_i_instructions.cc",
        "riscv_cheriot_instructions.cc",
        "riscv_cheriot_m_instructions.cc",
        "riscv_cheriot_minstret.cc",
        "riscv_cheriot_priv_instructions.cc",
        "riscv_cheriot_zicsr_instructions.cc",
    ],
    hdrs = [
        "cheriot_register.h",
        "cheriot_state.h",
        "riscv_cheriot_a_instructions.h",
        "riscv_cheriot_csr_enum.h",
        "riscv_cheriot_i_instructions.h",
        "riscv_cheriot_instruction_helpers.h",
        "riscv_cheriot_instructions.h",
        "riscv_cheriot_m_instructions.h",
        "riscv_cheriot_minstret.h",
        "riscv_cheriot_priv_instructions.h",
        "riscv_cheriot_zicsr_instructions.h",
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
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-riscv//riscv:stoull_wrapper",
        "@com_google_mpact-sim//mpact/sim/generic:arch_state",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:counters",
        "@com_google_mpact-sim//mpact/sim/generic:instruction",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
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
        "riscv_cheriot_register_aliases.h",
    ],
    deps = [
        ":riscv_cheriot",
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
        ":riscv_cheriot",
        ":riscv_cheriot_decoder",
        ":riscv_cheriot_isa",
        "@com_google_absl//absl/container:flat_hash_map",
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
        "@com_google_mpact-riscv//riscv:riscv_breakpoint",
        "@com_google_mpact-riscv//riscv:riscv_state",
        "@com_google_mpact-sim//mpact/sim/generic:component",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:core_debug_interface",
        "@com_google_mpact-sim//mpact/sim/generic:counters",
        "@com_google_mpact-sim//mpact/sim/generic:decode_cache",
        "@com_google_mpact-sim//mpact/sim/generic:type_helpers",
        "@com_google_mpact-sim//mpact/sim/util/memory",
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
        ":cheriot_top",
        ":riscv_cheriot",
        ":riscv_cheriot_decoder",
        ":riscv_cheriot_isa",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_mpact-riscv//riscv:stoull_wrapper",
        "@com_google_mpact-sim//mpact/sim/generic:core",
        "@com_google_mpact-sim//mpact/sim/generic:core_debug_interface",
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
        ":cheriot_top",
        ":debug_command_shell",
        ":instrumentation",
        ":riscv_cheriot",
        ":riscv_cheriot_decoder",
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
        "@com_google_mpact-riscv//riscv:stoull_wrapper",
        "@com_google_mpact-sim//mpact/sim/generic:core_debug_interface",
        "@com_google_mpact-sim//mpact/sim/generic:counters",
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
        ":riscv_cheriot",
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
        ":cheriot_top",
        ":debug_command_shell",
        ":instrumentation",
        ":riscv_cheriot",
        ":riscv_cheriot_decoder",
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
        ":riscv_cheriot",
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
