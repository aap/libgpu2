#!/bin/sh
# fetch-alt.sh -- re-create tools/gcc272/alt/ (other vendors' gcc 2.7.2.x).
#
# Downloads Red Hat 4.2 and 5.0 gcc RPMs, unpacks them without rpm/rpm2cpio
# (a 30-line Python RPM reader is inlined below), and rebuilds Red Hat 4.2's
# gcc 2.7.2.1 from its source RPM using the era compiler as $(CC).
#
# Usage:  sh tools/gcc272/fetch-alt.sh
# Needs:  ./root/ populated (run fetch.sh first), python3, cpio, curl.

set -e
here=$(cd -- "$(dirname -- "$0")" && pwd -P)
cd "$here"

MIRROR=http://ftp.icm.edu.pl/packages/linux-redhat/linux

RPMS="
5.0/en/os/i386/RedHat/RPMS/gcc-2.7.2.3-8.i386.rpm
5.0/en/os/i386/RedHat/RPMS/gcc-c++-2.7.2.3-8.i386.rpm
4.2/en/os/i386/RedHat/RPMS/gcc-2.7.2.1-2.i386.rpm
4.2/en/os/i386/RedHat/RPMS/gcc-c++-2.7.2.1-2.i386.rpm
4.2/en/os/i386/SRPMS/gcc-2.7.2.1-2.src.rpm
5.0/en/os/i386/SRPMS/gcc-2.7.2.3-8.src.rpm
updates/4.2/en/os/i386/libc-5.3.12-18.5.i386.rpm
updates/4.2/en/os/i386/ld.so-1.7.14-5.i386.rpm
"

mkdir -p rpms alt
for f in $RPMS; do
    b=$(basename "$f")
    [ -f "rpms/$b" ] || curl -fsS -o "rpms/$b" "$MIRROR/$f"
done

cat > rpms/rpm2cpio.py <<'PY'
import sys, gzip, struct
data = open(sys.argv[1], 'rb').read()
def hdr(off):
    assert data[off:off+3] == b'\x8e\xad\xe8', (off, data[off:off+4])
    nidx, dlen = struct.unpack('>II', data[off+8:off+16])
    return off + 16 + nidx*16 + dlen
off = hdr(96)              # signature header
off = (off + 7) & ~7       # 8-byte aligned
off = hdr(off)             # main header
sys.stdout.buffer.write(gzip.decompress(data[off:]))
PY

unpack() {   # unpack <destdir> <rpm>...
    d=$1; shift; mkdir -p "$d"
    for r in "$@"; do
        ( cd "$d" && python3 "$here/rpms/rpm2cpio.py" "$here/rpms/$r" | cpio -idmu --quiet )
    done
}

unpack alt/rh50/root  gcc-2.7.2.3-8.i386.rpm gcc-c++-2.7.2.3-8.i386.rpm
unpack alt/rh42/root  gcc-2.7.2.1-2.i386.rpm gcc-c++-2.7.2.1-2.i386.rpm
unpack alt/rh42/libc5 libc-5.3.12-18.5.i386.rpm ld.so-1.7.14-5.i386.rpm

# --- build a runnable gcc 2.7.2.1 from Red Hat 4.2's source RPM -----------
# (the shipped 4.2 binaries are libc5 and do not run on a modern kernel)
b=$here/build-alt
rm -rf "$b"; mkdir -p "$b/src"
( cd "$b/src" && python3 "$here/rpms/rpm2cpio.py" "$here/rpms/gcc-2.7.2.1-2.src.rpm" | cpio -idmu --quiet )
tar xzf "$b/src/gcc-2.7.2.1.tar.gz" -C "$b"
cd "$b/gcc-2.7.2.1"
chmod -R u+w .
gzip -dc "$b/src/rth-gcc-2.7.2-960814.diff.gz" | patch -p1 --forward >/dev/null 2>&1 || true
patch -p1 --forward < "$b/src/gcc-2.7.2-flow.patch" >/dev/null 2>&1 || true
CC=$here/gcc272; export CC
./configure --host=i386-linux --target=i386-linux --prefix="$b/inst" > configure.log 2>&1
GCC272_HOST_INTERP=1 make CC="$CC" HOST_CC="$CC" CFLAGS="-O2" LANGUAGES="c c++" cc1plus

# --- install both as GCC272_ALT targets -----------------------------------
install_alt() {  # install_alt <name> <cc1plus>
    n=$1; src=$2; e=$here/alt/$n/exec/i486-linux/2.7.2.3
    mkdir -p "$e"; cp "$src" "$e/cc1plus"; strip "$e/cc1plus" || true
    for f in "$here"/root/usr/lib/gcc-lib/i486-linux/2.7.2.3/*; do
        bn=$(basename "$f"); [ "$bn" = cc1plus ] && continue
        ln -sf "../../../../../root/usr/lib/gcc-lib/i486-linux/2.7.2.3/$bn" "$e/$bn"
    done
    echo "installed alt/$n"
}
install_alt rh50      "$here/alt/rh50/root/usr/lib/gcc-lib/i386-redhat-linux/2.7.2.3/cc1plus"
install_alt rh42-2721 "$b/gcc-2.7.2.1/cc1plus"

echo "done.  try:  GCC272_ALT=rh50 $here/g++272 -O -c foo.c -o foo.o"
