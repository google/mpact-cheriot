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

# Building

MPACT-Cheriot uses the [bazel](https://bazel.build) build system. This is best
installed using [bazilisk](https://github.com/bazelbuild/bazelisk). Place a file
named .bazeliskrc in your home directory specifying a bazel version 6.1.1 or
later:

> USE_BAZEL_VERSION=6.1.1

Once that has been set up, you can build all targets from the top level
directory using the command `bazel build ...:all`. To run all the tests, use the
command `bazel test ...:all`.
