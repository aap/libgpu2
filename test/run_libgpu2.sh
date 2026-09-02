#!/bin/sh
# Byte-diff src/libgpu2.c against Sony's 1998 orig/lib/libgpu2.o, built with
# the Debian hamm g++ 2.7.2.3 (tools/gcc272).
#
#   test/run_libgpu2.sh              per-function residual byte counts
#   test/run_libgpu2.sh -d           plus a whole-object disassembly diff
#   test/run_libgpu2.sh -f GS_...    plus one function side by side
#
# -m386 is required: libgpu2.o has `leave' epilogues and .align 4 function
# padding, i.e. it was built by an i386-configured driver, while this g++272
# is i486-linux (mov %ebp,%esp / .align 16).  See doc/notes/libgpu2.md.
#
# libgpu2.o is the one object in the archive built by gcc **2.7.2.1** (all the
# others say 2.7.2.3); test/run_libgpu2_721.sh drives the real 2.7.2.1
# cc1plus and is the authoritative check.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

tools/gcc272/g++272 -O2 -m386 -Iinclude -c src/libgpu2.c -o "$T/libgpu2-new.o"
exec test/run_libgpu2_cmp.sh "$T/libgpu2-new.o" "$@"
