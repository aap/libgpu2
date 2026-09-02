#!/bin/sh
# Differential test for src/slong.c against the 1998 object.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

MUL=Multiply__5slongxx
ASN=__as__5slongT0
SHR=__rs__FG5slongi
SHL=__ls__FG5slongi
COMB=Combine__5slong

tools/gcc272/g++272 -O -Iinclude -c src/slong.c -o "$T/slong-new.o"
for t in new old; do
    [ $t = new ] && src="$T/slong-new.o" || src=orig/lib/slong.o
    objcopy --redefine-sym $MUL=${t}_mul --redefine-sym $ASN=${t}_asn \
        --redefine-sym $SHR=${t}_shr --redefine-sym $SHL=${t}_shl \
        --redefine-sym $COMB=${t}_comb "$src" "$T/$t.o"
done

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_slong.c -o "$T/main.o"

ld -m elf_i386 -o "$T/diff_slong" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/main.o" "$T/old.o" "$T/new.o" \
    -L/usr/lib32 -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_slong"
