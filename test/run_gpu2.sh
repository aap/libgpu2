#!/bin/sh
# Differential test for src/gpu2.c against the 1998 object.
#
# Both GPU2s are renamed apart and linked into one binary together with
# the *original* addrconv.o (both sides' MemRead::ReadPixel go through
# it); everything else the two objects import - the allocator, the
# out-of-line constructors, OpenWindow, the external vtables, stdio -
# is a recording stub in test/diff_gpu2.c, so the comparison is about
# gpu2.o alone.  See that file for what is exercised.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CC272="tools/gcc272/g++272"

GCC272_1998=1 $CC272 -O -Iinclude -idirafter /usr/include \
	-c src/gpu2.c -o "$T/gpu2-new.o"

mkrenames() {
	p=$1
	cat <<EOF
GetCRT__4GPU2 ${p}_getcrt
__4GPU2Pciii ${p}_ctor
Get__4GPU2 ${p}_get
Put__4GPU2ix ${p}_put
ResizeWindow__4GPU2ii ${p}_resizewindow
SetRegister__5PCRTCix ${p}_pcrtc_setreg
Resize__5PCRTCii ${p}_pcrtc_resize
Resize__8PCRTCdmy ${p}_dmy_resize
ReadPixel__9MemRead16P6MemoryiiR8PixColor ${p}_rp16
ReadPixel__9MemRead24P6MemoryiiR8PixColor ${p}_rp24
ReadPixel__9MemRead32P6MemoryiiR8PixColor ${p}_rp32
blend__12PixelBlend1aR8PixColorRC8PixColor ${p}_blend1a
blend__13PixelBlendAlpR8PixColorRC8PixColor ${p}_blendalp
Flush__11XWindowDump ${p}_xd_flush
SetBackground__11XWindowDumpiii ${p}_xd_setbg
ClearDisplay__11XWindowDump ${p}_xd_clear
Resize__11XWindowDumpii ${p}_xd_resize
DisplayPixel__11XWindowDumpiiii ${p}_xd_disppix
DrawPixel__11XWindowDumpiiiiii ${p}_xd_draw6
DrawPixel__11XWindowDumpiiiii ${p}_xd_draw5
PrepareImgBuffer__11XWindowDumpii ${p}_xd_prepare
_._11XWindowDump ${p}_xd_dtor
_._7Xifbase ${p}_xif_dtor
Put__5PPOutP5PCalc ${p}_ppout_put
__builtin_new stub_new
__builtin_delete stub_delete
__4Pre1P4Pre3 stub_pre1_ctor
Put__4Pre1ix stub_pre1_put
__5param stub_param_ctor
__8Reciproc stub_reciproc_ctor
OpenWindow__7XWindowPcii stub_openwindow
ReadPixel__6BitBLTP6Memory stub_bitblt_readpixel
WritePixel__8FBConfigP6MemoryiiG8PixColoriii stub_fbwritepixel
__pure_virtual stub_purevirtual
__assert_fail stub_assert_fail
_IO_stderr_ fake_stderr
fprintf stub_fprintf
exit stub_exit
malloc stub_malloc
realloc stub_realloc
free stub_free
_vt.3DDA fake_vt_dda
_vt.3TXM fake_vt_txm
_vt.4Pre3 fake_vt_pre3
_vt.5PCalc fake_vt_pcalc
_vt.5MemIF fake_vt_memif
_vt.7XWindow fake_vt_xwindow
_vt.8PCRTCxif fake_vt_pcrtcxif
EOF
}
mkrenames o > "$T/ren-og"
mkrenames n > "$T/ren-ng"

objcopy --redefine-syms="$T/ren-og" orig/lib/gpu2.o "$T/o-gpu2.o"
objcopy --redefine-syms="$T/ren-ng" "$T/gpu2-new.o" "$T/n-gpu2.o"

# addrconv.o's unknown-psm error path wants stdio; route it to the same
# stubs so no real libio symbols are needed
cat > "$T/ren-ac" <<EOF
_IO_stderr_ fake_stderr
fprintf stub_fprintf
exit stub_exit
EOF
objcopy --redefine-syms="$T/ren-ac" orig/lib/addrconv.o "$T/addrconv.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -mstackrealign \
    -c test/diff_gpu2.c -o "$T/maingpu2.o"

ld -m elf_i386 -o "$T/diff_gpu2" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/maingpu2.o" "$T/o-gpu2.o" "$T/n-gpu2.o" \
    "$T/addrconv.o" \
    -L/usr/lib32 -lm -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_gpu2" "$@"
