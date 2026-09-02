#!/bin/sh
# Differential test for src/div.c against the 1998 object.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CTOR=__8Reciproc
REC=reciproc__8ReciprocxRiRxT3RUi

tools/gcc272/g++272 -O -Iinclude -c src/div.c -o "$T/div-new.o"
objcopy --redefine-sym $CTOR=new_ctor --redefine-sym $REC=new_rec \
    "$T/div-new.o" "$T/new.o"
objcopy --redefine-sym $CTOR=old_ctor --redefine-sym $REC=old_rec \
    orig/lib/div.o "$T/old.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_div.c -o "$T/main.o"

ld -m elf_i386 -o "$T/diff_div" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/main.o" "$T/old.o" "$T/new.o" \
    -L/usr/lib32 -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_div"
