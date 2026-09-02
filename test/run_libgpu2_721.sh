#!/bin/sh
# Byte-diff src/libgpu2.c against Sony's 1998 orig/lib/libgpu2.o, built with
# the compiler that actually produced it: **gcc 2.7.2.1** (Red Hat 4.2),
# installed as tools/gcc272/alt/rh42-2721.
#
# libgpu2.o's .comment says "GCC: (GNU) 2.7.2.1"; every other object in the
# archive says 2.7.2.3.  It is also the only object with `leave' epilogues and
# .align 4 function padding, i.e. i386 tuning -- hence -m386 as well.
#
#   test/run_libgpu2_721.sh              per-function residual byte counts
#   test/run_libgpu2_721.sh -d           plus a whole-object disassembly diff
#   test/run_libgpu2_721.sh -f GS_...    plus one function side by side
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

GCC272_ALT=rh42-2721 tools/gcc272/g++272 -O2 -m386 -Iinclude \
	-c src/libgpu2.c -o "$T/libgpu2-721.o"
exec test/run_libgpu2_cmp.sh "$T/libgpu2-721.o" "$@"
