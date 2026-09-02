#!/bin/sh
# fetch.sh -- rebuild tools/gcc272/root/ from scratch.
#
# Downloads the 1998 Debian 2.0 "hamm" packages that carry GNU C/C++ 2.7.2.3
# and unpacks them into ./root/.  Needs no root privileges and no chroot: the
# extracted i386 ELF binaries run directly against the host's 32-bit glibc.
#
# Usage:  sh tools/gcc272/fetch.sh
#
# Afterwards ./g++272 and ./gcc272 work.  ./patched/ (the reconstructed
# 1998-codegen cc1plus) is rebuilt separately by build-patched-cc1plus.sh.

set -e
here=$(cd -- "$(dirname -- "$0")" && pwd -P)
cd "$here"

MIRROR=http://archive.debian.org/debian-archive/debian

DEBS="
dists/hamm/main/binary-i386/devel/g++272_2.7.2.3-4.8.deb
dists/hamm/main/binary-i386/devel/gcc_2.7.2.3-4.8.deb
dists/hamm/main/binary-i386/interpreters/cpp_2.7.2.3-4.8.deb
dists/hamm/main/binary-i386/base/libc6_2.0.7t-1.deb
dists/hamm/main/binary-i386/devel/libc6-dev_2.0.7t-1.deb
dists/hamm/main/binary-i386/libs/libg++272_2.7.2.8-0.1.deb
dists/hamm/main/binary-i386/devel/libg++272-dev_2.7.2.8-0.1.deb
dists/hamm/main/binary-i386/devel/binutils_2.9.1-0.2.deb
"
SRCS="
dists/hamm/main/source/devel/gcc_2.7.2.3-4.8.diff.gz
"

mkdir -p debs src root shim
for f in $DEBS; do
    b=$(basename "$f")
    [ -f "debs/$b" ] || curl -fsS -o "debs/$b" "$MIRROR/$f"
done
for f in $SRCS; do
    b=$(basename "$f")
    [ -f "src/$b" ] || curl -fsS -o "src/$b" "$MIRROR/$f"
done
# vanilla FSF source, only needed for build-patched-cc1plus.sh
[ -f src/gcc-2.7.2.3.tar.gz ] || \
    curl -fsS -o src/gcc-2.7.2.3.tar.gz https://ftp.gnu.org/old-gnu/gcc/gcc-2.7.2.3.tar.gz

[ -f SHA256SUMS ] && ( cd debs && sha256sum -c --ignore-missing ../SHA256SUMS >/dev/null \
    && echo "checksums ok" ) || true

# --- unpack.  A hamm .deb is an ar archive of debian-binary/control.tar.gz/
# --- data.tar.gz, so plain `ar' + `tar' is enough; no dpkg needed.
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' 0
for d in debs/*.deb; do
    rm -rf "$tmp/x"; mkdir -p "$tmp/x"
    ( cd "$tmp/x" && ar x "$here/$d" )
    case $d in
    *binutils*)   # only the few programs we actually drive
        tar xzf "$tmp/x/data.tar.gz" -C root \
            usr/bin/as usr/bin/ld usr/bin/ar usr/bin/ranlib usr/bin/nm \
            usr/bin/objdump usr/bin/objcopy usr/bin/strip usr/bin/size \
            usr/lib/ldscripts \
            usr/lib/libbfd-2.9.1.so.0 usr/lib/libbfd-2.9.1.so.0.0.0 \
            usr/lib/libopcodes-2.9.1.so.0 usr/lib/libopcodes-2.9.1.so.0.0.0
        ;;
    *)  tar xzf "$tmp/x/data.tar.gz" -C root ;;
    esac
done

# The era libc.so is an ld script naming absolute /lib paths, which on a
# 64-bit host resolve to the host's 64-bit libc.  Point it at our own tree.
cat > root/usr/lib/libc.so <<EOF
/* GNU ld script -- rewritten by tools/gcc272/fetch.sh to point at the
   extracted 1998 glibc 2.0.7 instead of the host's 64-bit /lib.  */
GROUP ( $here/root/lib/libc.so.6 $here/root/usr/lib/libc_nonshared.a )
EOF

echo "tools/gcc272/root/ ready."
"$here/root/usr/lib/gcc-lib/i486-linux/2.7.2.3/cc1plus" --version 2>&1 | head -4
