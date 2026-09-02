# Stage wiring and object dependencies

Dependency edges below are mechanical (relocations of `orig/lib/*.o`,
2026-09-02); the data-flow arrows are the verified virtual-Put chain
(doc/STRUCTS.md).  Key property: **pre1.o, pre3.o and dda.o have no
outgoing static references at all** — stage-to-stage flow is entirely
virtual dispatch, so the pipeline objects only depend downward on
utility clusters, never on each other.

## Data flow (drawing path)

```
GS_PutPort(a,d)            GS_PutCtlPort(0x120000n0,d)
     │                          │  (maps to 0x80|n)
     ▼                          ▼
   GPU2::Put ──── a ≥ 0x80 ──► PCRTC::SetRegister ─► DispInfo / MemRead16|24|32
     │ a < 0x80                │  (virtual, 2-entry vt)      / PixelBlend* 
     ▼                         ▼                                │
   Pre1::Put            PCRTCdmy | PCRTCxif ──────────► XWindow | XWindowDump
     │ (non-virtual; switch a=0x00..0x1b, default: pass down)
     │ SendData / SendRegister
     ▼ virtual Put(Pre1*)
   Pre3   (primitive assembly: Triangle/Line/Point/Sprite, Float2Fix)
     ▼ virtual Put(Pre3*)
   PCalc  (setup: slopes, bbox, AA coverage, subpixel)──uses──► div param slong
     ▼ virtual Put(PCalc*) THROUGH the always-installed PPOut tap
   DDA    (rasterizer: 2x2 stamps)
     ▼ virtual Put(DDA*)
   TXM    (texture/LOD/filter/fog) ──uses──► clut texfunc txm_div addrconv
     ▼ virtual Put (via TXM+0x24/+0x278 MemIF*)
   MemIF  (ATest/ZTest/Blend/Dither/Clamp) ─► Memory (4MB + FB/ZB config)
                                               │ uses addrconv, bitblt, dbg
GS_SaveImage ─► GPU2::Get ─► BitBLT::ReadPixel(Memory*)
```

Register writes a stage doesn't consume travel down the same Put chain
(send_type=1) and are picked off by each stage's Set* methods
(inference; verify per stage as we decompile).

## Static dependency edges (relocation-resolved)

```
libgpu2  -> gpu2
gpu2     -> addrconv bitblt dda div memif memory param pcalc pcrtc pre1 pre3 txm xif
gpu2vec  -> (same as gpu2)
memif    -> memory
memory   -> addrconv bitblt dbg
pcrtc    -> addrconv memory xif
txm      -> addrconv clut texfunc txm_div
pcalc    -> div param slong
bitblt   -> addrconv
clut     -> addrconv
drawprim <-> gpu2reg   (mutual; grfw rasterizer-swap seam)
pre1, pre3, dda, slong, div, txm_div, texfunc, param, dbg, xif: no deps
```

## Unlock order (leaves first, each line enabled by the ones above)

1. addrconv [DONE] — slong div param txm_div texfunc dbg xif  (all leaf)
2. pre1 pre3 dda      (leaf in the static graph; struct work is the cost)
3. clut bitblt        (need addrconv)
4. memory             (addrconv bitblt dbg)  →  memif
5. pcalc              (div param slong)
6. txm                (addrconv clut texfunc txm_div)
7. pcrtc              (addrconv memory xif)
8. gpu2, libgpu2      (everything)
9. gpu2reg drawprim   (Tcl seam, self-contained pair)
10. gpu2vec           (subclasses of every stage; last)
```
