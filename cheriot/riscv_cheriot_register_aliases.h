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

#ifndef MPACT_CHERIOT__RISCV_CHERIOT_REGISTER_ALIASES_H_
#define MPACT_CHERIOT__RISCV_CHERIOT_REGISTER_ALIASES_H_

namespace mpact {
namespace sim {
namespace cheriot {

constexpr char kXRegisterAliases[32][6] = {
    /* x0 */ "zero",
    /* x1 */ "ra",
    /* x2 */ "sp",
    /* x3 */ "gp",
    /* x4 */ "tp",
    /* x5 */ "t0",
    /* x6 */ "t1",
    /* x7 */ "t2",
    /* x8 */ "s0",
    /* x9 */ "s1",
    /* x10 */ "a0",
    /* x11 */ "a1",
    /* x12 */ "a2",
    /* x13 */ "a3",
    /* x14 */ "a4",
    /* x15 */ "a5",
    /* x16 */ "a6",
    /* x17 */ "a7",
    /* x18 */ "s2",
    /* x19 */ "s3",
    /* x20 */ "s4",
    /* x21 */ "s5",
    /* x22 */ "s6",
    /* x23 */ "s7",
    /* x24 */ "s8",
    /* x25 */ "s9",
    /* x26 */ "s10",
    /* x27 */ "s11",
    /* x28 */ "t3",
    /* x29 */ "t4",
    /* x30 */ "t5",
    /* x31 */ "t6"};
constexpr char kCRegisterAliases[32][6] = {
    "czero", "cra", "csp",  "cgp",  "ctp", "ct0", "ct1", "ct2",
    "cs0",   "cs1", "ca0",  "ca1",  "ca2", "ca3", "ca4", "ca5",
    "ca6",   "ca7", "cs2",  "cs3",  "cs4", "cs5", "cs6", "cs7",
    "cs8",   "cs9", "cs10", "cs11", "ct3", "ct4", "ct5", "ct6"};
constexpr char kFRegisterAliases[32][6] = {
    "ft0", "ft1", "ft2",  "ft3",  "ft4", "ft5", "ft6",  "ft7",
    "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
    "fa6", "fa7", "fs2",  "fs3",  "fs4", "fs5", "fs6",  "fs7",
    "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__RISCV_CHERIOT_REGISTER_ALIASES_H_
