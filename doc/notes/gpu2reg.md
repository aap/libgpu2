# gpu2reg.o - the jtcl register console

Sony's interactive front end to the GS model: 82 Tcl-style commands, one
per GS register name, plus image upload/save utilities.  This is the
seed of the planned interactive debugger - it is the one place in the
archive where every register of the model can be poked *by name* from a
console, where local memory can be read back a quadword at a time, and
where the displayed CRT image can be captured pixel by pixel.

Reconstruction: src/gpu2reg.c, include/gpu2reg.h.  Not linked by
gsreplay or any replay path; verified by test/run_gpu2reg.sh (8.17M
records over 20000 randomised rounds against the 1998 object, 0
mismatches) and by hybrid-oracle non-perturbation (probe 0 failures,
r614 / o519 / RRV md5s unchanged).

## The GPU2Reg seam

`GPU2Reg` is an **abstract interface**: three pure virtuals (the local
`_vt.7GPU2Reg` carries three `__pure_virtual` entries), in declaration
order

    vt+0x08  void Put(int addr, long long data);
    vt+0x10  long long Get(void);
    vt+0x18  int GetCRT(void);

Neither gpu2.o nor gpu2vec.o emits a GPU2Reg vtable or inherits from it:
the concrete subclass lived in the **host tool** (the gpu2/gpu2vec
application wrapped its GPU2/GPU2VEC instance in a GPU2Reg adapter).
Constructing that adapter runs `GPU2Reg::GPU2Reg()`, which

1. `RegisterCommands(MyCBFuncs)` - hands the command table to the
   host's jtcl interpreter (one argument, not three as the old
   linkall/stubs.c guess had it);
2. `grfwSwitchTriangleRasterlizer(DrawTriangle)` and
   `grfwSwitchVertex(Vertex0, Vertex1, Vertex2)` - installs drawprim.o's
   vertex-kick callbacks into the host "grfw" framework;
3. `pGPU2Reg = this` - the global every handler and drawprim call
   through.

For a debugger, this is the whole integration contract: implement the
three virtuals over a GPU2 (Put -> GPU2::Put, Get -> GPU2::Get, GetCRT
-> GPU2::GetCRT), construct one GPU2Reg subclass instance, and the
entire console comes alive.

## jtcl records

The interpreter parses each command's arguments per its help string and
passes an array of 8-byte records; handlers read only the value union at
offset +4 (the first word is presumed the type tag):

    typedef struct {
        int type;
        union { int i; float f; char *s; } d;
    } jtcl_data_t;

Handler signature: `int handler(int cnt, char *args, jtcl_data_t *data)`
returning 0 (TCL_OK) or 1 (TCL_ERROR).  Only DISPLAY and SaveCRT look at
`cnt`.  Errors go through `sprintf(tcl_ip->result, ...)` - `tcl_ip`
points at the interpreter and `result` is its first member, exactly as
in real Tcl_Interp.

The command table entry is `{name, handler, help, 0}`; the table ends
with an all-NULL terminator.  Help strings are the argument signatures
("9I", "4IF", "S2I", "(2IS)|(2ISI)", ...); gpucacheinvld and quit have
NULL help, gpuextwrite an empty string.

## Command -> register map

Register handlers mask each argument to its field width, assemble the
64-bit datum in named long long locals, and call `Put(addr, data)`.
The masks/shifts follow the GS field layouts exactly (see src for every
field); the table below is the addr map with the surprises marked.

| commands | addr | note |
|---|---|---|
| gpuprim | 0x00 | 9 args: PRIM..FIX (bits 0-10) |
| gpurgbaq/2 | 0x01/0x11 | Q passed as raw float bits (FtoI) |
| gpust/2 | 0x02/0x12 | both floats as raw bits |
| gpuuv/2 | 0x03/0x13 | |
| gpuxyzf/2/3 | **0x0a**/0x04/0x0c | plain `gpuxyzf` writes 0x0a - the pre-retail non-kick XYZF slot (retail FOG) |
| gpuxyz2/3 | 0x05/0x0d | Z is a full signed 32-bit arg |
| gputex01/02 | 0x06/0x07 | 12 args |
| gpuclamp1/2 | 0x08/0x09 | |
| gpurgbaq2 etc | 0x11-0x13 | the 1998 kick-variant registers (dropped before retail) |
| gputex11/12 | 0x14/0x15 | |
| gputex21/**22** | **0x17/0x17** | **BUG: gputex21 also writes TEX2_2 (0x17); TEX2_1 (0x16) is unreachable** - same family as GS_OpenSim's double-0x17 init |
| gpuxyoffset1/2 | 0x18/0x19 | |
| gpuprmodecont | 0x1a | |
| gpuprmode | 0x1b | 8 args, bits 3-10 |
| gputexclut | 0x1c | |
| gpuscanmsk | 0x22 | |
| gpumiptbp11/12/21/22 | 0x34/0x35/0x36/0x37 | |
| gputexa | 0x3b | arg order TA0, **TA1, AEM** (source order ta0, ta1<<32, aem<<15) |
| gpufogcol | 0x3d | |
| gpucacheinvld | 0x3f | data 0 |
| gpuscissor1/2 | 0x40/0x41 | |
| gpualpha1/2 | 0x42/0x43 | FIX is a full byte |
| gpudimx | 0x44 | 16 args into an `int dm[4][4]` staging array |
| gpudthe/colclamp | 0x45/0x46 | |
| gputest1/2 | 0x47/0x48 | |
| gpupabe | 0x49 | |
| gpufba1/2 | 0x4a/0x4b | |
| gpuframe1/2 | 0x4c/0x4d | FBMSK staged through an `int` local |
| gpuzbuf1/2 | 0x4e/0x4f | |
| gpubitbltbuf | 0x50 | |
| gputrxpos | 0x51 | 5th arg = DIR, bits 59-60 |
| gputrxreg | 0x52 | RRH at bit 32 (the model's TRXREG layout) |
| gputrxdir | 0x53 | |
| gpuhwreg | 0x54 | 2 args: `hi<<32 | lo`, both unsigned |
| gpupmode | 0x80 | 11 args - the 1998 PMODE has NFLD (16) and EXVWINS/EXVWINE/EXSYNCMD (32-52), fields dropped from retail |
| gpusmode1 | 0x81 | 17 args, bits 0-30 |
| gpusmode2 | 0x82 | |
| gpusynch1 | 0x84 | |
| gpusyncv | 0x86 | |
| gpudispfb1/2 | 0x87/0x89 | |
| gpudisplay1/2 | 0x88/0x8a | |
| gpubgcolor | 0x8e | |
| gpuextbuf | 0x8b | 8 args |
| gpuextdata | 0x8c | |
| gpuextwrite | 0x8d | data 0 |
| gpureg | any | raw poke: `gpureg addr hi lo` |
| gpufile | - | replays an `addr hi lo` hex trace file (sample/sample.dat format) |
| quit | - | `exit(0)` |

## The pseudo-registers and the real merge path (0x101)

`GPU2::Put` routes addr < 0 or > 0xff to the virtual
`PCRTC::SetRegister`; pcrtc.o's jump table covers 0x80..0x101.  Two
pseudo-registers exist above the priv-reg range and **gpu2reg.o is the
only writer of both**:

* **0x100 "Display"** - `gpudisplay dn [hmag vmag]`.  Data = circuit
  number (0/1, >1 is fatal) | hmag<<32 | vmag<<48.  Drives the OLD
  unmerged display path (`DispInfo::oldDispPixel[Mag]`).  This is the
  same pseudo-register `GS_PutPort(0x7f)` (vsync) is rewritten to by
  libgpu2.o - but through the documented API only ever the 1-arg form.
* **0x101 "DisplayPcrtc"** - `gpupcrtc dn`.  Data = dn & 1.  This is the
  ONLY path in the entire archive to `DispInfo::DisplayPixel`, the real
  two-circuit PMODE merge (per-pixel RC1/RC2 blend, ALP/MMOD/SLBG,
  EXTBUF write-back).  A future debugger that wants the true merged
  output must send 0x101, not vsync.

Chain: `gpupcrtc 0` -> `pGPU2Reg->Put(0x101, 0)` -> `GPU2::Put` (addr >
0xff) -> virtual `PCRTCxif::SetRegister` -> `DisplayPcrtc(0)` ->
`DispInfo::DisplayPixel(0, mem, xif)`.

## Local memory readback

`savergb24 x y w h file` / `savergba32 x y w h file` program a
Local->Host transfer themselves (Put 0x51 = SSAX/SSAY, 0x52 = RRW |
RRH<<32, 0x53 = TRXDIR 1) and then pull quadwords with the virtual
`Get()` - 8 bytes per call, `(3*w*h+7)/8` calls for RGB24, `w*h/2` for
RGBA32 - into a `new char[]` buffer handed to `SaveImageFile`.
BITBLTBUF is *not* programmed: the user must set `gpubitbltbuf` first.

`savecrt w h file [chan]` instead calls `GetCRT()` per pixel (the
displayed CRT image, after merge); `chan == 1` saves alpha replicated
into RGBA, anything else the full RGBA32 pixel.

## Image upload

`gpurgb24pixel`/`gpurgba32pixel`/`gpurgba16pixel` open a host image file
(`OpenImage` -> `OpenImageFile`; IDX8 files are converted up with
`ConvImage8to24`), program TRXREG (w | h<<32) and TRXDIR 0, and stream
the pixels out via HWREG (0x54) writes - RGB24 packs 24-bit pixels
across quadword boundaries with an 8-case `count % 8` switch, RGBA16
converts 8-bit channels down to 5551 with a `count % 4` packer, IDX8/4
pack 8/16 indices per quadword.  `gpuidtex8pixel`/`gpuidtex4pixel` first
quantise with `ConvImage24to8(img, idx, 256/16, 1)`.
`gpuclutrgba32pixel`/`gpuclutrgba16pixel` upload the resulting palette
instead: for 256-colour CSM1 the entries are shuffled through the
PSMCT32 CLUT block arrangement `e = i/2*32 + i%2*8 + j + (j - j%8)`
(16x16), for 16-colour CSM1 linear 8x2; the 16-bit variant also knows
CSM2 (256x1 / 16x1, third argument non-zero).  The palette is packed
into a temporary RGB24 ImageData (format=1, pixel=clut) and pushed
through the same PutRGBA32/PutRGBA16 engines.

ImageData (host image library record, sizeof 0x224, pinned by the
`*img = tmp` 0x89-dword `rep movsl`):

    0x000 char name[512];
    0x200 int type;        /* always written 2 */
    0x204 int format;      /* 1 RGB24, 2 RGBA32/ARGB, 3 IDX8 */
    0x208 int width;   0x20c int height;
    0x210 int ncolor;      /* untouched here (name guessed) */
    0x214 u8 *r; 0x218 u8 *g; 0x21c u8 *b;   /* palette planes */
    0x220 u8 *pixel;

"CHANNEL_ARGB is not supported yet." is the format==2 rejection in the
indexed/CLUT handlers.

## Original bugs (all reproduced)

1. **gputex21 writes register 0x17** (TEX2_2) - copy-paste; TEX2_1
   unreachable from the console.
2. **SaveRGB24Pixel overruns its buffer**: `new char[w*h*3]` but the
   Get loop rounds up to `(3*w*h+7)/8` whole quadwords and writes up to
   7 bytes past the end (heap corruption caught live by the test
   harness; the harness allocator pads by 8).
3. **SaveRGBA32Pixel saves uninitialised bytes for odd w*h**: the loop
   count `w*h/2` rounds down, so the last 4 bytes of the buffer are
   never written but still saved.
4. **GPU2File never fcloses** its trace file - each `gpufile` leaks a
   FILE (fd exhaustion in long sessions; also surfaced by the test).
5. **All the Save/CLUT handlers free `new char[]` buffers with scalar
   `delete`** (`__builtin_delete`, not `__builtin_vec_delete`) -
   harmless for char arrays under this runtime, but wrong C++.
6. **IDTEX8Pixel is missing its `static`** - the one handler with
   external linkage (its neighbour IDTEX4Pixel is static), plainly an
   oversight; the table is the intended access path.
7. `Quit` never returns (exit(0)) but is declared as an int handler.

## Behavioural notes for the debugger seed

* All handlers return 0 even for nonsense values (masking, never
  validation); only the image/file handlers return 1 (with
  tcl_ip->result set) on open/format failures.
* `gpufile` accepts the `addr hi lo` hex format of sample/sample.dat,
  skipping the rest of each line: `while (fgetc(fp) != '\n');` -
  a file whose last line lacks '\n' loops on EOF forever (untested
  against the original but plainly true from the code; keep traces
  newline-terminated).
* The float path (RGBAQ's Q, ST's S/T) passes IEEE bits through
  unmodified via `*(int *)&f` - except that the second float of a pair
  takes one flds/fstps round trip through the x87 in the original
  codegen, which quietises signalling NaNs.  Bit-exact for everything a
  console user would type.
