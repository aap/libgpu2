#!/bin/sh
# Differential test for src/pcrtc.c against the 1998 object.
#
# Both pcrtc.o's are linked into one program with every defined global and
# weak symbol renamed (o_* / n_*), so neither side can borrow the other's
# code - which matters here because most of pcrtc.o's functions are weak
# out-of-line copies of header inlines and the linker would otherwise keep
# only the first of each.
#
# xif.c includes Xlib.h, so pcrtc.c needs -idirafter for the host's X11
# headers exactly as tools/build.sh does for xif.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

CC272="tools/gcc272/g++272"
CC="clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector"

GCC272_1998=1 $CC272 -O -idirafter /usr/include -Iinclude \
	-c src/pcrtc.c -o "$T/pcrtc-new.o"

# Rename every defined global/weak symbol.  The entry points we call get
# short names; everything else just gets a prefix so the two copies cannot
# collide.
mkrenames() {
	p=$1 obj=$2
	nm --defined-only "$obj" |
	awk -v p="$p" '$2 ~ /^[TWRDVB]$/ {
		n = $3
		if (n == "__8PCRTCxifP6MemoryPciiPFiiPCUi_v") n2 = p "_ctor"
		else if (n == "SetRegister__8PCRTCxifix")     n2 = p "_setreg"
		else if (n == "Resize__8PCRTCxifii")          n2 = p "_resize"
		else                                          n2 = p "_" n
		print n, n2
	}'
}
mkrenames o orig/lib/pcrtc.o > "$T/ren-op"
mkrenames n "$T/pcrtc-new.o" > "$T/ren-np"

objcopy --redefine-syms="$T/ren-op" orig/lib/pcrtc.o "$T/o-pcrtc.o"
objcopy --redefine-syms="$T/ren-np" "$T/pcrtc-new.o" "$T/n-pcrtc.o"

$CC -c test/diff_pcrtc.c -o "$T/mainpcrtc.o"
$CC -c tools/shims.c -o "$T/shimspcrtc.o"

ld -m elf_i386 -o "$T/diff_pcrtc" \
    --dynamic-linker=/lib/ld-linux.so.2 \
    /usr/lib32/crt1.o /usr/lib32/crti.o \
    "$T/mainpcrtc.o" "$T/o-pcrtc.o" "$T/n-pcrtc.o" \
    orig/lib/addrconv.o orig/lib/memory.o \
    "$T/shimspcrtc.o" \
    -L/usr/lib32 -lm -lc -l:libc_nonshared.a \
    /usr/lib32/crtn.o

exec "$T/diff_pcrtc" "$@"
