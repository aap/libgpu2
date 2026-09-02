# div.o — `Reciproc`

**2879/2889 .text bytes (99.65%)**, .rodata identical (4 doubles),
relocations identical, symbol table identical.
The 10 residual bytes are all in the constructor (frame size + two
spill-slot displacements); `Reciproc::reciproc` is **byte-identical**.

Differential test: `test/run_div.sh` — both constructors run, all
**512 table entries compare identical**, then 13 048 963 `reciproc()`
calls over every power of two ±1, the whole low 20-bit range and random
64-bit inputs: 0 mismatches.

## What Reciproc is

`sizeof(Reciproc) == 8`: two `long long *` tables of 256 entries each,
allocated with `new long long[256]` (`__builtin_vec_new(0x800)`).

The constructor builds a piecewise-linear approximation of `1/v` over
`v = 1 + (i+0.5)/256`:

```
x = (i + 0.5)/256.0;
y = (1.0/(1.0+x) - 0.5) * 256.0;        a = (long long)(y * 2^28);
z = 1.0/((1.0+x)*(1.0+x)) * 256.0;      b = (long long)(z * 2^17);
a += 9;  b += 1;                        /* fixed bias */
if (i == N) { a += 2; b -= 2; }         /* 70 hand-tuned entries */
tbl0[i] = (a + 1) >> 1;                 /* ~ (1/v - 0.5) * 2^35 */
tbl1[i] = (b + 1) >> 1;                 /* ~ 2^24 / v^2        */
```

The 70 correction indices are
0 2 3 4 6 7 13 15 16 17 18 22 26 28 30 31 33 34 35 36 39 42 45 51 53 55
56 61 64 67 68 84 86 87 89 90 91 95 96 109 112 116 118 124 130 134 136
141 157 161 167 170 175 176 183 184 186 193 194 204 210 213 217 228 236
239 241 242 250 255 — each written as its own `if (i == N) { a += 2;
b -= 2; }` (the object has 70 separate compare/add blocks, not a chain).
These are almost certainly per-entry tweaks that make the model's table
agree bit-for-bit with the real GS ROM table.

`reciproc(x, sft, v0, v1, rem)`:

- `a = |x|`; scan down from bit 63 with a *sign-propagating* mask
  (`mask = 0x8000000000000000LL; mask >>= 1`) to find the top set bit `n`.
- `n < 0` (x == 0): all three outputs zeroed, return.
- normalise the mantissa to 31 bits:
  `t = (a & ((1<<n)-1)) << (31-n)` for n <= 31, `>> (n-31)` otherwise.
- exact power of two (`(t & 0x7fffffff) == 0`): `v0 = v1 = 0x80000000`,
  `sft = 1 - n`, return.
- otherwise index the tables with `(t >> 23) & 0xff`, reconstruct
  `y = tbl0[idx] + 0x400000000` (= 2^35/v), interpolate linearly against
  the *interval midpoint* (`frac` is the distance from 0x400000, and the
  below-midpoint arm adds `~(prod>>20)` plus a +1 when the product is an
  exact multiple of 2^20 — i.e. it subtracts `ceil`, not `floor`), then
  apply one more second-order correction term.
- outputs: `v0` = unrefined `y >> 3` (1/x in Q32), `v1` = refined,
  `sft` = `-n`, `rem` = the low 32 bits of the normalised mantissa `t`.

## Source shapes pinned by the bytes

- `i` is `unsigned int` (`fildll` of a zero-extended value; `jbe` on the
  loop test).
- The ctor computes two *named* intermediate doubles `y` and `z` and only
  then does the two `(long long)` conversions — writing the whole thing
  as two casts of one expression each interleaves the x87 differently.
- In `reciproc`, ten locals in the order `a, t, y, s, frac, flag, r0, r1,
  idx, u, mask` reproduce the spill-slot layout exactly, and:
  - `m = n;` after the mantissa `if/else` (a redundant-looking copy that
    the original really has — it is the `mov -0x58,%edx; mov %edx,-0x58`
    self-copy, and it is what puts `n` in `%edx` for `1 - m`);
  - `idx = (t >> 23) & 0xff;` — the `& 0xff` spelling, not
    `(unsigned char)(...)`, which costs 24 extra bytes;
  - every 64-bit product is written **slope-first**:
    `s = (s & 0xffffff) * (frac & 0x7fffff);` — the reverse spelling
    swaps `%eax`'s operand;
  - `u = y >> 16;` must be its own statement before the last product.

## Residual

Constructor, 10 bytes: the original puts the two `(x+1)>>1` DImode
results in one *reused* stack temp at -0x14 (allocated before `a`), ours
gets two reload spill slots at -0x24/-0x2c below `a`, and the frame grows
0x1c -> 0x2c.  Every instruction is otherwise identical, including the
whole x87 sequence, all 70 correction blocks and the .rodata pool.  Both
the stock Debian cc1plus and the `GCC272_1998=1` patched one produce the
same thing, and no declaration order, store spelling or index form
changed it.

## Open questions

- Table field names (`tbl0`/`tbl1`) are invented.
- Why the correction list exists at all — presumably measured against
  hardware; worth diffing our generated table against a PS2 dump.
