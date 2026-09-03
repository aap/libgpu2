#!/bin/sh
# Differential test for src/gpu2vec.c against the 1998 object.
#
# Both gpu2vec.o are renamed apart (every GLOBAL and WEAK definition gets
# an o_/n_ name, so the two copies of the weak header inlines and of the
# local vtables cannot be merged by the linker) and linked into one
# binary.  Everything the object imports from the rest of the archive -
# the allocator, the out-of-line constructors, the four downstream stage
# entries, OpenWindow, the external vtables, stderr and exit - is a
# recording stub in test/diff_gpu2vec.c.  <stdio.h> is NOT stubbed apart
# from fprintf (which has to serve both the vector files and the
# constructor's stderr message): fputc / sprintf / sscanf / strcat run
# for real, so the test compares the vector files byte for byte.
#
# The 1998 addrconv.o is linked in for address_convert, which the
# MemRead readers in the inlined PCRTCxif constructor reference.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CC272="tools/gcc272/g++272"

GCC272_1998=1 $CC272 -O -Iinclude -idirafter /usr/include \
	-c src/gpu2vec.c -o "$T/gpu2vec-new.o"

mkrenames() {
	p=$1
	cat <<EOF
__7GPU2VECPciii ${p}_ctor
GetCRT__7GPU2VEC ${p}_getcrt
Get__7GPU2VEC ${p}_get
Put__7GPU2VECix ${p}_put
ResizeWindow__7GPU2VECii ${p}_resizewindow
SetVector__7GPU2VECiP8_IO_FILE ${p}_setvector
Put__4MyPPix ${p}_mypp_put
RegisterVec__5MyDDAP5PCalc ${p}_dda_regvec
TriangleVec__5MyDDAP5PCalc ${p}_dda_trivec
Put__5MyDDAP5PCalc ${p}_dda_put
SetVector__5MyDDAP8_IO_FILE ${p}_dda_setvec
__5MyDDAP3TXM ${p}_dda_ctor
_._5MyDDA ${p}_dda_dtor
RegisterVec__5MyTXMP3DDA ${p}_txm_regvec
PrimitiveVec__5MyTXMP3DDA ${p}_txm_primvec
Put__5MyTXMP3DDA ${p}_txm_put
SetVector__5MyTXMP8_IO_FILE ${p}_txm_setvec
__5MyTXMP5MemIF ${p}_txm_ctor
_._5MyTXM ${p}_txm_dtor
RegisterVec__7MyMemIFR10PixelStamp ${p}_mif_regvec
PrimitiveVec__7MyMemIFR10PixelStamp ${p}_mif_primvec
Stamp__7MyMemIFR10PixelStamp ${p}_mif_stamp
SetVector__7MyMemIFP8_IO_FILE ${p}_mif_setvec
__7MyMemIFP6Memory ${p}_mif_ctor
_._7MyMemIF ${p}_mif_dtor
Dump__8MyMemory ${p}_mem_dump
_._8MyMemory ${p}_mem_dtor
_vt.5MyDDA ${p}_vt_mydda
_vt.5MyTXM ${p}_vt_mytxm
_vt.7MyMemIF ${p}_vt_mymemif
_vt.8MyMemory ${p}_vt_mymemory
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
Put__3DDAP5PCalc stub_dda_put
Put__3TXMP3DDA stub_txm_put
Stamp__5MemIFR10PixelStamp stub_memif_stamp
__pure_virtual stub_purevirtual
__assert_fail stub_assert_fail
_IO_stderr_ fake_stderr
fprintf my_fprintf
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
mkrenames o > "$T/ren-ov"
mkrenames n > "$T/ren-nv"

objcopy --redefine-syms="$T/ren-ov" orig/lib/gpu2vec.o "$T/o-gpu2vec.o"
objcopy --redefine-syms="$T/ren-nv" "$T/gpu2vec-new.o" "$T/n-gpu2vec.o"

# addrconv.o's unknown-psm error path wants stdio; route it to the same
# stubs so no real libio symbol clashes with the renamed ones
cat > "$T/ren-acv" <<EOF
_IO_stderr_ fake_stderr
fprintf my_fprintf
exit stub_exit
EOF
objcopy --redefine-syms="$T/ren-acv" orig/lib/addrconv.o "$T/addrconv-v.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -mstackrealign \
    -c test/diff_gpu2vec.c -o "$T/maingpu2vec.o"

ld -m elf_i386 -o "$T/diff_gpu2vec" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/maingpu2vec.o" "$T/o-gpu2vec.o" "$T/n-gpu2vec.o" \
    "$T/addrconv-v.o" \
    -L/usr/lib32 -lm -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_gpu2vec" "$@"
