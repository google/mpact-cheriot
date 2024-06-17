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

"""Load dependent repositories"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def mpact_cheriot_repos():
    """ Load dependencies needed to use mpact-riscv as a 3rd-party consumer"""

    if not native.existing_rule("com_google_mpact-riscv"):
        http_archive(
            name = "com_google_mpact-riscv",
            sha256 = "794987c7ad5ff3c609f6db2805d599e0c9eca9d3040d6d9cd244fb89b88161a8",
            strip_prefix = "mpact-riscv-e5b4a8b3a84a4eba84d48a272bae1476a838a9ee",
            url = "https://github.com/google/mpact-riscv/archive/e5b4a8b3a84a4eba84d48a272bae1476a838a9ee.tar.gz",
        )
