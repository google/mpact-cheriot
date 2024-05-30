# Copyright 2024 Google LLC
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

"""Call the first level dependent repos to load their dependencies."""

load("@com_google_mpact-riscv//:repos.bzl", "mpact_riscv_repos")

def mpact_cheriot_riscv_repos():
    """ Extra dependencies to finish setting up repositories"""

    mpact_riscv_repos()
