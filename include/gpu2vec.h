/* gpu2vec - Sony's pipeline-tap / RTL-test-vector layer (gpu2vec.o).
 *
 * GPU2VEC is a parallel top level to GPU2: it assembles the same pipeline
 * from instrumented subclasses (MyPP -> MyDDA -> MyTXM -> MyMemIF ->
 * MyMemory) whose overridden Put/Stamp write one test-vector line per
 * stage event to a per-stage FILE* before forwarding to the real stage
 * code.  See doc/notes/gpu2vec.md for the tap protocol and formats.
 *
 * The 1998 gpu2vec.C saw the same full header stack as gpu2.C - the two
 * objects' .rodata is byte-identical from 0x000 to the end of the
 * "dn == 0 || dn == 1" string, and their weak-inline tails match symbol
 * for symbol.  Our per-object reconstructions of pcalc.h/pre3.h/dda.h/
 * memif.h carry mutually conflicting stand-ins (each declares its own
 * narrow view of its neighbours), so the middle of that stack is CLONED
 * here with the smallest deltas that make one TU of it:
 *
 *   - PPDDA::Put is pure (gpu2vec.o's local _vt.5PPDDA entry is
 *     __pure_virtual, where pcalc.h declares it plain);
 *   - PCalc sits next to the real Pre3 (pre3.h's, with the inline
 *     constructor) instead of the data-only views the two headers carry;
 *   - DDA sits next to the real full PCalc and txm.h's pure DDATXM;
 *   - MemIF's constructor is inline (gpu2vec.o and gpu2.o expand it
 *     into their constructors; neither has an UND __5MemIFP6Memory).
 *
 * memory.h, clut.h, txm_div.h, pre1.h and pcrtc.h (which brings the
 * Xlib `Depth' rename and xif.h with XIF_FILE = "../gpu2u/xif.h") are
 * included as-is.  The main line can dedupe the clones into a shared
 * stack once gpu2.o lands - gpu2.c needs the identical declarations.
 *
 * ORDER IS LOAD BEARING (the same two rules as pcrtc.h): string
 * constants land in .rodata in parse order - bitblt.h's two messages,
 * then TXM::SearchQlevel's assert strings, then Frame2d's, then the
 * display messages, then gpu2vec.c's own - and deferred inlines and
 * local vtables come out in reverse declaration order.  The class order
 * below reproduces gpu2vec.o's .rodata vtable block (_vt.8MyMemory at
 * 0x390 down to _vt.5PPDDA at 0x550) and its weak/global .text tail
 * (0x7a50-0x8afa) exactly.
 */

#ifndef GPU2VEC_H
#define GPU2VEC_H

/* Xlib.h declares `struct Depth', and g++ 2.7 will not have that tag in the
 * same scope as bitblt.h's `int Depth(int)' - in either order.  pcrtc.c is
 * the one translation unit that needs both, so pull Xlib in first with its
 * `Depth' renamed; nothing here uses Screen::depths.  XLIB_ILLEGAL_ACCESS
 * has to be set before the first Xlib.h, and xif.h's own #define is then a
 * harmless repeat of the same empty definition. */
#define XLIB_ILLEGAL_ACCESS
#define Depth XlibDepth
#include <X11/Xlib.h>
#undef Depth

#include "memory.h"

/* ---- PCalc (CLONE of include/pcalc.h: PPDDA pure, real Pre3) ---- */

#include "param.h"
#include "div.h"
#include "slong.h"

class Pre1;
class Pre3;
class PCalc;
class DDA;

/* The next stage after PCalc.  One pure virtual: gpu2vec.o's local
 * _vt.5PPDDA entry is __pure_virtual. */
class PPDDA {
public:
	virtual void Put(PCalc *p) = 0;
};


struct Scissor {		/* 0x10, one per context */
	int scax0;
	int scax1;
	int scay0;
	int scay1;
};

class PCalc {
public:
	PPDDA *out;		/* 0x000  next stage */
	int sft;		/* 0x004  subpixel bits, 4 */
	int msft;		/* 0x008  0x16 */
	int pix;		/* 0x00c  0x10, one pixel in subpixel units */
	int one;		/* 0x010  1 */

	param dx;		/* 0x014  d/dx of every attribute */
	param dy;		/* 0x064  d/dy */
	param sv;		/* 0x0b4  start value */
	Reciproc rcp;		/* 0x104 */
	param A;		/* 0x10c  the three sorted vertices */
	param B;		/* 0x15c */
	param C;		/* 0x1ac */

	char spoint;		/* 0x1fc  vertex the scan starts at */
	char epointy;		/* 0x1fd  vertex supplying the end Y */
	char epointx;		/* 0x1fe  vertex supplying the end X */

	int sx;			/* 0x200  start point, subpixel */
	int sy;			/* 0x204 */
	int sxi;		/* 0x208  start point, pixels */
	int syi;		/* 0x20c */
	int ex;			/* 0x210  end point, subpixel */
	int ey;			/* 0x214 */
	int exi;		/* 0x218  end point, pixels */
	int eyi;		/* 0x21c */
	int m_220;		/* 0x220 */
	int stampw;		/* 0x224  4 or 8 */
	int m_228;		/* 0x228 */
	int m_22c;		/* 0x22c */
	int m_230;		/* 0x230 */
	int m_234;		/* 0x234 */
	int sortcode;		/* 0x238 */
	int m_23c;		/* 0x23c */
	int m_240;		/* 0x240 */
	int m_244;		/* 0x244 */
	int m_248;		/* 0x248 */
	int cov[6];		/* 0x24c  AA coverage slopes, per edge */
	Scissor scissor[2];	/* 0x264 */
	char m_284[0x800];	/* 0x284  never referenced by pcalc.o */

	long long m_a84;	/* 0xa84 */
	long long m_a8c;	/* 0xa8c */
	long long m_a94;	/* 0xa94 */
	long long m_a9c;	/* 0xa9c */
	long long m_aa4;	/* 0xaa4  0xffff */
	long long m_aac;	/* 0xaac  0xffff0000 */
	int m_ab4;		/* 0xab4  0x10 */
	int FIX;		/* 0xab8 */
	int m_abc;		/* 0xabc */
	int m_ac0;		/* 0xac0 */
	int m_ac4;		/* 0xac4 */
	int ddx[3];		/* 0xac8 */
	int ddy[3];		/* 0xad4 */
	int bbl;		/* 0xae0 */
	int bbt;		/* 0xae4 */
	int bbr;		/* 0xae8 */
	int bbb;		/* 0xaec */
	int m_af0;		/* 0xaf0 */
	int ddax;		/* 0xaf4  DDA start x */
	int dday;		/* 0xaf8  DDA start y */
	unsigned int covs[3];	/* 0xafc  AA coverage start values */
	unsigned int covdx[3];	/* 0xb08 */
	unsigned int covdy[3];	/* 0xb14 */

	long long ozv;		/* 0xb20  output block: start values */
	int ofv;		/* 0xb28 */
	int oav;		/* 0xb2c */
	int orv;		/* 0xb30 */
	int ogv;		/* 0xb34 */
	int obv;		/* 0xb38 */
	int osv;		/* 0xb3c */
	int otv;		/* 0xb40 */
	int oqv;		/* 0xb44 */
	long long ozdx;		/* 0xb48 */
	int ofdx;		/* 0xb50 */
	int oadx;		/* 0xb54 */
	int ordx;		/* 0xb58 */
	int ogdx;		/* 0xb5c */
	int obdx;		/* 0xb60 */
	int osdx;		/* 0xb64 */
	int otdx;		/* 0xb68 */
	int oqdx;		/* 0xb6c */
	long long ozdy;		/* 0xb70 */
	int ofdy;		/* 0xb78 */
	int oady;		/* 0xb7c */
	int ordy;		/* 0xb80 */
	int ogdy;		/* 0xb84 */
	int obdy;		/* 0xb88 */
	int osdy;		/* 0xb8c */
	int otdy;		/* 0xb90 */
	int oqdy;		/* 0xb94 */

	int xdir;		/* 0xb98 */
	int ydir;		/* 0xb9c */
	int steep[3];		/* 0xba0 */
	int flat;		/* 0xbac */
	int SCANMSK;		/* 0xbb0 */
	int send_type;		/* 0xbb4 */
	int send_addr;		/* 0xbb8 */
	long long send_reg;	/* 0xbbc */
	int TME;		/* 0xbc4 */
	int FGE;		/* 0xbc8 */
	int ABE;		/* 0xbcc */
	int AA1;		/* 0xbd0 */
	int m_bd4;		/* 0xbd4 */
	int CTXT;		/* 0xbd8 */
	int FST;		/* 0xbdc */
	int maxexp;		/* 0xbe0 */
	int m_be4;		/* 0xbe4 */
	int m_be8;		/* 0xbe8 */
	int m_bec;		/* 0xbec */
	unsigned int rem;	/* 0xbf0  reciproc's remainder output */
	int m_bf4;		/* 0xbf4 */
	int type;		/* 0xbf8 */
				/* 0xbfc vptr */

	int Subpixel(const int &v) { return v & ((1 << sft) - 1); }
	int Ceil(const int &v) {
		if (v & ((1 << sft) - 1))
			return (v >> sft) + 1;
		else
			return v >> sft;
	}
	int Floor(const int &v) { return v >> sft; }
	PCalc(PPDDA *p) {
		out = p;
		sft = 4;
		msft = 0x16;
		pix = 0x10;
		one = 1;
		SCANMSK = 0;
		scissor[0].scax0 = 0;
		scissor[0].scax1 = 0;
		scissor[0].scay0 = 0;
		scissor[0].scay1 = 0;
		scissor[1].scax0 = 0;
		scissor[1].scax1 = 0;
		scissor[1].scay0 = 0;
		scissor[1].scay1 = 0;
		m_aa4 = 0xffff;
		m_aac = 0xffff0000;
		m_ab4 = 0x10;
		maxexp = 0;
	}

	void SwapLine(int &a, int &b, int &c, int &d);
	void SwapLine(unsigned int &a, unsigned int &b, unsigned int &c,
		unsigned int &d);
	void SortVertex(Pre3 *p, param *v);
	void GetSPoint(void);
	void CorrectSPoint(void);
	void CorrectEPoint(void);
	void Slope(Pre3 *p, param *v);
	void CheckOverFlow(void);
	void StartVal(Pre3 *p, param *v, param w);
	void GetDDAStart(Pre3 *p);
	int AASlope(long long x, int n, int d);
	long long C_Hosei(long long v, int d);
	void SortCoverage(Pre3 *p);
	int AAStartVal(int a, int b, int c, int d);
	void AACoverage(Pre3 *p);
	void DrawTriangle(Pre3 *p);
	void BBox(void);
	void SortLine(param *v);
	void CorrectLineStart(void);
	void CorrectLineEnd(void);
	void LineSlope(param *v, int n, param &d, long long &a, long long &b,
		int &c, int &e);
	void LineDDAEdgeStart(void);
	void LineAACov(Pre3 *p);
	void DrawLine(Pre3 *p);
	void SpriteSlope(long long x, int n, long long &r);
	void SpriteStartVal(long long &r, long long a, long long b, int c);
	void DrawSprite(Pre3 *p);
	void DrawPoint(Pre3 *p);
	void ReverseDir(void);
	void Primitive(Pre3 *p);
	void Register(Pre3 *p);
	virtual void Put(Pre3 *p);
};

/* ---- Pre3 (CLONE of include/pre3.h minus its stand-in PCalc) ---- */


struct Vertex {			/* 0x30 bytes */
	int x;			/* 0x00  screen X, XYOFFSET already applied */
	int y;			/* 0x04 */
	long long z;		/* 0x08  24-bit Z, zero extended */
	int r;			/* 0x10 */
	int g;			/* 0x14 */
	int b;			/* 0x18 */
	int a;			/* 0x1c */
	int f;			/* 0x20  fog */
	int s;			/* 0x24  fixed point, common exponent */
	int t;			/* 0x28 */
	int q;			/* 0x2c */
};

class Pre3 {
public:
	PCalc *pcalc;		/* 0x000 */
	int nvtx;		/* 0x004  vertices in the queue == next slot */
	int S[3];		/* 0x008  raw 24-bit floats from Pre1 */
	int T[3];		/* 0x014 */
	int Q[3];		/* 0x020 */
	int dx[3];		/* 0x02c  dx[i] = v[i].x - v[i+1].x */
	int dy[3];		/* 0x038  dy[i] = v[i+1].y - v[i].y */
	int dxzero[3];		/* 0x044  dx[i] == 0 */
	int dyzero[3];		/* 0x050  dy[i] == 0 */
	int steep[3];		/* 0x05c  |dx[i]| < |dy[i]|, Y is the major axis */
	long long area;		/* 0x068  2 * signed triangle area */
	Vertex v[3];		/* 0x070 */
	long long send_reg;	/* 0x100  pass-through register value */
	int send_addr;		/* 0x108  pass-through register address */
	int type;		/* 0x10c  0 point 1 line 2 triangle 3 sprite */
	int send_type;		/* 0x110  0 = primitive, 1 = register */
	int CTXT;		/* 0x114 */
	int FST;		/* 0x118 */
	int AA1;		/* 0x11c  forced to 0 for points and sprites */
	int m_120;		/* 0x120  cleared by the ctor, never written */
	int ABE;		/* 0x124 */
	int FGE;		/* 0x128 */
	int TME;		/* 0x12c */
	int IIP;		/* 0x130 */
	int FIX;		/* 0x134 */
	int maxexp;		/* 0x138  Pre1::MaxExp() for this primitive */
	int m_13c;		/* 0x13c  never touched by pre3.o */
	int m_140;		/* 0x140  never touched by pre3.o */
	int restart;		/* 0x144  reload the queue; Pre1 clears it */
				/* 0x148 vptr */

	Pre3(PCalc *p) {
		pcalc = p;
		nvtx = 0;
		m_120 = 0;
		maxexp = 0;
		restart = 1;
	}
	int NumVertex() { return nvtx; }

	void Register(Pre1 *p);
	int Float2Fix(int val, unsigned int maxexp);
	void SetAttr(Pre1 *p);
	int Triangle(Pre1 *p);
	void Point(Pre1 *p);
	int Line(Pre1 *p);
	int Sprite(Pre1 *p);
	void Primitive(Pre1 *p);
	virtual void Put(Pre1 *p);
};

#include "pre1.h"

/* The rasterizer's interface, as txm.h declares it: one pure virtual,
 * so the local _vt.6DDATXM's one entry is __pure_virtual. */
class DDATXM {
public:
	virtual void Put(DDA *d) = 0;
};

/* ---- DDA (CLONE of include/dda.h minus its stand-in PCalc/DDATXM) ---- */

/* One "column" of DDA state.  The same 19-word shape is used four
 * times: the live values, the saved copy taken at the start of the
 * scanline, the per-stamp x step and the per-stamp y step. */
struct DDAvalue {		/* 0x4c */
	int x;			/* 0x00  pixel x (even), bit 0 = xdir */
	long long z;		/* 0x04 */
	int cov[3];		/* 0x0c  AA coverage, per edge */
	int a;			/* 0x18 */
	int b;			/* 0x1c */
	int g;			/* 0x20 */
	int r;			/* 0x24 */
	int f;			/* 0x28 */
	int s;			/* 0x2c */
	int t;			/* 0x30 */
	int q;			/* 0x34 */
	int e[3];		/* 0x38  the three edge functions */
	int el;			/* 0x44  distance to the bbox left edge */
	int er;			/* 0x48  distance to the bbox right edge */
};

class DDA {
public:
	PCalc *pcalc;		/* 0x000  the primitive being drawn */
	DDATXM *txm;		/* 0x004  the next stage */
	int y;			/* 0x008  the stamp's base scanline (mask row 0) */
	int bt;			/* 0x00c  rows past the bbox start edge, +2 a row */
	int bb;			/* 0x010  rows left to the bbox end edge, -2 a row */
	int yn;			/* 0x014  scanlines left to the last vertex */

	DDAvalue v;		/* 0x018  live values */
	DDAvalue sv;		/* 0x064  saved at the start of the scanline */
	int dyy;		/* 0x0b0  y step of y/bt/bb/yn */
	int dybt;		/* 0x0b4 */
	int dybb;		/* 0x0b8 */
	int dyyn;		/* 0x0bc */
	DDAvalue dx;		/* 0x0c0  per-stamp x step */
	DDAvalue dy;		/* 0x10c  per-stamp y step */

	/* the block TXM reads */
	int isreg;		/* 0x158  1 = a register write, not a stamp */
	int first;		/* 0x15c  1 = first stamp of the primitive */
	int reg_addr;		/* 0x160 */
	long long reg_data;	/* 0x164 */
	int px;			/* 0x16c  x, bit 0 = xdir (or the reg addr) */
	int py;			/* 0x170  y/2 */
	int mask;		/* 0x174  2x8 pixel mask */
	long long z0;		/* 0x178  row 0 */
	int a0;			/* 0x180 */
	int b0;			/* 0x184 */
	int g0;			/* 0x188 */
	int r0;			/* 0x18c */
	int f0;			/* 0x190 */
	long long z1;		/* 0x194  row 1 */
	int a1;			/* 0x19c */
	int b1;			/* 0x1a0 */
	int g1;			/* 0x1a4 */
	int r1;			/* 0x1a8 */
	int f1;			/* 0x1ac */
	int s0;			/* 0x1b0 */
	int t0;			/* 0x1b4 */
	int q0;			/* 0x1b8 */
	int s1;			/* 0x1bc */
	int t1;			/* 0x1c0 */
	int q1;			/* 0x1c4 */
	int cova0;		/* 0x1c8  row 0 coverage, edge 0 */
	int covb0;		/* 0x1cc  row 0 coverage, edge 1 or 2 */
	int cova1;		/* 0x1d0  row 1 coverage, edge 0 */
	int covb1;		/* 0x1d4 */
	int covdxa;		/* 0x1d8  coverage d/dx, edge 0 */
	int covdxb0;		/* 0x1dc  row 0 coverage d/dx */
	int covdxb1;		/* 0x1e0  row 1 coverage d/dx */
	int m_1e4;		/* 0x1e4 */
	int m_1e8;		/* 0x1e8 */
	int m_1ec;		/* 0x1ec */
	int esel0;		/* 0x1f0  row 0 uses edge 2, not edge 1 */
	int esel1;		/* 0x1f4  row 1 uses edge 2, not edge 1 */
	int amask;		/* 0x1f8  AA edge mask (or the reg addr) */
	long long dzdx;		/* 0x1fc  per-pixel slopes, extended */
	int dadx;		/* 0x204 */
	int dbdx;		/* 0x208 */
	int dgdx;		/* 0x20c */
	int drdx;		/* 0x210 */
	int dfdx;		/* 0x214 */
	int dsdx;		/* 0x218 */
	int dtdx;		/* 0x21c */
	int dqdx;		/* 0x220 */
	int zc;			/* 0x224  Z carries out of the stamp */
	int ydir;		/* 0x228 */
	int TME;		/* 0x22c */
	int FGE;		/* 0x230 */
	int ABE;		/* 0x234 */
	int FST;		/* 0x238 */
	int AA1;		/* 0x23c */
	int m_240;		/* 0x240 */
	int CTXT;		/* 0x244 */
	int maxexp;		/* 0x248 */
	int type;		/* 0x24c */
				/* 0x250 vptr */

	DDA(DDATXM *t) {
		txm = t;
		m_240 = 0;
		m_1e4 = m_1e8 = m_1ec = 0;
	}

	void InitStamp(void);
	int Stamping(int n);
	void InitWalk(void);
	void HorizontalWalk(void);
	void VerticalWalk(void);
	int IsVerticalWalk(void);
	int IsWalk(void);
	void Register(void);
	void Primitive(void);
	virtual void Put(PCalc *p);
};

/* The PCalc->DDA tap stage; always installed by GPU2 and GPU2VEC alike
 * (dbg.o, gpu2.o and gpu2vec.o each carry the same local _vt.5PPOut and
 * weak Put).  Forwards through the DDA's vtable. */
class PPOut : public PPDDA {
public:
	DDA *dda;		/* 0x04 */

	PPOut(DDA *d) { dda = d; }
	void Put(PCalc *p) { dda->Put(p); }
};

/* ---- MemIF (CLONE of include/memif.h; the constructor is inline
 * here - gpu2vec.o and gpu2.o expand it into their own constructors and
 * neither references an out-of-line __5MemIFP6Memory) ---- */

/* ATST 0..7: NEVER, ALWAYS, LESS, LEQUAL, EQUAL, GEQUAL, GREATER,
 * NOTEQUAL - all written the other way round, as AREF <op> A. */
class AlphaTest {
public:
	int ATE;		/* 0x00 */
	int ATST;		/* 0x04 */
	int AFAIL;		/* 0x08 */
	int AREF;		/* 0x0c */

	int Pass(int a);
	void ATest(PixelStamp &s);
};

/* The destination alpha test: keep the pixel only when the frame buffer's
 * alpha MSB matches DATM. */
class DAlphaTest {
public:
	int DATE;		/* 0x00 */
	int DATM;		/* 0x04 */

	void DATest(Memory *mem, PixelStamp &s);
};

/* ZTST 0..3: NEVER, ALWAYS, GEQUAL, GREATER. */
class DepthTest {
public:
	int ZTE;		/* 0x00 */
	int ZTST;		/* 0x04 */

	void ZTest(Memory *mem, PixelStamp &s);
};

/* Cout = (A - B) * C >> 7 + D.  The four selectors are 0 = source,
 * 1 = destination (the frame buffer), else 0 / FIX. */
class AlphaBlend {
public:
	int PABE;		/* 0x00  per-pixel alpha blending */
	int A;			/* 0x04 */
	int B;			/* 0x08 */
	int D;			/* 0x0c  note: D before C, as ALPHA decodes */
	int C;			/* 0x10 */
	int FIX;		/* 0x14 */

	void Blend(Memory *mem, PixelStamp &s);
};

class Dither {
public:
	int DTHE;		/* 0x00 */
	int mat[4][4];		/* 0x04  DIMX, sign extended from 3 bits */

	void Dithering(PixelStamp &s);
};

class ColorClamp {
public:
	int CLAMP;		/* 0x00  1 = clamp, else mask to 8 bits */

	void Clamp(PixelStamp &s);
};

class MemIF {
public:
	Memory *mem;		/* 0x00 */
	int ctxt;		/* 0x04 */
	AlphaTest atest;	/* 0x08  live */
	AlphaTest atestc[2];	/* 0x18  TEST_1 / TEST_2 */
	DAlphaTest datest;	/* 0x38 */
	DAlphaTest datestc[2];	/* 0x40 */
	DepthTest ztest;	/* 0x50 */
	DepthTest ztestc[2];	/* 0x58 */
	AlphaBlend blend;	/* 0x68 */
	AlphaBlend blendc[2];	/* 0x80  ALPHA_1 / ALPHA_2 */
	Dither dither;		/* 0xb0 */
	ColorClamp clamp;	/* 0xf4 */
				/* 0xf8  vptr */

	MemIF(Memory *m) { mem = m; }
	virtual void Stamp(PixelStamp &s);
	int ReadWord(int i);
	void SetContext(Gpu2RegCtxt c);
	void SetPABE(long long data);
	void SetCOLCLAMP(long long data);
	void SetDTHE(long long data);
	void SetDIMX(long long data);
	void SetALPHA(int ctx, long long data);
	void SetTEST(int ctx, long long data);
	void Context();
};

#define MEMIF_DECLARED	/* clut.h's stand-in; the real one is above */
#include "clut.h"
#include "txm_div.h"

/* ---- TXM (CLONE of include/txm.h minus its includes and its DDATXM;
 * the text is otherwise identical, so the SearchQlevel assert strings
 * land in .rodata exactly as they do in txm.o and gpu2.o) ---- */

/* Texturing declares two: the ctor pops InitTable's argument at once. */
class TexCoord : public NormTexCoord {
public:
	TexCoord() { InitTable(); }
};

/* TEX0.FST: 0 = STQ, 1 = UV.  Named by Texturing's mangling. */
enum Gpu2RegFST { Gpu2RegSTQ, Gpu2RegUV };

/* The era <assert.h> expansion, so __FILE__ comes out as "txm.h". */
extern "C" void __assert_fail(const char *, const char *, unsigned int,
	const char *) __attribute__((__noreturn__));
#define assert(e) \
	((e) ? (void)0 : __assert_fail(#e, "txm.h", __LINE__, \
		__PRETTY_FUNCTION__))

/* Significant bits of a CLAMP MINU/MINV field (the REGION_REPEAT mask
 * width); a free inline, so no out-of-line copy reaches txm.o. */
inline int
Bits(int a)
{
	int i, m;

	m = 0x200;
	for (i = 9; i >= 0; i--) {
		if (a & m)
			break;
		m >>= 1;
	}
	return i + 1;
}

/* texfunc.o's stage; the per-context copies are TXM's. */
class TexFunc {
public:
	int func;		/* 0x00  TEX0.TFX, live */
	int funcc[2];		/* 0x04  TEX0_1 / TEX0_2 */
	int tcc;		/* 0x0c  TEX0.TCC, live */
	int tccc[2];		/* 0x10 */
	Gpu2RegCtxt ctxt;	/* 0x18 */

	void Func(PixColor &t, PixColor &f);
	void Context(void) {
		if (ctxt == 0) { func = funcc[0]; tcc = tccc[0]; }
		else { func = funcc[1]; tcc = tccc[1]; }
	}
	void SetContext(Gpu2RegCtxt c) { ctxt = c; Context(); }
};

/* TEXA: the alpha substituted for formats with fewer than 8 alpha bits. */
class TexA {
public:
	int AEM, TA0, TA1;	/* 0x00 */
};

/* FOGCOL. */
class Fog {
public:
	int R, G, B;		/* 0x00 */

	void Fogging(Pixel &p);
};

/* The antialias coverage source: the DDA whose slopes ExtCov re-walks. */
class AA {
public:
	const DDA *dda;		/* 0x00 */

	void Set(const DDA *d);
};

/* What TEX0/TEX1/MIPTBP/CLAMP/SCISSOR/FRAME decode into, per context. */
struct TexAttr {
	int TBP[7];		/* 0x00  TBP0..TBP6 * 64, word addresses */
	int TBW[7];		/* 0x1c  TBW0..TBW6 * 64, pixels */
	int PSM;		/* 0x38 */
	int W, H;		/* 0x3c  1 << TW, 1 << TH */
	int TW, TH, TCC;	/* 0x44 */
	int LCM, L, K, MXL;			/* 0x50  K: signed 12 bit */
	int MMAG, MMIN, MTBA;			/* 0x60 */
	int WMS, WMT;				/* 0x6c */
	int MINU, MAXU, MINV, MAXV;		/* 0x74  all * 16 */
	int SCAX0, SCAX1, SCAY0, SCAY1;		/* 0x84 */
	int FBP;		/* 0x94  FBP * 2048 */
	int FBW;		/* 0x98  FBW * 64 */
	int FPSM;		/* 0x9c */
	int rMAXU, rMINU, rMAXV, rMINV;		/* 0xa0  raw, REGION_REPEAT */
	int rUbits, rVbits;			/* 0xb0  bits of rMINU/rMINV */

	void MipTbpAuto(void);

	/* CLAMP: 0 REPEAT, 1 CLAMP, 2 REGION_CLAMP, 3 REGION_REPEAT.
	 * Members, not free inlines: the object reads WMS/MINU/MAXU in
	 * the arm that needs them and pins the coordinate to a stack
	 * slot by taking its address. */
	void WrapU(int &c, int w, int lod) {
		if (WMS == 0)
			c &= (1 << (w + 4)) - 1;
		else if (WMS == 1) {
			if (c < 0) c = 0;
			else if (c > (16 << w) - 16) c = (16 << w) - 16;
		} else if (WMS == 2) {
			int lo, hi;
			lo = (MINU >> 4 >> lod) << 4;
			hi = (MAXU >> 4 >> lod) << 4;
			if (c < lo) c = lo;
			else if (c > hi) c = hi;
		} else
			c = (c & (((rMINU >> lod) << 4) | 0xf)) |
				((rMAXU >> lod) << 4);
	}

	void WrapV(int &c, int w, int lod) {
		if (WMT == 0)
			c &= (1 << (w + 4)) - 1;
		else if (WMT == 1) {
			if (c < 0) c = 0;
			else if (c > (16 << w) - 16) c = (16 << w) - 16;
		} else if (WMT == 2) {
			int lo, hi;
			lo = (MINV >> 4 >> lod) << 4;
			hi = (MAXV >> 4 >> lod) << 4;
			if (c < lo) c = lo;
			else if (c > hi) c = hi;
		} else
			c = (c & (((rMINV >> lod) << 4) | 0xf)) |
				((rMAXV >> lod) << 4);
	}
};

/* include/clut.h's TexClut (clut.o's own view, which stops at 0x498)
 * plus the per-context selector TXM keeps behind it at 0x6f0. */
class TexClutCtx : public TexClut {
public:
	Gpu2RegCtxt ctxt;	/* 0x498 */

	void Context(void) {
		if (ctxt == 0)
			attr = attr1;
		else
			attr = attr2;
	}
	void SetContext(Gpu2RegCtxt c) { ctxt = c; Context(); }
};

/* valid8 maps the eight centre pixels of a stamp to the one whose Q the
 * LOD comes from.  A class, not a bare array: its constructor is what
 * puts `_GLOBAL_.I._3TXM.valid8' in .ctors and the 256 bytes in .data
 * with .bss empty, exactly as txm.o has them. */
class Valid8 {
public:
	char tbl[256];

	Valid8() {
		int i;

		for (i = 0; ; i++) {
			if (i > 255)
				return;
			if (i & 0x02) tbl[i] = 1;
			else if (i & 0x20) tbl[i] = 9;
			else if (i & 0x04) tbl[i] = 2;
			else if (i & 0x40) tbl[i] = 10;
			else if (i & 0x01) tbl[i] = 0;
			else if (i & 0x10) tbl[i] = 8;
			else if (i & 0x08) tbl[i] = 3;
			else tbl[i] = 11;
		}
	}
};

class TXM : public DDATXM, public AddrConv {
public:
	MemIF *memif;		/* 0x024 */
	int m_28;		/* 0x028 */
	Gpu2RegCtxt ctxt;	/* 0x02c */
	TexAttr attr;		/* 0x030  the live context */
	TexAttr attrc[2];	/* 0x0e8  context 1 / context 2 */
	TexClutCtx clut;	/* 0x258 */
	TexA texa;		/* 0x6f4 */
	TexFunc texfunc;	/* 0x700 */
	Fog fog;		/* 0x71c */
	AA aa;			/* 0x728 */

	static Valid8 valid8;

	void GetOneTexel(int u, int v, int lod, PixColor &c);
	void NFilter(NormTexCoord &u, NormTexCoord &v, int lod, PixColor &c);
	void LFilter(NormTexCoord &u, NormTexCoord &v, int lod, PixColor &c);
	void NMNFilter(NormTexCoord &u, NormTexCoord &v, int lod, PixColor &c);
	void NMLFilter(NormTexCoord &u, NormTexCoord &v, int lod, PixColor &c);
	void LMNFilter(NormTexCoord &u, NormTexCoord &v, int lod, PixColor &c);
	void LMLFilter(NormTexCoord &u, NormTexCoord &v, int lod, PixColor &c);
	void Texturing(Pixel &p, int lod, Gpu2RegFST fst);
	int ComputeLod(PixelStamp &s);
	void Stamp(PixelStamp &s);
	void ExtCov(PixelStamp &s);
	void AA1(PixelStamp &s);

	/* The inline members below come out out-of-line at the end of
	 * .text in reverse declaration order (g++ 2.7 writes a class's
	 * inlines where it writes its vtable): this order is byte-visible,
	 * and so is SearchQlevel's line number.  KEEP IT ON LINE 450. */

	void Context(void) {
		if (ctxt == 0)
			attr = attrc[0];
		else
			attr = attrc[1];
	}

	void SetTEX0(int ctx, long long data) {
		TexAttr *a = &attrc[ctx];
		TexClutCtx *cl;
		ClutAttr *c;

		a->TBP[0] = (data & 0x3fff) << 6;
		a->PSM = (data >> 20) & 0x3f;
		a->TBW[0] = ((data >> 14) & 0x3f) << 6;
		a->TW = (data >> 26) & 0xf;
		a->TH = (data >> 30) & 0xf;
		a->W = 1 << a->TW;
		a->H = 1 << a->TH;
		a->TCC = (data >> 34) & 1;
		if (a->MTBA == 1)
			a->MipTbpAuto();
		Context();

		cl = &clut;
		c = &(&cl->attr1)[ctx];
		c->PSM = (data >> 20) & 0x3f;
		c->CBP = ((data >> 37) & 0x3fff) << 6;
		c->CPSM = (data >> 51) & 0xf;
		c->CSM = (data >> 55) & 1;
		c->CSA = ((data >> 56) & 0x1f) << 4;
		c->CLD = (data >> 61) & 7;
		cl->LoadData((&cl->attr1)[ctx]);
		cl->Context();

		texfunc.funcc[ctx] = (data >> 35) & 3;
		texfunc.tccc[ctx] = (data >> 34) & 1;
		texfunc.Context();
	}

	void SetTEX1(int ctx, long long data) {
		TexAttr *a = &attrc[ctx];

		a->LCM = data & 3;
		a->L = (data >> 19) & 3;
		a->K = (data >> 32) & 0xfff;
		if (a->K & 0x800)
			a->K |= 0xfffff800;
		a->MXL = (data >> 2) & 7;
		a->MMAG = (data >> 5) & 1;
		a->MMIN = (data >> 6) & 7;
		a->MTBA = (data >> 9) & 1;
		Context();
	}

	void SetMIPTBP1(int ctx, long long data) {
		TexAttr *a = &attrc[ctx];

		a->TBP[1] = (data & 0x3fff) << 6;
		a->TBW[1] = ((data >> 14) & 0x3f) << 6;
		a->TBP[2] = ((data >> 20) & 0x3fff) << 6;
		a->TBW[2] = ((data >> 34) & 0x3f) << 6;
		a->TBP[3] = ((data >> 40) & 0x3fff) << 6;
		a->TBW[3] = ((data >> 54) & 0x3f) << 6;
		Context();
	}

	void SetMIPTBP2(int ctx, long long data) {
		TexAttr *a = &attrc[ctx];

		a->TBP[4] = (data & 0x3fff) << 6;
		a->TBW[4] = ((data >> 14) & 0x3f) << 6;
		a->TBP[5] = ((data >> 20) & 0x3fff) << 6;
		a->TBW[5] = ((data >> 34) & 0x3f) << 6;
		a->TBP[6] = ((data >> 40) & 0x3fff) << 6;
		a->TBW[6] = ((data >> 54) & 0x3f) << 6;
		Context();
	}

	void SetCLAMP(int ctx, long long data) {
		TexAttr *a = &attrc[ctx];
		int minu, maxu, minv, maxv;

		a->WMS = data & 3;
		a->WMT = (data >> 2) & 3;
		minu = (data >> 4) & 0x3ff;
		a->MINU = minu*16;
		maxu = (data >> 14) & 0x3ff;
		a->MAXU = maxu*16;
		minv = (data >> 24) & 0x3ff;
		a->MINV = minv*16;
		maxv = (data >> 34) & 0x3ff;
		a->MAXV = maxv*16;
		a->rMINU = minu;
		a->rMAXU = maxu;
		a->rMINV = minv;
		a->rMAXV = maxv;
		a->rUbits = Bits(a->rMINU);
		a->rVbits = Bits(a->rMINV);
		Context();
	}

	void SetSCISSOR(int ctx, long long data) {
		TexAttr *a = &attrc[ctx];

		a->SCAX0 = data & 0x7ff;
		a->SCAX1 = (data >> 16) & 0x7ff;
		a->SCAY0 = (data >> 32) & 0x7ff;
		a->SCAY1 = (data >> 48) & 0x7ff;
		Context();
	}

	void SetFRAME(int ctx, long long data) {
		TexAttr *a = &attrc[ctx];

		a->FBP = (data & 0x1ff) << 11;
		a->FBW = ((data >> 16) & 0x3f) << 6;
		a->FPSM = (data >> 24) & 0x3f;
		Context();
	}

	void SetContext(Gpu2RegCtxt c) {
		ctxt = c;
		Context();
		texfunc.SetContext(ctxt);
		clut.SetContext(ctxt);
	}

	void SetTEX2(int ctx, long long data) {
		TexClutCtx *cl = &clut;
		ClutAttr *c = &(&cl->attr1)[ctx];

		c->PSM = (data >> 20) & 0x3f;
		c->CBP = ((data >> 37) & 0x3fff) << 6;
		c->CPSM = (data >> 51) & 0xf;
		c->CSM = (data >> 55) & 1;
		c->CSA = ((data >> 56) & 0x1f) << 4;
		c->CLD = (data >> 61) & 7;
		cl->LoadData((&cl->attr1)[ctx]);
		cl->Context();
	}

	void SetTEXCLUT(long long data) {
		TexClutCtx *cl = &clut;
		int cbw, cou, cov;

		cbw = (data & 0x3f) << 6;
		clut.attr1.CBW = cbw;
		cou = ((data >> 6) & 0x3f) << 4;
		clut.attr1.COU = cou;
		cov = (data >> 12) & 0x3ff;
		clut.attr1.COV = cov;
		clut.attr2.CBW = cbw;
		clut.attr2.COU = cou;
		clut.attr2.COV = cov;
		cl->Context();
	}

	void SetTEXA(long long data) {
		texa.TA0 = data & 0xff;
		texa.TA1 = (data >> 32) & 0xff;
		texa.AEM = (data >> 15) & 1;
	}

	void SetFOGCOL(long long data) {
		fog.R = data & 0xff;
		fog.G = (data >> 8) & 0xff;
		fog.B = (data >> 16) & 0xff;
	}

	int ClampLod(int lod) {
		int m;

		if (lod >= 0) {
			m = attr.MXL*16;
			if (lod >= m)
				return m;
			return lod;
		}
		return 0;
	}

	int ClampT(int v) {
		unsigned a;
		int s, r;

		a = v >> 8;
		s = (a >> 19) & 1;
		if (s)
			a = -a;
		r = 0x1ffff;
		if ((a & 0x60000) == 0)
			r = a & 0x1ffff;
		a = r;
		if (s)
			a = -a;
		return a;
	}

	int ClampQ(int v) {
		int r;
		unsigned a;

		a = v >> 8;
		if ((a & 0x80000) == 0) {
			r = 0x1ffff;
			if ((a & 0x60000) == 0)
				r = a & 0x1ffff;
			return r;
		}
		return 0;
	}

	/* The Q the LOD is computed from: the first live pixel of the
	 * stamp's eight centre columns, in valid8's priority order.  The
	 * assert below is on line 450 and its line number, its file name
	 * and its __PRETTY_FUNCTION__ are all in txm.o's .rodata. */
	int SearchQlevel(PixelStamp &s) {
		int vld8, i;

		vld8 = (s.mask & 0xf) | ((s.mask >> 4) & 0xf0);
		assert(vld8 != 0);
		i = valid8.tbl[vld8];
		return s.pix[i].m_24;
	}

	TXM(MemIF *m) {
		int i;

		for (i = 1; i != -1; i--)
			;
		memif = m;
		clut.memif = m;
	}

	int LFilter1(int a, int b, int p0, int p1, int p2, int p3) {
		int x, y;

		x = ((16 - a)*p0 + a*p1) >> 4;
		y = ((16 - a)*p2 + a*p3) >> 4;
		return ((16 - b)*x + b*y) >> 4;
	}

	int MFilter1(int a, int x, int y) {
		return ((16 - a)*x + a*y) >> 4;
	}

	virtual void Put(DDA *d);
};
#include "pcrtc.h"

/* ---- the tap layer itself ---------------------------------------- */

/* Each My* stage carries one FILE* behind the base object; a null FILE*
 * means the tap is off and Put/Stamp forward straight to the base.
 * MyDDA/MyTXM/MyMemIF define their Put/Stamp override out of line in
 * gpu2vec.c, so each is its class's key method: the vtable and all the
 * inline members below come out *global* in gpu2vec.o.  MyMemory's only
 * virtual is the inline destructor, so its vtable stays local and the
 * destructor weak - exactly the split the 1998 object has. */

class MyDDA : public DDA {
public:
	FILE *fp;		/* 0x254 */

	MyDDA(TXM *t) : DDA(t) { fp = 0; }
	virtual ~MyDDA() { if (fp) fclose(fp); }
	void SetVector(FILE *f) { fp = f; }

	void RegisterVec(PCalc *p);
	void TriangleVec(PCalc *p);
	void Put(PCalc *p);
};

class MyTXM : public TXM {
public:
	FILE *fp;		/* 0x72c */

	MyTXM(MemIF *m) : TXM(m) { fp = 0; }
	virtual ~MyTXM() { if (fp) fclose(fp); }
	void SetVector(FILE *f) { fp = f; }

	void RegisterVec(DDA *d);
	void PrimitiveVec(DDA *d);
	void Put(DDA *d);
};

class MyMemIF : public MemIF {
public:
	FILE *fp;		/* 0xfc */

	MyMemIF(Memory *m) : MemIF(m) { fp = 0; }
	virtual ~MyMemIF() { if (fp) fclose(fp); }
	void SetVector(FILE *f) { fp = f; }

	void RegisterVec(PixelStamp &s);
	void PrimitiveVec(PixelStamp &s);
	void Stamp(PixelStamp &s);
};

class MyMemory : public Memory {
public:
	FILE *fp;		/* 0x4001c8 */

	MyMemory() { fp = 0; }
	virtual ~MyMemory() { if (fp) fclose(fp); }
	void SetVector(FILE *f) { fp = f; }

	void Dump();
};

/* The front-end block (GPU2's unnamed 0x10-byte Pre1/Pre3/PCalc/PPOut
 * holder) plus the register-tap FILE*.  No virtuals and no base, so no
 * vtable and nothing deferred; Put logs the register write and forwards
 * to Pre1 by a direct call, just as GPU2::Put does. */
class MyPP {
public:
	Pre1 *pre1;		/* 0x00 */
	Pre3 *pre3;		/* 0x04 */
	PCalc *pcalc;		/* 0x08 */
	PPOut *out;		/* 0x0c */
	FILE *fp;		/* 0x10 */

	MyPP(MyDDA *d) {
		out = new PPOut(d);
		pcalc = new PCalc(out);
		pre3 = new Pre3(pcalc);
		pre1 = new Pre1(pre3);
		fp = 0;
	}
	void SetVector(FILE *f) { fp = f; }

	void Put(int addr, long long data);
};

/* The instrumented top level.  Same six-slot layout as GPU2 plus the
 * vector-select state SetVector() keeps (vec = which stage is tapped,
 * fp = the raw-register tap of mode 6). */
class GPU2VEC {
public:
	MyPP *pp;		/* 0x00 */
	MyDDA *dda;		/* 0x04 */
	MyTXM *txm;		/* 0x08 */
	MyMemIF *memif;		/* 0x0c */
	MyMemory *mem;		/* 0x10 */
	PCRTC *pcrtc;		/* 0x14 */
	int vec;		/* 0x18 */
	FILE *fp;		/* 0x1c */

	GPU2VEC(char *title, int width, int height, int disp_on);
	unsigned int GetCRT();
	long long Get();
	int Put(int addr, long long data);
	void ResizeWindow(int w, int h);
	void SetVector(int sel, FILE *f);
};

#endif
