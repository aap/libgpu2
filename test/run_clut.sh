#!/bin/sh
# Differential test for src/clut.c against the 1998 object.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CC272="tools/gcc272/g++272"

GCC272_1998=1 $CC272 -O -Iinclude -c src/clut.c -o "$T/clut-new.o"

mkrenames() {
	p=$1
	cat <<EOF
load1__7TexClutR8ClutAttr ${p}_load1
load2__7TexClutR8ClutAttr ${p}_load2
LoadData__7TexClutR8ClutAttr ${p}_LoadData
Lookup__7TexCluti ${p}_Lookup
EOF
}
mkrenames o > "$T/ren-oc"
mkrenames n > "$T/ren-nc"

objcopy --redefine-syms="$T/ren-oc" orig/lib/clut.o "$T/o-clut.o"
objcopy --redefine-syms="$T/ren-nc" "$T/clut-new.o" "$T/n-clut.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_clut.c -o "$T/mainclut.o"

# addrconv.o is byte-identical to ours, so one shared copy serves both halves
ld -m elf_i386 -o "$T/diff_clut" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/mainclut.o" "$T/o-clut.o" "$T/n-clut.o" orig/lib/addrconv.o \
    -L/usr/lib32 -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_clut"
