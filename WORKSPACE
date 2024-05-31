# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

workspace(name = "com_google_mpact-cheriot")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

# MPACT-Sim repo
http_archive(
    name = "com_google_mpact-sim",
    sha256 = "384daa4f4bcacf0552cebdb5b3001b51af19dfed3dfa3367de5e3babcd65511d",
    strip_prefix = "mpact-sim-a3dbb21f3f3d3193a8d0b6667f1d0807d1dc4dd4",
    url = "https://github.com/google/mpact-sim/archive/a3dbb21f3f3d3193a8d0b6667f1d0807d1dc4dd4.tar.gz"
)

# MPACT-RiscV repo
http_archive(
    name = "com_google_mpact-riscv",
    sha256 = "c30dc2ec57fd476d833dc8e5c02045acdfda4136cb5c2376044432d453aecd4d",
    strip_prefix = "mpact-riscv-a43a92ba38d8260f4207416e11721ceee7135624",
    url = "https://github.com/google/mpact-riscv/archive/a43a92ba38d8260f4207416e11721ceee7135624.tar.gz"
)

# MPACT-ReNode repo will go here.
git_repository(
  name = "com_google_mpact-renode",
  branch = "main",
  remote = "https://mpact.googlesource.com/mpact-renode",
)

load("@com_google_mpact-sim//:repos.bzl", "mpact_sim_repos")

mpact_sim_repos()

load("@com_google_mpact-sim//:deps.bzl", "mpact_sim_deps")

mpact_sim_deps()
