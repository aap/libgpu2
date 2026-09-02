#!/bin/sh
# Differential test for src/param.c against the 1998 object.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

tools/gcc272/g++272 -O -Iinclude -c src/param.c -o "$T/param-new.o"
for t in new old; do
    [ $t = new ] && src="$T/param-new.o" || src=orig/lib/param.o
    objcopy \
        --redefine-sym __5param=${t}_ctor \
        --redefine-sym __as__5paramRC5param=${t}_asn \
        --redefine-sym SetXY__5paramRC5param=${t}_setxy \
        --redefine-sym IfMinus__5paramR5param=${t}_ifminus \
        --redefine-sym GetAbs__5param=${t}_getabs \
        --redefine-sym ShiftARGBSlope__5parami=${t}_shift \
        --redefine-sym __ls__FRC5parami=${t}_shl \
        --redefine-sym __ls__FRC5paramRCi=${t}_shlr \
        --redefine-sym __rs__FRC5parami=${t}_shr \
        --redefine-sym __pl__FRC5paramT0=${t}_add \
        --redefine-sym __mi__FRC5paramT0=${t}_sub \
        --redefine-sym __ml__FRC5paramT0=${t}_mul \
        --redefine-sym __ml__FRC5parami=${t}_muli \
        --redefine-sym __dv__FRC5parami=${t}_divi \
        "$src" "$T/$t.o"
done

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_param.c -o "$T/main.o"

ld -m elf_i386 -o "$T/diff_param" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/main.o" "$T/old.o" "$T/new.o" \
    -L/usr/lib32 -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_param"
