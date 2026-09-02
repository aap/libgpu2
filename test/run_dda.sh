#!/bin/sh
# Differential test for src/dda.c against the 1998 object.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CC272="tools/gcc272/g++272"

GCC272_1998=1 $CC272 -O -Iinclude -c src/dda.c -o "$T/dda-new.o"

mkrenames() {
	p=$1
	cat <<EOF
__3DDAP6DDATXM ${p}_dda_ctor
Put__3DDAP5PCalc ${p}_dda_put
InitStamp__3DDA ${p}_InitStamp
Stamping__3DDAi ${p}_Stamping
InitWalk__3DDA ${p}_InitWalk
HorizontalWalk__3DDA ${p}_HorizontalWalk
VerticalWalk__3DDA ${p}_VerticalWalk
IsVerticalWalk__3DDA ${p}_IsVerticalWalk
IsWalk__3DDA ${p}_IsWalk
Register__3DDA ${p}_Register
Primitive__3DDA ${p}_Primitive
_vt.3DDA ${p}_vt_dda
EOF
}
mkrenames o > "$T/ren-od"
mkrenames n > "$T/ren-nd"

objcopy --redefine-syms="$T/ren-od" orig/lib/dda.o "$T/o-dda.o"
objcopy --redefine-syms="$T/ren-nd" "$T/dda-new.o" "$T/n-dda.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_dda.c -o "$T/maindda.o"

ld -m elf_i386 -o "$T/diff_dda" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/maindda.o" "$T/o-dda.o" "$T/n-dda.o" \
    -L/usr/lib32 -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_dda" "$@"
