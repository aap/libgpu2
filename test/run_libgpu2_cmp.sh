#!/bin/sh
# test/run_libgpu2_cmp.sh <object> [-d] [-f <func>]
#
# Compare a freshly built libgpu2 object against Sony's 1998 orig/lib/libgpu2.o:
# section sizes, .rodata, .bss layout, relocations, and per-function .text
# bytes (matched by symbol name, so upstream size drift does not smear the
# report).  -d appends a whole-object disassembly diff, -f <func> a side-by-side
# disassembly of one function.
set -e
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}/lg2test
mkdir -p "$T"

NEW=$1; shift
OLD=orig/lib/libgpu2.o

echo "=== sections ==="
for s in .text .data .bss .rodata; do
	o=$(objdump -h "$OLD" | awk -v s=$s '$2==s{print $3}')
	n=$(objdump -h "$NEW" | awk -v s=$s '$2==s{print $3}')
	printf '%-10s orig=%s  new=%s%s\n' "$s" "${o:-none}" "${n:-none}" \
	    "$([ "$o" = "$n" ] || echo '   <-- DIFFERS')"
done

echo
echo "=== .comment ==="
objdump -s -j .comment "$OLD" | tail -n +5
objdump -s -j .comment "$NEW" | tail -n +5

echo
echo "=== .rodata ==="
objdump -s -j .rodata "$OLD" | tail -n +5 > "$T/ro.old"
objdump -s -j .rodata "$NEW" | tail -n +5 > "$T/ro.new"
diff "$T/ro.old" "$T/ro.new" >/dev/null && echo "identical" || diff "$T/ro.old" "$T/ro.new"

echo
echo "=== .bss symbols ==="
objdump -t "$OLD" | grep '\.bss' | grep -v ' d ' | sort > "$T/bss.old"
objdump -t "$NEW" | grep '\.bss' | grep -v ' d ' | sort > "$T/bss.new"
diff "$T/bss.old" "$T/bss.new" >/dev/null && echo "identical" || diff "$T/bss.old" "$T/bss.new"

echo
echo "=== relocations (type + target, in order) ==="
objdump -r "$OLD" | grep R_386 | awk '{print $2, $3}' > "$T/rel.old"
objdump -r "$NEW" | grep R_386 | awk '{print $2, $3}' > "$T/rel.new"
if diff "$T/rel.old" "$T/rel.new" >/dev/null; then
	echo "identical ($(wc -l < "$T/rel.old") records)"
else
	diff "$T/rel.old" "$T/rel.new" | head -40
fi

echo
echo "=== .text bytes, per function ==="
objcopy -O binary -j .text "$OLD" "$T/text.old"
objcopy -O binary -j .text "$NEW" "$T/text.new"
python3 - "$T/text.old" "$T/text.new" "$OLD" "$NEW" <<'EOF'
import sys, subprocess
a = open(sys.argv[1],'rb').read(); b = open(sys.argv[2],'rb').read()

def funcs(obj):
    d = {}
    for l in subprocess.run(['objdump','-t',obj],capture_output=True,
                            text=True).stdout.splitlines():
        f = l.split()
        if len(f) >= 6 and f[3] == '.text' and 'F' in l[:30]:
            d[f[5]] = (int(f[0],16), int(f[4],16))
    return d

fo, fn = funcs(sys.argv[3]), funcs(sys.argv[4])
print("orig .text %d bytes, new .text %d bytes (incl. inter-function padding)"
      % (len(a), len(b)))
tot = res = 0
for name, (off, size) in sorted(fo.items(), key=lambda kv: kv[1][0]):
    if name not in fn:
        print("  %-22s MISSING in new" % name); continue
    noff, nsize = fn[name]
    x, y = a[off:off+size], b[noff:noff+nsize]
    tot += size
    if x == y:
        print("  %-22s %4d bytes  MATCH" % (name, size))
    else:
        n = min(len(x), len(y))
        d = sum(1 for i in range(n) if x[i] != y[i]) + abs(len(x)-len(y))
        res += d
        print("  %-22s %4d bytes  %d differ (new %d bytes)" %
              (name, size, d, nsize))
print("  %-22s %4d bytes  %d residual" % ("TOTAL", tot, res))
EOF

# strip addresses and branch targets so only the instruction stream is compared
NORM='s/^ *[0-9a-f]*:\t[0-9a-f ]*\t/  /;s/\(call\|j[a-z]*\) *[0-9a-f]* <[^>]*>/\1 ./;s/^\t*[0-9a-f]*: //'

while [ $# -gt 0 ]; do
	case $1 in
	-d)	echo; echo "=== disassembly diff ==="
		objdump -d -r "$OLD" | sed "$NORM" > "$T/d.old"
		objdump -d -r "$NEW" | sed "$NORM" > "$T/d.new"
		diff -u "$T/d.old" "$T/d.new" || true ;;
	-f)	shift; f=$1
		echo; echo "=== $f (orig | new) ==="
		objdump -d -r "$OLD" | sed -n "/<$f>:/,/^\$/p" | sed "$NORM" > "$T/f.old"
		objdump -d -r "$NEW" | sed -n "/<$f>:/,/^\$/p" | sed "$NORM" > "$T/f.new"
		diff -y -W 96 "$T/f.old" "$T/f.new" || true ;;
	esac
	shift
done
