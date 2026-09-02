#!/bin/sh
# Differential test for src/memif.c against the 1998 object.
#
# memif.o depends on memory.o (FBConfig/ZBConfig::ReadStamp/WriteStamp and
# Memory::SetRegister), so both objects are renamed per side and linked
# together: each side's memif resolves to its own memory.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CC272="tools/gcc272/g++272"

GCC272_1998=1 $CC272 -O -Iinclude -c src/memory.c -o "$T/memory-new.o"
GCC272_1998=1 $CC272 -O -Iinclude -c src/memif.c  -o "$T/memif-new.o"

mkrenames() {
	p=$1
	cat <<EOF
ReadPixel__8FBConfigP6Memoryii ${p}_FBReadPixel
ReadStamp__8FBConfigP6MemoryR10PixelStamp ${p}_FBReadStamp
WritePixel__8FBConfigP6MemoryiiG8PixColoriii ${p}_FBWritePixel
WriteStamp__8FBConfigP6MemoryR10PixelStamp ${p}_FBWriteStamp
ReadZ__8ZBConfigP6Memoryii ${p}_ZBReadZ
ReadStamp__8ZBConfigP6MemoryR10PixelStamp ${p}_ZBReadStamp
WriteZ__8ZBConfigP6MemoryiiUi ${p}_ZBWriteZ
WriteStamp__8ZBConfigP6MemoryR10PixelStamp ${p}_ZBWriteStamp
SetRegister__6Memoryix ${p}_SetRegister
Pass__9AlphaTesti ${p}_Pass
ATest__9AlphaTestR10PixelStamp ${p}_ATest
DATest__10DAlphaTestP6MemoryR10PixelStamp ${p}_DATest
ZTest__9DepthTestP6MemoryR10PixelStamp ${p}_ZTest
Blend__10AlphaBlendP6MemoryR10PixelStamp ${p}_Blend
Dithering__6DitherR10PixelStamp ${p}_Dithering
Clamp__10ColorClampR10PixelStamp ${p}_Clamp
Stamp__5MemIFR10PixelStamp ${p}_Stamp
ReadWord__5MemIFi ${p}_ReadWord
__5MemIFP6Memory ${p}_MemIFctor
SetContext__5MemIF11Gpu2RegCtxt ${p}_SetContext
SetPABE__5MemIFx ${p}_SetPABE
SetCOLCLAMP__5MemIFx ${p}_SetCOLCLAMP
SetDTHE__5MemIFx ${p}_SetDTHE
SetDIMX__5MemIFx ${p}_SetDIMX
SetALPHA__5MemIFix ${p}_SetALPHA
SetTEST__5MemIFix ${p}_SetTEST
Context__5MemIF ${p}_Context
AAMask__C10PixelStamp ${p}_AAMask
_vt.5MemIF ${p}_vtMemIF
EOF
}
mkrenames o > "$T/ren-oi"
mkrenames n > "$T/ren-ni"

objcopy --redefine-syms="$T/ren-oi" orig/lib/memory.o "$T/oi-memory.o"
objcopy --redefine-syms="$T/ren-oi" orig/lib/memif.o  "$T/oi-memif.o"
objcopy --redefine-syms="$T/ren-ni" "$T/memory-new.o" "$T/ni-memory.o"
objcopy --redefine-syms="$T/ren-ni" "$T/memif-new.o"  "$T/ni-memif.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_memif.c -o "$T/mainmif.o"

ld -m elf_i386 -o "$T/diff_memif" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/mainmif.o" "$T/oi-memif.o" "$T/oi-memory.o" \
    "$T/ni-memif.o" "$T/ni-memory.o" \
    orig/lib/bitblt.o orig/lib/addrconv.o \
    -L/usr/lib32 -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_memif"
