#ifndef TXM_H
#define TXM_H

/* TXM - the texture machine.  The DDA pushes one 8x2 stamp at a time
 * into TXM::Put through the `DDATXM' interface; TXM turns it into a
 * PixelStamp, textures, antialiases and fogs it and hands it to
 * MemIF::Stamp.  Register writes take the same path.  Layout verified
 * against orig/lib/txm.o (0x72c); doc/notes/txm.md.  SearchQlevel's
 * assert bakes __LINE__ (450) and "txm.h" into .rodata: LINE NUMBERS
 * BELOW ARE LOAD BEARING. */
#include "memif.h"
#define MEMIF_DECLARED	/* clut.h's stand-in; memif.h has the real one */
#include "clut.h"
#include "txm_div.h"

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

class DDA;

/* The rasterizer's interface: one pure virtual, so txm.o has a local
 * `_vt.6DDATXM' whose one entry is `__pure_virtual'. */
class DDATXM {
public:
	virtual void Put(DDA *d) = 0;
};

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

#endif
