#!/bin/sh
# Build the GS model and the replay harness with a MODERN compiler.
#
# tools/build.sh is the 1998 path: the era g++ 2.7.2.3 compiles the model,
# and everything is i386 because the original archive is.  This script is
# the other one: the same src/ and include/, compiled by the host's own
# g++ (or clang), natively 64-bit by default.  Both builds share every
# header; the modern-only differences are guarded `#if __GNUC__ >= 3' or
# written so the era compiler folds them away (doc/notes/modern-port.md).
#
#     tools/build-modern.sh              # amd64, the default
#     BITS=32 tools/build-modern.sh      # i386, for comparison with build.sh
#     CXX=clang++ CC=clang tools/build-modern.sh
#
# Products (all suffixed with $BITS so the era binaries are never
# clobbered):
#
#     tools/libgpu2-modern.a       the 23-object model archive (of whichever
#                                  width this invocation built; the binaries
#                                  are statically linked, so both widths'
#                                  binaries coexist regardless)
#     tools/obj-modern$BITS/       its objects
#     tools/probe$BITS             the behaviour suite ("0 failures")
#     tools/gsreplay$BITS          the dump replayer
#     tools/swz$BITS tools/fmt$BITS tools/regprobe$BITS
#
# Verified: probe reports 0 failures and the three corpus dumps replay to
# the era build's exact end-state md5s, at both 32 and 64 bits.
#
# Prerequisites: 64-bit libX11 + headers.  BITS=32 additionally wants a
# 32-bit userland to link against (Void: glibc-devel-32bit libX11-32bit);
# it links by hand with ld because a non-multilib gcc has neither 32-bit
# libgcc nor 32-bit libstdc++ -- tools/shims-modern.c supplies what is
# missing from both.
set -e
cd "$(dirname "$0")/.."

BITS=${BITS:-64}
CXX=${CXX:-g++}
CC=${CC:-gcc}
OBJ=tools/obj-modern$BITS
LIB=tools/libgpu2-modern.a

# The 23 archive members, in the order tools/build.sh lists them.
MODS="addrconv libgpu2 pre1 pre3 slong div txm_div texfunc param pcalc \
dbg clut bitblt xif memory memif dda pcrtc txm gpu2 gpu2reg drawprim gpu2vec"

# ---- flags --------------------------------------------------------------
# -fno-exceptions -fno-rtti: the model throws nothing and asks nothing, and
#   dropping both means the link needs no libstdc++ (there rarely is a
#   32-bit one) -- tools/shims-modern.c covers operator new and the pure
#   virtual handler by itself.
# -include include/modern.h: declarations the 1998 libc5 headers leaked.
#   Done on the command line rather than in the sources because xif.h,
#   pcrtc.h, txm.h and gpu2vec.h bake __LINE__ into their objects.
# -fno-strict-aliasing: clut.c, drawprim.c and gpu2reg.c punt floats
#   through *(int *)&f (gpu2reg.h's FtoI), which a strict-aliasing
#   optimizer is entitled to miscompile.  -fwrapv for the same reason:
#   the model shifts and negates signed values the way 1998 C++ did.
# -w: the sources are 1998 C++ and hand string literals to char* by the
#   hundred; that is the only warning class they produce.
CFLAGS="-m$BITS -O2 -fno-pie -fno-stack-protector -fno-strict-aliasing -fwrapv -w"
CXXFLAGS="$CFLAGS -fno-exceptions -fno-rtti"

# x87, not SSE.  The 1998 objects were compiled for a 387 and the model's
# runtime-built tables (div.c's reciprocal pair, txm_div.c's texture-divide
# pair) are (long long) truncations of double expressions, exactly where
# 80-bit excess precision would show.  Measured: those four tables come out
# byte-identical under x87 and SSE, and so do all three corpus replays --
# so this is belt and braces, and clang (which has no -mfpmath=387 on
# x86-64) is fine without it.
# (-fsyntax-only: both compilers reject a bad -mfpmath while processing
# options, and there is no 32-bit libstdc++ here to link a test against.)
if $CXX $CXXFLAGS -mfpmath=387 -fsyntax-only -x c++ /dev/null 2>/dev/null; then
	CXXFLAGS="$CXXFLAGS -mfpmath=387"
else
	echo "note: $CXX has no -mfpmath=387; using its default FP (verified equivalent)"
fi

# ---- the model ----------------------------------------------------------
mkdir -p "$OBJ"
for m in $MODS; do
	$CXX $CXXFLAGS -Iinclude -include include/modern.h -c "src/$m.c" -o "$OBJ/$m.o"
done
$CXX $CXXFLAGS -c tools/shims-modern.c -o "$OBJ/shims-modern.o"

rm -f "$LIB"
for m in $MODS; do
	ar q "$LIB" "$OBJ/$m.o"
done
ar s "$LIB"
echo "modern archive: $LIB ($BITS-bit, no 1998 objects)"

# ---- harness ------------------------------------------------------------
# These are C, not C++; compat.h declares the one libc5 function glibc has
# since dropped.
for t in probe swz fmt regprobe gsreplay; do
	$CC $CFLAGS -Iinclude -include tools/compat.h -c "tools/$t.c" -o "$OBJ/$t.o"
done

if [ "$BITS" = 32 ]; then
	# No 32-bit libgcc/libstdc++ on a non-multilib gcc, so drive ld
	# directly, exactly as tools/build.sh does for the 1998 objects.
	link() {
		out=$1; shift
		ld -m elf_i386 -o "$out" \
		    --dynamic-linker=/lib/ld-linux.so.2 \
		    /usr/lib32/crt1.o /usr/lib32/crti.o \
		    "$@" "$OBJ/shims-modern.o" "$LIB" \
		    -L/usr/lib32 -l:libX11.so.6 -lm -lc -l:libc_nonshared.a \
		    /usr/lib32/crtn.o
	}
else
	# Native: the system compiler driver and the system X11 will do.
	link() {
		out=$1; shift
		$CC -m64 -no-pie -o "$out" "$@" "$OBJ/shims-modern.o" "$LIB" -lX11 -lm
	}
fi

for t in probe swz fmt regprobe gsreplay; do
	link "tools/$t$BITS" "$OBJ/$t.o"
done
echo "built: probe$BITS swz$BITS fmt$BITS regprobe$BITS gsreplay$BITS"
