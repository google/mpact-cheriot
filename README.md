# MPACT-Cheriot

MPACT-Cheriot is an implementation of an instruction set simulator for the
[CherIoT](https://cheriot.org) instruction set architecture created using the
[MPACT-Sim](https://github.com/google/mpact-sim) simulator tools framework and
reusing code from [MPACT-RiscV](https://github.com/google/mpact-riscv).
Additional information and codegen tools can be found at
[https://cheriot.org](https://cheriot.org)

There are three main targets in cheriot/BUILD:

*   mpact_cheriot

    This is the standalone simulator target. It implements a simple command line
    assembly level debug interface that is accessed by passing the command line
    option '-i' or '-interactive'.

*   renode_mpact_cheriot

    This produces a shared object, a '.so' file, that can be used together with
    ReNode, and that is loaded from MpactCheriotCPU.cs plugin file that can be
    found in mpact_sim:/util/renode/renode_cs. A debug interface can be made
    available on a socket that you can connect to with telnet or putty. This
    allows you to step, set breakpoints/watchpoints, etc. while running an
    application on ReNode.

*   cheriot_test_rig

    This produces an executable suitable for running with TestRIG and comparing
    against the sail cheriot implementation. For more on that, see the TestRIG
    documentation.

## Building

### Bazel

MPACT-Sim utilizes the [Bazel](https://bazel.build/) build system. The easiest
way to install bazel is to use
[Bazelisk](https://github.com/bazelbuild/bazelisk), a wrapper for Bazel that
automates selecting and downloading the right version of bazel. Use `brew
install bazelisk` on macOS, `choco install bazelisk` on Windows, and on linux,
download the Bazelisk binary, add it to your `PATH`, then alias bazel to the
bazelisk binary.

### Java

MPACT-Sim depends on Java, so a reasonable JRE has to be installed. For macOS,
run `brew install java`, on linux `sudo apt install default-jre`, and on Windows
follow the appropriate instructions at [java.com](https://java.com).

### Build and Test

To build the mpact-sim libraries, use the command `bazel build ...:all` from the
top level directory. To run the tests, use the command `bazel test ...:all`
