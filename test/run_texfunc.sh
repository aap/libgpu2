#!/bin/sh
# Differential test for src/texfunc.c against the 1998 object.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

MANGLED=Func__7TexFuncR8PixColorT1

tools/gcc272/g++272 -O -Iinclude -c src/texfunc.c -o "$T/texfunc-new.o"
objcopy --redefine-sym $MANGLED=new_func "$T/texfunc-new.o" "$T/new.o"
objcopy --redefine-sym $MANGLED=old_func orig/lib/texfunc.o "$T/old.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_texfunc.c -o "$T/main.o"

ld -m elf_i386 -o "$T/diff_texfunc" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/main.o" "$T/old.o" "$T/new.o" \
    -L/usr/lib32 -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_texfunc"
