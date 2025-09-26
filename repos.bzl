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
            sha256 = "9568e7799f1b4ec2d653bafa0de26c1a8d188643a46fe731a51f3184a08b034e",
            strip_prefix = "mpact-riscv-6115cfb29ca6839b03d0638504968654d73ffed6",
            url = "https://github.com/google/mpact-riscv/archive/6115cfb29ca6839b03d0638504968654d73ffed6.tar.gz",
        )
