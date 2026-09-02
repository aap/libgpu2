# param.o

**4029/5571 .text bytes; 12 of the 14 functions reproduce exactly.**
`param::param`, `operator=`, `SetXY`, `IfMinus`, `GetAbs`, `operator+`,
`operator-`, `operator*(param)`, `operator*(int)`, `operator/`,
`operator<<(int)` and `operator>>` are byte-identical apart from the
2-byte intra-section call displacement they all carry (`call __5param`,
`call abs`, `call __divdi3`), which only differs because the two
unmatched functions have the wrong size and shift everything after them.
`ShiftARGBSlope` and `operator<<(const int&)` are short.
No .data/.bss/.rodata; relocations identical apart from those two.

Differential test: `test/run_param.sh` — 4 200 001 calls exercising all
14 entry points on random 80-byte states: **0 mismatches**.  (The test
supplies its own `__divdi3` so it needs no libgcc.)

## What param is

`sizeof(param) == 0x50` — two ints and nine 64-bit fixed-point values,
one per interpolated GS vertex attribute:

```
+0x00 int x        +0x04 int y
+0x08 z   +0x10 r   +0x18 g   +0x20 b   +0x28 a
+0x30 f   +0x38 s   +0x40 t   +0x48 q
```

The names are inferred: the attribute count and `ShiftARGBSlope()`
scaling exactly `+0x10..+0x30` (five fields) pin the r,g,b,a,f group,
and XYZ/RGBA/F/STQ is the GS attribute set.  Two independent statement
orders in the object corroborate the grouping:

- shifts run **z, a, f, r, g, b, s, t, q**
- arithmetic runs **z, f, r, g, b, a, s, t, q**
- the ctor, `operator=`, `IfMinus` and `GetAbs` run in offset order.

`operator=` copies everything **except x and y**; `SetXY` is the
separate copy for those two.  `IfMinus(p)` writes `p.field = (field >= 0
? 1 : -1)` for all nine (a sign vector); `GetAbs()` does
`field = field > 0 ? field : -field` in place.

**`ShiftARGBSlope(n)` has an unconditional extra `>> 2` tail** on all
five fields, on top of the requested shift:

```c
if (n >= 0) { r >>= n; g >>= n; b >>= n; a >>= n; f >>= n; }
else { n = abs(n); r <<= n; g <<= n; b <<= n; a <<= n; f <<= n; }
r >>= 2; g >>= 2; b >>= 2; a >>= 2; f >>= 2;
```

This was missed on the first pass and found only by the differential
test (every result was off by exactly two bits).  The slopes it scales
are per-2x2-stamp; the `>>2` converts them to per-pixel.  Anyone reading
`PCalc` should not assume `ShiftARGBSlope(0)` is a no-op.

The free operators all default-construct a local `param d`, fill it and
return it by value (hidden return-slot pointer as argument 0, final
`rep movsl` of 0x14 dwords).  `operator/(param, int)` promotes the
divisor with `cltd` and calls `__divdi3` nine times.

## Source shapes pinned by the bytes

1. `IfMinus` is `p.f = f >= 0 ? 1 : -1;` — the `< 0 ? -1 : 1` spelling
   puts `1` in the fall-through and inverts the branch.
2. The three shift operators need the shifted value in a **named
   `long long v`** before the store (`v = p.z << n; d.z = v;`); writing
   `d.z = p.z << n;` is 12 bytes shorter per field.
3. `operator*(param, param)` is `p.@ * o.@` and `operator*(param, int)`
   is `p.@ * n` (the int is `cltd`-promoted and lands in `%eax`).
4. The per-function field orders above are not interchangeable.

## Residuals

**`ShiftARGBSlope`** — 0x20a vs 0x2c5.  The original spills every one of
the 15 shift results to its own stack slot and copies it to the member
(`mov %ebx,-0x8(%ebp); ...; mov -0x8(%ebp),%esi; mov %esi,0x10(%edi)`),
frame 0x7c = 15*8 + 4.  Ours stores straight from the register.  15
distinct slots means 15 distinct pseudos, i.e. plain `r = r >> n;`
statements — a single named temp gives one reused slot (0x2b2, right
shape wrong slots), and 15 named temps or block-scoped temps are
coalesced away entirely (0x1ab/0x20a).  So the source is the plain form
and the original's compiler simply spilled where ours does not.

**`operator<<(const param&, const int&)`** — 0x2bd vs 0x333.  Two
differences: the original loads `*n` **once** and keeps it in a register
across all nine fields (ours re-derefs `0x10(%ebp)` each time), and it
spills more of the loaded operands.  Adding an `int m = n;` copy gets
the single load but puts it *before* the first `p.z` load instead of
after (0x2da, 171 difflines) — not obviously better, so the plain form
is kept.

Both are allocation/spill differences; behaviour is verified identical.

## Load-bearing for PCalc

- Six `param` subobjects live in `PCalc` at +0x14/+0x64/+0xb4/+0x10c/
  +0x15c/+0x1ac (0x50 spacing, confirming sizeof), each constructed by an
  out-of-line `__5param` call.
- `operator=` **does not copy x/y** — PCalc must be calling `SetXY`
  separately wherever it wants the position to travel with the values.
- `ShiftARGBSlope`'s hidden `>>2`.
- `operator/` is the only user of `__divdi3` in this cluster.
