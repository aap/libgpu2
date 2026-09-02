# texfunc.o — `TexFunc::Func`

**Not byte-matched.**  One function, 762 bytes in the original, 826 in
ours; the *instruction shape* is reproduced (the clamp idiom, the branch
structure, the alignment-padded labels, the cross-jumped tail) but the
register allocation differs and costs one spill.  .rodata is identical
(the error string); relocations identical.

Differential test: `test/run_texfunc.sh` — 3 676 128 calls covering all
four TFX modes x TCC, the full 0..255 component range and out-of-range /
negative / full-32-bit inputs: **0 mismatches**.  The reconstruction is
semantically exact.

## What TexFunc is

`Func(PixColor &t, PixColor &f)` applies texture colour `t` to fragment
colour `f` **in place** (the second argument is the destination).
`PixColor` is four ints `r,g,b,a` at 0x00/0x04/0x08/0x0c.  `TexFunc`
uses only two of its own fields: `+0x00 func` (TEX0.TFX) and
`+0x0c tcc` (TEX0.TCC); +0x04/+0x08 are untouched here.

| func | RGB | A |
|---|---|---|
| 0 MODULATE | `clamp(t*f >> 7)` | `clamp(t.a*f.a >> 7)` if tcc, else `f.a` |
| 1 DECAL | `t` | `t.a` if tcc, else `f.a` |
| 2 HIGHLIGHT | `clamp((t*f >> 7) + f.a)` | `clamp(t.a + f.a)` if tcc, else `f.a` |
| 3 HIGHLIGHT2 | `clamp((t*f >> 7) + f.a)` | `t.a` if tcc, else `f.a` |
| other | `fprintf(stderr, "TXM: Illegal Texture function\n"); exit(0);` |

Note `exit(0)`, not `exit(1)` — unlike addrconv's error path.
0x80 is 1.0 in the multiply (`>> 7`), and every result is clamped to
0..255 *including* the negative side, so negative texture or fragment
components clamp to 0 rather than wrapping.

## Source shape pinned by the bytes

The clamp is

```c
x = <expr>;
if (x < 0)
        v = 0;
else {
        v = x;
        if (v > 255)
                v = 255;
}
```

with `x` a shared `int` temporary.  This is what makes the 0-arm a
separate basic block ending in a jump, which gcc then *threads* past the
`> 255` test (it knows 0 <= 255); the label of the positive arm is
therefore preceded by a barrier and gcc emits `.align 16,0x90` before it
— those are the runs of `0x90` bytes inside the original function, and
they only appear with this spelling.  The four alternatives all lose them:

| spelling | size |
|---|---|
| `x = e; v = x<0?0:x; if (v>255) v=255;` | 666 (temp coalesced away) |
| `v = e; if (v<0) v=0; if (v>255) v=255;` | 666 |
| `v = e<0?0:e; if (v>255) v=255;` (dup, CSEd) | 906 |
| `x = e; if (x<0) v=0; else { v=x; if (v>255) v=255; }` | **826, correct shape** |

Also pinned: `t.r * f.r` (texture operand first, it goes in `%eax`), the
`if (tcc)` vs `if (tcc == 0)` spelling per case (case 0 and 2 use
`if (tcc)`, cases 1 and 3 use `if (tcc == 0)` — the fall-through arm
differs), and the alpha result going through a variable stored *after*
the branch (gcc cross-jumps the three `a = f.a` arms into the single
`mov 0xc(%esi),%eax` at the tail that all cases share).

## Residual and best explanation

The original allocates six pseudos to six hard registers:
`edi=this, ebx=t, esi=f, ecx=r, edx=g, eax=b` — with the expression temp
`x` sharing `eax` with `b` (that is why the blue clamp has no
`mov %eax,%ecx` copy while red and green do).  Our build instead assigns
`edi=t, esi=f, ebx=r, ecx=g`, leaves `b` in a 4-byte stack slot and
reloads `this` from `0x8(%ebp)` into `edx` at each `tcc` test.  The extra
spill and the four reloads are the whole 64-byte difference.

Nothing source-level moved it: declaration order, `register` hints,
reusing `b` for the alpha, dropping the temp, and duplicating the
expression instead of using a temp were all tried.  It is a
global-register-allocation priority difference (the original ranks
`this` above `r`), of the same family as the addrconv residuals — except
here it costs size rather than a couple of bytes.

## Load-bearing for TXM

`PixColor` is shared with TXM and MemIF; the definition here (4 ints,
16 bytes) should be factored out when txm.o lands.  TXM must set
`TexFunc::func`/`tcc` from TEX0 before calling `Func`, and must not rely
on the illegal-TFX path returning (it `exit(0)`s).
