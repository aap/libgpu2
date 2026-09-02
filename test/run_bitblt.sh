#!/bin/sh
# Differential test for src/bitblt.c against the 1998 object.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CC272="tools/gcc272/g++272"

GCC272_1998=1 $CC272 -O -Iinclude -c src/bitblt.c -o "$T/bitblt-new.o"

mkrenames() {
	p=$1
	cat <<EOF
read__6BitBLTP6Memoryii ${p}_read
write__6BitBLTP6MemoryUiii ${p}_write
DoBitBLT__6BitBLTP6Memory ${p}_DoBitBLT
WritePixel__6BitBLTP6Memoryx ${p}_WritePixel
ReadPixel__6BitBLTP6Memory ${p}_ReadPixel
EOF
}
mkrenames o > "$T/ren-ob"
mkrenames n > "$T/ren-nb"

objcopy --redefine-syms="$T/ren-ob" orig/lib/bitblt.o "$T/o-bitblt.o"
objcopy --redefine-syms="$T/ren-nb" "$T/bitblt-new.o" "$T/n-bitblt.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_bitblt.c -o "$T/mainbb.o"

# addrconv.o is byte-identical to ours, so one shared copy serves both halves
ld -m elf_i386 -o "$T/diff_bitblt" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/mainbb.o" "$T/o-bitblt.o" "$T/n-bitblt.o" orig/lib/addrconv.o \
    -L/usr/lib32 -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_bitblt"
