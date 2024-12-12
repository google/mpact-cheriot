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
            sha256 = "d9210d4ac6553c18264ca1fa6bfddfb922502aaf43929dfadab02c51c5312872",
            strip_prefix = "mpact-riscv-5cb491addf4b28fa82fb5b3eaacaae38d7c0bdad",
            url = "https://github.com/google/mpact-riscv/archive/5cb491addf4b28fa82fb5b3eaacaae38d7c0bdad.tar.gz",
        )
