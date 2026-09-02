#!/bin/sh
# Differential test for src/libgpu2.c against Sony's 1998 orig/lib/libgpu2.o.
#
# The eight public entry points are renamed old_*/new_* with objcopy and both
# objects are linked into one i386 binary; their bss globals are local symbols,
# so the two implementations keep independent state.  test/diff_libgpu2.c
# supplies the GPU2 stubs both call and compares every observable: the
# GPU2::Put trace, the constructor arguments, new/delete counts, return
# values, the save-area state and the GS_SaveImage output bytes.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

GCC272_ALT=rh42-2721 tools/gcc272/g++272 -O2 -m386 -Iinclude \
	-c src/libgpu2.c -o "$T/libgpu2-721.o"

SYMS="GS_InitSim GS_OpenSim GS_CloseSim GS_PutPort GS_PutCtlPort \
GS_SaveImage GS_SetSaveImageArea GS_GetSaveImageArea"

ren() {
	pfx=$1; in=$2; out=$3
	set --
	for s in $SYMS; do
		set -- "$@" --redefine-sym "$s=${pfx}_${s#GS_}"
	done
	objcopy "$@" "$in" "$out"
}
ren old orig/lib/libgpu2.o "$T/old.o"
ren new "$T/libgpu2-721.o" "$T/new.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
	-c test/diff_libgpu2.c -o "$T/diffmain.o"

# __divdi3 (GS_PutCtlPort's 64-bit divide) comes from libgcc.  The host has
# no 32-bit libgcc, so use the era one -- which is the very implementation
# the 1998 objects were linked against.
GCCLIB=tools/gcc272/root/usr/lib/gcc-lib/i486-linux/2.7.2.3/libgcc.a

ld -m elf_i386 -o "$T/diff_libgpu2" \
	--dynamic-linker=/lib/ld-linux.so.2 \
	/usr/lib32/crt1.o /usr/lib32/crti.o \
	"$T/diffmain.o" "$T/old.o" "$T/new.o" \
	${GCCLIB:+"$GCCLIB"} \
	-L/usr/lib32 -lc -l:libc_nonshared.a \
	/usr/lib32/crtn.o

exec "$T/diff_libgpu2"
