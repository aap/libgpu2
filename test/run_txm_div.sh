#!/bin/sh
# Differential test for src/txm_div.c against the 1998 object.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

TD=TexDiv__12NormTexCoordiii
IT=InitTable__12NormTexCoord
MK=mktable__12NormTexCoord
EV=evalute__12NormTexCoordiii
CY=cal_y__12NormTexCoordiii
ST=_12NormTexCoord.SLOPE_TBL
OT=_12NormTexCoord.OFFSET_TBL
TI=_12NormTexCoord.table_init

tools/gcc272/g++272 -O -Iinclude -c src/txm_div.c -o "$T/txm_div-new.o"
for t in new old; do
    [ $t = new ] && src="$T/txm_div-new.o" || src=orig/lib/txm_div.o
    objcopy --redefine-sym $TD=${t}_texdiv --redefine-sym $IT=${t}_init \
        --redefine-sym $MK=${t}_mktable --redefine-sym $EV=${t}_eval \
        --redefine-sym $CY=${t}_caly --redefine-sym $ST=${t}_slope \
        --redefine-sym $OT=${t}_offset --redefine-sym $TI=${t}_tinit \
        "$src" "$T/$t.o"
done

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_txm_div.c -o "$T/main.o"

ld -m elf_i386 -o "$T/diff_txm_div" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/main.o" "$T/old.o" "$T/new.o" \
    -L/usr/lib32 -lc -lm -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_txm_div"
