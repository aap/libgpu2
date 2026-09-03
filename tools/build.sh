#!/bin/sh
# Build the GS-dump replay harness against libgpu2 (Sony's 1998 i386 GS model).
#
# The archive is 32-bit i386 only, so everything here is -m32.  Void's gcc is
# not multilib, so clang cross-compiles to i386 and GNU ld links by hand.
#
# Prerequisites (Void):  xbps-install -S glibc-devel-32bit libX11-32bit
#
# The link uses orig/lib/libgpu2-patched.a (GS_SaveImage fwrite 3->4, see
# othersrc FINDINGS.md section 9 -- the patch is in the libgpu2.o member;
# the archive's gpu2.o member is byte-identical to pristine).  No harness
# binary calls GS_SaveImage, so pristine vs patched makes no difference;
# our src/libgpu2.c matches the PRISTINE object, original bug intact.
#
# HYBRID BUILDS (decompilation verification): objects listed in $REPLACE are
# taken from ../src (compiled here) instead of the original archive, e.g.
#     REPLACE="addrconv slong" ./build.sh
# Not-yet-decompiled objects keep coming from the 1998 archive.
#
# OWN BUILDS:  OWN=1 ./build.sh  compiles every decompiled module and links
# against an archive containing ONLY our objects - no 1998 members at all.
# Possible since the 20-object milestone: the link pulls nothing beyond
# $OWNLIST (gpu2reg/drawprim/gpu2vec are Sony's never-linked jtcl console
# and test-vector tap layer; they join the list when decompiled).
set -e
cd "$(dirname "$0")"

OWNLIST="addrconv libgpu2 pre1 pre3 slong div txm_div texfunc param pcalc \
dbg clut bitblt xif memory memif dda pcrtc txm gpu2"
[ -n "$OWN" ] && REPLACE=$OWNLIST

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
            # the era compiler: old C++ ABI, so the object drops straight in.
            # libgpu2.o was built differently from the other 22 objects
            # (RH 4.2 gcc 2.7.2.1, -O2 -m386; see doc/notes/libgpu2.md).
            # xif.c, pcrtc.c and gpu2.c include Xlib.h (via xif.h): -idirafter
            # gets the host's X11 headers searched after the era libc ones
            # (doc/notes/xif.md).
            case $m in
            libgpu2) era="env GCC272_ALT=rh42-2721 gcc272/g++272 -O2 -m386";;
            xif|pcrtc|gpu2) era="env GCC272_1998=1 gcc272/g++272 -O -idirafter /usr/include";;
            *)       era="gcc272/g++272 -O";;
            esac
            ${CC272:-$era} -I../include -c "../src/$m.c" -o "obj/$m.o"
        fi
    done
    if [ -n "$OWN" ]; then
        for m in $REPLACE; do
            ar q "$LIB" "obj/$m.o"
        done
        ar s "$LIB"
        echo "own archive: [$REPLACE] - no 1998 objects"
    else
        cp "$ORIG/lib/libgpu2-patched.a" "$LIB"
        for m in $REPLACE; do
            ar d "$LIB" "$m.o"
            ar r "$LIB" "obj/$m.o"
        done
        echo "hybrid archive: replaced [$REPLACE]"
    fi
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
