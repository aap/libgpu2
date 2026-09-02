#!/bin/sh
# Differential test for src/txm.c against the 1998 object.
#
# The two texture machines are renamed apart and linked into one binary
# together with the *original* addrconv.o, clut.o, texfunc.o and
# txm_div.o, which both sides share: TXM calls them with its own
# subobjects as `this', so sharing them keeps the comparison about txm.o
# alone.  Each side gets its own 16 MB VRAM arena and its own fake
# MemIF, so the only thing that can differ is txm.o's behaviour.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CC272="tools/gcc272/g++272"

GCC272_1998=1 $CC272 -O -Iinclude -c src/txm.c -o "$T/txm-new.o"

mkrenames() {
	p=$1
	cat <<EOF
Put__3TXMP3DDA ${p}_put
GetOneTexel__3TXMiiiR8PixColor ${p}_getonetexel
__3TXMP5MemIF ${p}_txm_ctor
ClampQ__3TXMi ${p}_clampq
ClampT__3TXMi ${p}_clampt
ClampLod__3TXMi ${p}_clamplod
MFilter1__3TXMiii ${p}_mfilter1
LFilter1__3TXMiiiiii ${p}_lfilter1
_GLOBAL_.I._3TXM.valid8 ${p}_valid8_init
_3TXM.valid8 ${p}_valid8
NFilter__3TXMR12NormTexCoordT1iR8PixColor ${p}_nfilter
LFilter__3TXMR12NormTexCoordT1iR8PixColor ${p}_lfilter
NMNFilter__3TXMR12NormTexCoordT1iR8PixColor ${p}_nmnfilter
NMLFilter__3TXMR12NormTexCoordT1iR8PixColor ${p}_nmlfilter
LMNFilter__3TXMR12NormTexCoordT1iR8PixColor ${p}_lmnfilter
LMLFilter__3TXMR12NormTexCoordT1iR8PixColor ${p}_lmlfilter
Texturing__3TXMR5Pixeli10Gpu2RegFST ${p}_texturing
ComputeLod__3TXMR10PixelStamp ${p}_computelod
Stamp__3TXMR10PixelStamp ${p}_stamp
ExtCov__3TXMR10PixelStamp ${p}_extcov
AA1__3TXMR10PixelStamp ${p}_aa1
MipTbpAuto__7TexAttr ${p}_miptbpauto
Set__2AAPC3DDA ${p}_aaset
Fogging__3FogR5Pixel ${p}_fogging
SearchQlevel__3TXMR10PixelStamp ${p}_searchqlevel
SetFOGCOL__3TXMx ${p}_setfogcol
SetTEXA__3TXMx ${p}_settexa
SetTEXCLUT__3TXMx ${p}_settexclut
SetTEX2__3TXMix ${p}_settex2
SetContext__3TXM11Gpu2RegCtxt ${p}_setcontext
SetFRAME__3TXMix ${p}_setframe
SetSCISSOR__3TXMix ${p}_setscissor
SetCLAMP__3TXMix ${p}_setclamp
SetMIPTBP2__3TXMix ${p}_setmiptbp2
SetMIPTBP1__3TXMix ${p}_setmiptbp1
SetTEX1__3TXMix ${p}_settex1
SetTEX0__3TXMix ${p}_settex0
Context__3TXM ${p}_context
AAMask__C10PixelStamp ${p}_aamask
_vt.3TXM ${p}_vt_txm
_vt.6DDATXM ${p}_vt_ddatxm
EOF
}
mkrenames o > "$T/ren-ot"
mkrenames n > "$T/ren-nt"

objcopy --redefine-syms="$T/ren-ot" orig/lib/txm.o "$T/o-txm.o"
objcopy --redefine-syms="$T/ren-nt" "$T/txm-new.o" "$T/n-txm.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -mstackrealign \
    -c test/diff_txm.c -o "$T/maintxm.o"

ld -m elf_i386 -o "$T/diff_txm" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/maintxm.o" "$T/o-txm.o" "$T/n-txm.o" \
    orig/lib/addrconv.o orig/lib/clut.o orig/lib/texfunc.o \
    orig/lib/txm_div.o \
    -L/usr/lib32 -lm -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_txm" "$@"
