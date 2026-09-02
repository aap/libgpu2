#!/bin/sh
# Build the GS-dump replay harness against libgpu2 (Sony's 1998 i386 GS model).
#
# The archive is 32-bit i386 only, so everything here is -m32.  Void's gcc is
# not multilib, so clang cross-compiles to i386 and GNU ld links by hand.
#
# Prerequisites (Void):  xbps-install -S glibc-devel-32bit libX11-32bit
#
# The link uses orig/lib/libgpu2-patched.a (GS_SaveImage fwrite 3->4, see
# othersrc FINDINGS.md section 9); gsreplay never calls GS_SaveImage, so
# pristine vs patched makes no difference to it.
#
# HYBRID BUILDS (decompilation verification): objects listed in $REPLACE are
# taken from ../src (compiled here) instead of the original archive, e.g.
#     REPLACE="addrconv slong" ./build.sh
# Not-yet-decompiled objects keep coming from the 1998 archive.
set -e
cd "$(dirname "$0")"

ORIG=../orig
INC=$ORIG/include

CC="clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector -g"

# ---- assemble the archive for this build --------------------------------
LIB=libgpu2-build.a
rm -f "$LIB"
if [ -n "$REPLACE" ]; then
    mkdir -p obj
    set -- ""
    for m in $REPLACE; do
        # our replacement: prefer a prebuilt obj/, else compile ../src/$m.c
        if [ -f "../src/$m.o" ]; then
            cp "../src/$m.o" "obj/$m.o"
        else
            ${CC272:-$CC} -I"$INC" -I../include -c "../src/$m.c" -o "obj/$m.o"
        fi
    done
    cp "$ORIG/lib/libgpu2-patched.a" "$LIB"
    for m in $REPLACE; do
        ar d "$LIB" "$m.o"
        ar r "$LIB" "obj/$m.o"
    done
    echo "hybrid archive: replaced [$REPLACE]"
else
    cp "$ORIG/lib/libgpu2-patched.a" "$LIB"
fi

# ---- harness ------------------------------------------------------------
$CC -I"$INC" -include compat.h -c shims.c    -o shims.o
$CC -I"$INC" -include compat.h -c probe.c    -o probe.o
$CC -I"$INC" -include compat.h -c swz.c      -o swz.o
$CC -I"$INC" -include compat.h -c fmt.c      -o fmt.o
$CC -I"$INC" -include compat.h -c regprobe.c -o regprobe.o
$CC -I"$INC" -include compat.h -c gsreplay.c -o gsreplay.o

link() {
    out=$1; shift
    ld -m elf_i386 -o "$out" \
        --dynamic-linker=/lib/ld-linux.so.2 \
        /usr/lib32/crt1.o /usr/lib32/crti.o \
        "$@" shims.o \
        "$LIB" \
        -L/usr/lib32 -l:libX11.so.6 -lm -lc -l:libc_nonshared.a \
        /usr/lib32/crtn.o
}

link probe    probe.o
link swz      swz.o
link fmt      fmt.o
link regprobe regprobe.o
link gsreplay gsreplay.o
echo "built: probe swz fmt regprobe gsreplay"
