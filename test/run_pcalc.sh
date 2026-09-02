#!/bin/sh
# Differential test for src/pcalc.c against the 1998 object.
#
# Each side gets its own param / div / slong cluster as well, so the whole
# call graph below PCalc is compared, not just PCalc itself.  The drive
# stream comes from the *original* pre1.o + pre3.o (shared by both sides).
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CC272="tools/gcc272/g++272"

GCC272_1998=1 $CC272 -O -Iinclude -c src/pcalc.c -o "$T/pcalc-new.o"
$CC272 -O -Iinclude -c src/param.c -o "$T/param-new.o"
$CC272 -O -Iinclude -c src/div.c -o "$T/div-new.o"
$CC272 -O -Iinclude -c src/slong.c -o "$T/slong-new.o"

# every defined global of pcalc.o / param.o / div.o / slong.o
mkrenames() {
	p=$1
	cat <<EOF
SwapLine__5PCalcRiN31 ${p}_swapline_i
SwapLine__5PCalcRUiN31 ${p}_swapline_u
SortVertex__5PCalcP4Pre3P5param ${p}_sortvertex
GetSPoint__5PCalc ${p}_getspoint
CorrectSPoint__5PCalc ${p}_correctspoint
CorrectEPoint__5PCalc ${p}_correctepoint
Slope__5PCalcP4Pre3P5param ${p}_slope
CheckOverFlow__5PCalc ${p}_checkoverflow
StartVal__5PCalcP4Pre3P5paramG5param ${p}_startval
GetDDAStart__5PCalcP4Pre3 ${p}_getddastart
AASlope__5PCalcxii ${p}_aaslope
C_Hosei__5PCalcxi ${p}_c_hosei
SortCoverage__5PCalcP4Pre3 ${p}_sortcoverage
AAStartVal__5PCalciiii ${p}_aastartval
AACoverage__5PCalcP4Pre3 ${p}_aacoverage
DrawTriangle__5PCalcP4Pre3 ${p}_drawtriangle
BBox__5PCalc ${p}_bbox
SortLine__5PCalcP5param ${p}_sortline
CorrectLineStart__5PCalc ${p}_correctlinestart
CorrectLineEnd__5PCalc ${p}_correctlineend
LineSlope__5PCalcP5paramiR5paramRxT4RiT6 ${p}_lineslope
LineDDAEdgeStart__5PCalc ${p}_lineddaedgestart
LineAACov__5PCalcP4Pre3 ${p}_lineaacov
DrawLine__5PCalcP4Pre3 ${p}_drawline
SpriteSlope__5PCalcxiRx ${p}_spriteslope
SpriteStartVal__5PCalcRxxxi ${p}_spritestartval
DrawSprite__5PCalcP4Pre3 ${p}_drawsprite
DrawPoint__5PCalcP4Pre3 ${p}_drawpoint
ReverseDir__5PCalc ${p}_reversedir
Primitive__5PCalcP4Pre3 ${p}_primitive
Register__5PCalcP4Pre3 ${p}_register
Put__5PCalcP4Pre3 ${p}_pcalc_put
__5PCalcP5PPDDA ${p}_pcalc_ctor
Floor__5PCalcRCi ${p}_floor
Ceil__5PCalcRCi ${p}_ceil
Subpixel__5PCalcRCi ${p}_subpixel
_vt.5PCalc ${p}_vt_pcalc
__5param ${p}_param_ctor
__as__5paramRC5param ${p}_param_asn
SetXY__5paramRC5param ${p}_param_setxy
IfMinus__5paramR5param ${p}_param_ifminus
GetAbs__5param ${p}_param_getabs
ShiftARGBSlope__5parami ${p}_param_shift
__ls__FRC5parami ${p}_param_shl
__ls__FRC5paramRCi ${p}_param_shlr
__rs__FRC5parami ${p}_param_shr
__pl__FRC5paramT0 ${p}_param_add
__mi__FRC5paramT0 ${p}_param_sub
__ml__FRC5paramT0 ${p}_param_mul
__ml__FRC5parami ${p}_param_muli
__dv__FRC5parami ${p}_param_divi
__8Reciproc ${p}_rcp_ctor
reciproc__8ReciprocxRiRxT3RUi ${p}_reciproc
Multiply__5slongxx ${p}_slong_mul
__as__5slongT0 ${p}_slong_asn
__rs__FG5slongi ${p}_slong_shr
__ls__FG5slongi ${p}_slong_shl
Combine__5slong ${p}_slong_combine
EOF
}
mkrenames o > "$T/pren-o"
mkrenames n > "$T/pren-n"

for o in pcalc param div slong; do
	objcopy --redefine-syms="$T/pren-o" "orig/lib/$o.o" "$T/o-$o.o"
	objcopy --redefine-syms="$T/pren-n" "$T/$o-new.o" "$T/n-$o.o"
done
# the driver's own Pre3 vtable needs a C-callable name
objcopy --redefine-sym _vt.4Pre3=_vt_4Pre3 orig/lib/pre3.o "$T/d-pre3.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_pcalc.c -o "$T/mainpc.o"

ld -m elf_i386 -o "$T/diff_pcalc" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/mainpc.o" orig/lib/pre1.o "$T/d-pre3.o" \
    "$T/o-pcalc.o" "$T/o-param.o" "$T/o-div.o" "$T/o-slong.o" \
    "$T/n-pcalc.o" "$T/n-param.o" "$T/n-div.o" "$T/n-slong.o" \
    -L/usr/lib32 -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_pcalc" "$@"
