#!/bin/sh
# Differential test for src/gpu2reg.c + src/drawprim.c against the 1998
# objects.  The 82 command handlers are static, so both sides are driven
# through their (renamed) MyCBFuncs tables; drawprim's globals are
# renamed o_*/n_* directly.  DrawLine prints its one-shot warning to
# stdout; that is expected output.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CC272="tools/gcc272/g++272"

GCC272_1998=1 $CC272 -O -Iinclude -c src/gpu2reg.c  -o "$T/gpu2reg-new.o"
GCC272_1998=1 $CC272 -O -Iinclude -c src/drawprim.c -o "$T/drawprim-new.o"

mkrenames() {
	p=$1
	cat <<EOF
MyCBFuncs ${p}_MyCBFuncs
pGPU2Reg ${p}_pGPU2Reg
__7GPU2Reg ${p}_ctor
IDTEX8Pixel__FiPcP11jtcl_data_t ${p}_IDTEX8Pixel
Vertex0__FiiP11_grfwVertex ${p}_Vertex0
Vertex1__FiiP11_grfwVertex ${p}_Vertex1
Vertex2__FiiP11_grfwVertex ${p}_Vertex2
DrawLine__FiP11_grfwVertexT1 ${p}_DrawLine
DrawTriangle__FiP11_grfwVertexN21 ${p}_DrawTriangle
EOF
}
mkrenames o > "$T/ren-o"
mkrenames n > "$T/ren-n"

objcopy --redefine-syms="$T/ren-o" orig/lib/gpu2reg.o   "$T/o-gpu2reg.o"
objcopy --redefine-syms="$T/ren-o" orig/lib/drawprim.o  "$T/o-drawprim.o"
objcopy --redefine-syms="$T/ren-n" "$T/gpu2reg-new.o"   "$T/n-gpu2reg.o"
objcopy --redefine-syms="$T/ren-n" "$T/drawprim-new.o"  "$T/n-drawprim.o"

clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector \
    -c test/diff_gpu2reg.c -o "$T/maingpu2reg.o"

ld -m elf_i386 -o "$T/diff_gpu2reg" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/maingpu2reg.o" \
    "$T/o-gpu2reg.o" "$T/o-drawprim.o" \
    "$T/n-gpu2reg.o" "$T/n-drawprim.o" \
    -L/usr/lib32 -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_gpu2reg" "$@"
