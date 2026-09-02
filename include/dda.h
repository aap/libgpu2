#ifndef DDA_H
#define DDA_H

/* DDA - the scanline rasterizer.
 *
 * PCalc hands the DDA a fully set-up primitive: a start position, the
 * three edge functions with their x/y gradients, the clipped bounding
 * box, and the start value and d/dx, d/dy slopes of every interpolated
 * attribute (see doc/notes/pcalc.md, PCalc's output block at 0xb20).
 * The DDA walks that state over the primitive one *stamp* at a time - a
 * 2-scanline by 4- or 8-pixel block - decides which of the stamp's
 * pixels are inside, and pushes the block to TXM through the DDATXM
 * vtable.  A register write travels the same path with the 64-bit value
 * packed into the interpolator's start-value fields (DDA::Register).
 *
 * Layout verified against orig/lib/dda.o and the DDA construction that
 * gpu2.o inlines: sizeof 0x254, vptr at 0x250, one virtual (Put), the
 * constructor `DDA(DDATXM*)' defined inline here because dda.o emits it
 * *after* Put() - the g++ 2.7 signature of a header inline (the same
 * tell as PCalc's constructor, doc/notes/pcalc.md).
 *
 * PCalc is declared here rather than pulled from include/pcalc.h: that
 * header's PCalc carries inline members, and g++ 2.7 emits an
 * out-of-line copy of every header inline into every including TU - so
 * including it would put Subpixel/Ceil/Floor and PCalc's constructor
 * into dda.o, which the 1998 object does not have.  Only the fields the
 * DDA reads are named; everything below 0xabc is a hole here.
 */

class DDA;
class Pre3;

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

class PCalc {
public:
	char m_000[0xabc];	/* 0x000  not read by the DDA */
	int m_abc;		/* 0xabc  edge function 0 at the start */
	int m_ac0;		/* 0xac0 */
	int m_ac4;		/* 0xac4 */
	int ddx[3];		/* 0xac8  dE/dx per edge */
	int ddy[3];		/* 0xad4  dE/dy per edge */
	int bbl;		/* 0xae0  bounding box, as distances */
	int bbt;		/* 0xae4 */
	int bbr;		/* 0xae8 */
	int bbb;		/* 0xaec */
	int m_af0;		/* 0xaf0  scanlines left to the last vertex */
	int ddax;		/* 0xaf4  DDA start, in 2x2 stamp units */
	int dday;		/* 0xaf8 */
	unsigned int covs[3];	/* 0xafc  AA coverage start values */
	unsigned int covdx[3];	/* 0xb08 */
	unsigned int covdy[3];	/* 0xb14 */

	long long ozv;		/* 0xb20  start values */
	int ofv;		/* 0xb28 */
	int oav;		/* 0xb2c */
	int orv;		/* 0xb30 */
	int ogv;		/* 0xb34 */
	int obv;		/* 0xb38 */
	int osv;		/* 0xb3c */
	int otv;		/* 0xb40 */
	int oqv;		/* 0xb44 */
	long long ozdx;		/* 0xb48  d/dx */
	int ofdx;		/* 0xb50 */
	int oadx;		/* 0xb54 */
	int ordx;		/* 0xb58 */
	int ogdx;		/* 0xb5c */
	int obdx;		/* 0xb60 */
	int osdx;		/* 0xb64 */
	int otdx;		/* 0xb68 */
	int oqdx;		/* 0xb6c */
	long long ozdy;		/* 0xb70  d/dy */
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
	unsigned int send_addr;	/* 0xbb8 */
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
	unsigned int rem;	/* 0xbf0 */
	int m_bf4;		/* 0xbf4 */
	int type;		/* 0xbf8 */
	virtual void Put(Pre3 *p);
};

/* The next stage.  TXM derives from it; only the vtable matters here:
 * one entry, Put(DDA*), at offset 0 of the object. */
class DDATXM {
public:
	virtual void Put(DDA *d);
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

#endif
