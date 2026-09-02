#!/bin/sh
# Differential test for src/memory.c against the 1998 object.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CC272="tools/gcc272/g++272"

GCC272_1998=1 $CC272 -O -Iinclude -c src/memory.c -o "$T/memory-new.o"

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
EOF
}
mkrenames o > "$T/ren-om"
mkrenames n > "$T/ren-nm"

objcopy --redefine-syms="$T/ren-om" orig/lib/memory.o "$T/o-memory.o"
objcopy --redefine-syms="$T/ren-nm" "$T/memory-new.o" "$T/n-memory.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_memory.c -o "$T/mainmem.o"

# addrconv.o is byte-identical to ours; bitblt.o is the shared transfer
# engine Memory::SetRegister drives, not code under test.
ld -m elf_i386 -o "$T/diff_memory" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/mainmem.o" "$T/o-memory.o" "$T/n-memory.o" \
    orig/lib/bitblt.o orig/lib/addrconv.o \
    -L/usr/lib32 -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_memory"
