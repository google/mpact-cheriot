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
            sha256 = "3d611cb829263e3e0932637ac684f04eb0493af983d49f21e1dbac0579393c06",
            strip_prefix = "mpact-riscv-ed34cf1b442e5d97bb003012d86ac7b9b587e702",
            url = "https://github.com/google/mpact-riscv/archive/ed34cf1b442e5d97bb003012d86ac7b9b587e702.tar.gz",
        )
