#!/bin/sh
# build-patched-cc1plus.sh -- rebuild ./patched/i486-linux/2.7.2.3/cc1plus
#
# Builds a vanilla FSF gcc-2.7.2.3 cc1plus, with patches/*.patch applied,
# USING the extracted 1998 gcc 2.7.2.3 C compiler as $(CC).  That is why no
# K&R-era source patching is needed: the era compiler eats its own source.
#
# Usage:  sh tools/gcc272/build-patched-cc1plus.sh [builddir]
# Needs:  ./root/ populated (run fetch.sh first) and src/gcc-2.7.2.3.tar.gz.

set -e
here=$(cd -- "$(dirname -- "$0")" && pwd -P)
build=${1:-$here/build}

[ -x "$here/gcc272" ] || { echo "run fetch.sh first" >&2; exit 1; }

rm -rf "$build"
mkdir -p "$build"
tar xzf "$here/src/gcc-2.7.2.3.tar.gz" -C "$build"
cd "$build/gcc-2.7.2.3"
chmod -R u+w .

for p in "$here"/patches/*.patch; do
    [ -e "$p" ] || continue
    echo "applying $(basename "$p")"
    patch -p1 < "$p"
done

CC=$here/gcc272; export CC
./configure --host=i386-pc-linux-gnu --target=i386-pc-linux-gnu \
            --prefix="$build/install" > configure.log 2>&1

# GCC272_HOST_INTERP=1 so the result records /lib/ld-linux.so.2 and stays
# relocatable, exactly like the 1998 Debian binaries do on this host.
GCC272_HOST_INTERP=1 \
make CC="$CC" HOST_CC="$CC" CFLAGS="-O2" LANGUAGES="c c++" cc1plus

mkdir -p "$here/patched/i486-linux/2.7.2.3"
cp cc1plus "$here/patched/i486-linux/2.7.2.3/cc1plus"
strip "$here/patched/i486-linux/2.7.2.3/cc1plus" || true

# the rest of the gcc-lib directory (cpp, specs, includes) is shared
for f in "$here"/root/usr/lib/gcc-lib/i486-linux/2.7.2.3/*; do
    b=$(basename "$f")
    [ "$b" = cc1plus ] && continue
    ln -sf "../../../root/usr/lib/gcc-lib/i486-linux/2.7.2.3/$b" \
           "$here/patched/i486-linux/2.7.2.3/$b"
done

echo "patched cc1plus installed; use with GCC272_1998=1 ./g++272 -O ..."
