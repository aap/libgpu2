#!/bin/sh
# Differential test for src/pre1.c + src/pre3.c against the 1998 objects.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CC272="tools/gcc272/g++272"

GCC272_1998=1 $CC272 -O -Iinclude -c src/pre1.c -o "$T/pre1-new.o"
GCC272_1998=1 $CC272 -O -Iinclude -c src/pre3.c -o "$T/pre3-new.o"

# Both object sets define the same globals; rename them apart.  Every defined
# global of pre1.o/pre3.o is listed, plus the C-unfriendly vtable symbol.
mkrenames() {
	p=$1
	cat <<EOF
Reverse__Fi ${p}_Reverse
__4Pre1P4Pre3 ${p}_pre1_ctor
Put__4Pre1ix ${p}_pre1_put
SendData__4Pre1 ${p}_SendData
SendRegister__4Pre1ix ${p}_SendRegister
MaxExp__4Pre1 ${p}_MaxExp
Register__4Pre3P4Pre1 ${p}_Register
Float2Fix__4Pre3iUi ${p}_Float2Fix
SetAttr__4Pre3P4Pre1 ${p}_SetAttr
Triangle__4Pre3P4Pre1 ${p}_Triangle
Point__4Pre3P4Pre1 ${p}_Point
Line__4Pre3P4Pre1 ${p}_Line
Sprite__4Pre3P4Pre1 ${p}_Sprite
Primitive__4Pre3P4Pre1 ${p}_Primitive
Put__4Pre3P4Pre1 ${p}_pre3_put
NumVertex__4Pre3 ${p}_NumVertex
__4Pre3P5PCalc ${p}_pre3_ctor
_vt.4Pre3 ${p}_vt_pre3
EOF
}
mkrenames o > "$T/ren-o"
mkrenames n > "$T/ren-n"

objcopy --redefine-syms="$T/ren-o" orig/lib/pre1.o "$T/o-pre1.o"
objcopy --redefine-syms="$T/ren-o" orig/lib/pre3.o "$T/o-pre3.o"
objcopy --redefine-syms="$T/ren-n" "$T/pre1-new.o" "$T/n-pre1.o"
objcopy --redefine-syms="$T/ren-n" "$T/pre3-new.o" "$T/n-pre3.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_pre13.c -o "$T/main13.o"

ld -m elf_i386 -o "$T/diff_pre13" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/main13.o" "$T/o-pre1.o" "$T/o-pre3.o" "$T/n-pre1.o" "$T/n-pre3.o" \
    -L/usr/lib32 -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_pre13"
