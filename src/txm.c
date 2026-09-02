#include <stdio.h>
#include <stdlib.h>

#include "gsdefs.h"
#include "txm.h"

/* GS register numbers TXM decodes itself. */
#define GS_TEX0_1	0x06
#define GS_TEX0_2	0x07
#define GS_CLAMP_1	0x08
#define GS_CLAMP_2	0x09
#define GS_TEX1_1	0x14
#define GS_TEX1_2	0x15
#define GS_TEX2_1	0x16
#define GS_TEX2_2	0x17
#define GS_TEXCLUT	0x1c
#define GS_SCANMSK	0x22
#define GS_MIPTBP1_1	0x34
#define GS_MIPTBP1_2	0x35
#define GS_MIPTBP2_1	0x36
#define GS_MIPTBP2_2	0x37
#define GS_TEXA		0x3b
#define GS_FOGCOL	0x3d
#define GS_SCISSOR_1	0x40
#define GS_SCISSOR_2	0x41

/* Not <math.h>: the era libc5 header declares log() with attributes that
 * change how gcc 2.7 pops its arguments (doc/MATCHING.md, txm_div). */
extern "C" double log(double);

Valid8 TXM::valid8;

/* The DDA as TXM reads it.  include/dda.h is the rasterizer's own view
 * and carries an inline constructor that g++ 2.7 would emit into txm.o -
 * the 1998 object has no such symbol, so TXM had a view of its own, and
 * it lives here rather than in txm.h because txm.h's line numbers are
 * load bearing (SearchQlevel's assert). */
class DDA {
public:
	char m_000[0x158];	/* 0x000  not read by TXM */
	int isreg, first, reg_addr;		/* 0x158 */
	long long reg_data;			/* 0x164 */
	int px, py, mask;			/* 0x16c */
	long long z0;				/* 0x178 */
	int a0, b0, g0, r0, f0;			/* 0x180 */
	long long z1;				/* 0x194 */
	int a1, b1, g1, r1, f1;			/* 0x19c */
	int s0, t0, q0, s1, t1, q1;		/* 0x1b0 */
	int cova0, covb0, cova1, covb1;		/* 0x1c8 */
	int covdxa, covdxb0, covdxb1;		/* 0x1d8 */
	int m_1e4, m_1e8, m_1ec;		/* 0x1e4 */
	int esel0, esel1, amask;		/* 0x1f0 */
	long long dzdx;				/* 0x1fc */
	int dadx, dbdx, dgdx, drdx;		/* 0x204 */
	int dfdx, dsdx, dtdx, dqdx;		/* 0x214 */
	int zc, ydir;				/* 0x224 */
	int TME, FGE, ABE, FST, AA1;		/* 0x22c */
	int m_240, CTXT, maxexp, type;		/* 0x240 */
};

/* ---- texel unpacking ------------------------------------------------ */

/* PSMCT32/PSMCT24 (and the 32 bit CLUT): one word, four bytes. */
inline void
Unpack32(PixColor &c, unsigned d)
{
	c.R = d & 0xff;
	c.G = (d >> 8) & 0xff;
	c.B = (d >> 16) & 0xff;
	c.A = d >> 24;
}

/* PSMCT16/PSMCT16S (and the 16 bit CLUT): 1-5-5-5, each component
 * replicated into the low bits. */
inline void
Unpack16(PixColor &c, int d)
{
	int r, g, b, a;

	r = d & 0x1f;
	g = (d >> 5) & 0x1f;
	b = (d >> 10) & 0x1f;
	a = (d >> 15) & 1;
	c.R = (r << 3) | (r >> 2);
	c.G = (g << 3) | (g >> 2);
	c.B = (b << 3) | (b >> 2);
	c.A = a;
}

inline int
IsZero(PixColor &c)
{
	return c.R == 0 && c.G == 0 && c.B == 0;
}

/* ---- one texel ------------------------------------------------------ */

void
TXM::GetOneTexel(int u, int v, int lod, PixColor &c)
{
	int psm;
	int d;
	int a;

	psm = attr.PSM;
	Address(u, v, psm, attr.TBW[lod], attr.TBP[lod]);
	switch (psm) {
	case PSMCT32:
	case PSMZ32:
		d = memif->mem->vram[addr];
		Unpack32(c, d);
		break;

	case PSMCT24:
	case PSMZ24:
		d = memif->mem->vram[addr];
		Unpack32(c, d);
		if (texa.AEM == 0)
			a = texa.TA0;
		else if (IsZero(c))
			a = 0;
		else
			a = texa.TA0;
		c.A = a;
		break;

	case PSMCT16:
	case PSMCT16S:
	case PSMZ16:
	case PSMZ16S:
		d = memif->mem->vram[addr];
		if (bitpos != 0)
			d = (unsigned)d >> 16;
		d &= 0xffff;
		Unpack16(c, d);
		if (c.A != 0)
			a = texa.TA1;
		else if (texa.AEM == 0)
			a = texa.TA0;
		else if (IsZero(c))
			a = 0;
		else
			a = texa.TA0;
		c.A = a;
		break;

	case PSMT8:
	case PSMT4:
	case PSMT8H:
	case PSMT4HL:
	case PSMT4HH:
		d = memif->mem->vram[addr];
		d = (unsigned)d >> bitpos;
		d &= (1 << Depth(psm)) - 1;
		d = clut.Lookup(d);
		if (clut.attr.CPSM == 0)
			Unpack32(c, d);
		else {
			Unpack16(c, d);
			if (c.A != 0)
				a = texa.TA1;
			else if (texa.AEM == 0)
				a = texa.TA0;
			else if (IsZero(c))
				a = 0;
			else
				a = texa.TA0;
			c.A = a;
		}
		break;

	default:
		fprintf(stderr, "TXM:Illegal Texture pixel format\n");
		exit(0);
	}
}

/* ---- texture coordinates -------------------------------------------- */

/* Denormalise one perspective-divided coordinate into 12.4 texels.
 * `w' is the texture's log2 size for this LOD, already reduced by `lod'
 * everywhere it matters. */
inline int
TexCoordN(NormTexCoord &n, int w, int lod)
{
	int e, r, t;
	unsigned m;

	t = n.ue + n.re;
	if (n.qzero == 0)
		e = t + w - lod - 1;
	else
		e = t - lod - 1;
	e = e > 16 ? 16 : e;
	m = (unsigned)((n.rm | 0x8000)*n.um) >> 15 & 0xffff;
	if (e == 16 || (e == 15 && (short)m < 0)) {
		r = 0;
		if (n.sign == 0)
			r = 0x7fff;
	} else if (e < 0)
		r = 0;
	else {
		r = m >> (15 - e);
		if (n.sign)
			r = -r;
	}
	return r;
}

/* The same, for the bilinear filters: they sample half a texel below and
 * to the left, but not when the coordinate has already saturated. */
inline int
TexCoordL(NormTexCoord &n, int w, int lod)
{
	int e, r, ovf, t;
	unsigned m;

	t = n.ue + n.re;
	if (n.qzero == 0)
		e = t + w - lod - 1;
	else
		e = t - lod - 1;
	e = e > 16 ? 16 : e;
	m = (unsigned)((n.rm | 0x8000)*n.um) >> 15 & 0xffff;
	if (e == 16 || (e == 15 && (short)m < 0)) {
		ovf = 1;
		r = 0;
		if (n.sign == 0)
			r = 0x7fff;
	} else {
		ovf = 0;
		if (e < 0)
			r = 0;
		else {
			r = m >> (15 - e);
			if (n.sign)
				r = -r;
		}
	}
	if (ovf == 0)
		r -= 8;
	return r;
}

/* ---- the six filters ------------------------------------------------ */

void
TXM::NFilter(NormTexCoord &u, NormTexCoord &v, int lod, PixColor &c)
{
	int uu, vv;
	int w, h;

	uu = TexCoordN(u, attr.TW, 0);
	vv = TexCoordN(v, attr.TH, 0);
	w = attr.TW;
	h = attr.TH;
	attr.WrapU(uu, w, 0);
	attr.WrapV(vv, h, 0);
	GetOneTexel(uu >> 4, vv >> 4, 0, c);
}

void
TXM::LFilter(NormTexCoord &u, NormTexCoord &v, int lod, PixColor &c)
{
	int uu, vv;
	int w, h;
	int au, av, u0, v0, u1, v1;
	int n, m;
	PixColor t0, t1, t2, t3;

	uu = TexCoordL(u, attr.TW, lod);
	vv = TexCoordL(v, attr.TH, lod);
	w = attr.TW - lod;
	h = attr.TH - lod;
	attr.WrapU(uu, w, lod);
	attr.WrapV(vv, h, lod);
	au = uu & 0xf;
	av = vv & 0xf;
	u0 = uu >> 4;
	v0 = vv >> 4;

	if (attr.WMS == 3)
		n = attr.rUbits;
	else
		n = attr.TW;
	n -= lod;
	if (n <= 2)
		n = 3;
	m = (1 << n) - 1;
	u1 = ((u0 + 1) & m) | (~m & u0);

	if (attr.WMT == 3)
		n = attr.rVbits;
	else
		n = attr.TH;
	n -= lod;
	if (n <= 2)
		n = 3;
	m = (1 << n) - 1;
	v1 = ((v0 + 1) & m) | (~m & v0);

	GetOneTexel(u0, v0, lod, t0);
	GetOneTexel(u1, v0, lod, t1);
	GetOneTexel(u0, v1, lod, t2);
	GetOneTexel(u1, v1, lod, t3);
	c.R = LFilter1(au, av, t0.R, t1.R, t2.R, t3.R);
	c.G = LFilter1(au, av, t0.G, t1.G, t2.G, t3.G);
	c.B = LFilter1(au, av, t0.B, t1.B, t2.B, t3.B);
	c.A = LFilter1(au, av, t0.A, t1.A, t2.A, t3.A);
}

void
TXM::NMNFilter(NormTexCoord &u, NormTexCoord &v, int lod, PixColor &c)
{
	int uu, vv;
	int w, h;
	int l, mxl;

	if (lod <= 7)
		l = 0;
	else {
		l = (lod + 8) >> 4;
		mxl = attr.MXL;
		if (l > mxl)
			l = mxl;
	}
	uu = TexCoordN(u, attr.TW, l);
	vv = TexCoordN(v, attr.TH, l);
	w = attr.TW - l;
	h = attr.TH - l;
	attr.WrapU(uu, w, l);
	attr.WrapV(vv, h, l);
	GetOneTexel(uu >> 4, vv >> 4, l, c);
}

void
TXM::NMLFilter(NormTexCoord &u, NormTexCoord &v, int lod, PixColor &c)
{
	int uu, vv;
	int w, h;
	int l0, l1, f, mxl;
	PixColor t0, t1;

	l0 = lod >> 4;
	l1 = l0 + 1;
	mxl = attr.MXL;
	if (l1 > mxl)
		l1 = mxl;

	uu = TexCoordN(u, attr.TW, l0);
	vv = TexCoordN(v, attr.TH, l0);
	w = attr.TW - l0;
	h = attr.TH - l0;
	attr.WrapU(uu, w, l0);
	attr.WrapV(vv, h, l0);
	GetOneTexel(uu >> 4, vv >> 4, l0, t0);

	uu = TexCoordN(u, attr.TW, l1);
	vv = TexCoordN(v, attr.TH, l1);
	w = attr.TW - l1;
	h = attr.TH - l1;
	attr.WrapU(uu, w, l1);
	attr.WrapV(vv, h, l1);
	GetOneTexel(uu >> 4, vv >> 4, l1, t1);

	f = lod & 0xf;
	c.R = MFilter1(f, t0.R, t1.R);
	c.G = MFilter1(f, t0.G, t1.G);
	c.B = MFilter1(f, t0.B, t1.B);
	c.A = MFilter1(f, t0.A, t1.A);
}

void
TXM::LMNFilter(NormTexCoord &u, NormTexCoord &v, int lod, PixColor &c)
{
	int l, mxl;

	l = (lod + 8) >> 4;
	mxl = attr.MXL;
	if (l > mxl)
		l = mxl;
	LFilter(u, v, l, c);
}

void
TXM::LMLFilter(NormTexCoord &u, NormTexCoord &v, int lod, PixColor &c)
{
	int l0, l1, f, mxl;
	PixColor t0, t1;

	l0 = lod >> 4;
	LFilter(u, v, l0, t0);
	l1 = l0 + 1;
	mxl = attr.MXL;
	if (l1 > mxl)
		l1 = mxl;
	LFilter(u, v, l1, t1);
	f = lod & 0xf;
	c.R = MFilter1(f, t0.R, t1.R);
	c.G = MFilter1(f, t0.G, t1.G);
	c.B = MFilter1(f, t0.B, t1.B);
	c.A = MFilter1(f, t0.A, t1.A);
}

/* ---- one fragment --------------------------------------------------- */

void
TXM::Texturing(Pixel &p, int lod, Gpu2RegFST fst)
{
	TexCoord u, v;
	PixColor t;

	u.TexDiv(p.m_1c, p.m_24, fst);
	v.TexDiv(p.m_20, p.m_24, fst);
	if (lod == 0) {
		if (attr.MMAG == 0)
			NFilter(u, v, 0, t);
		else
			LFilter(u, v, 0, t);
	} else {
		int mmin;

		mmin = attr.MMIN;
		switch (mmin) {
		case 0:
			NFilter(u, v, 0, t);
			break;
		case 1:
			LFilter(u, v, 0, t);
			break;
		case 2:
			NMNFilter(u, v, lod, t);
			break;
		case 3:
			NMLFilter(u, v, lod, t);
			break;
		case 4:
			LMNFilter(u, v, lod, t);
			break;
		case 5:
			LMLFilter(u, v, lod, t);
			break;
		default:
			fprintf(stderr, "Unknown filter mode\n");
			exit(1);
		}
	}
	texfunc.Func(t, p.c);
}

/* ---- LOD ------------------------------------------------------------ */

/* The correction the hardware's log2 table carries: the difference
 * between the exact log2 of the bucket and its linear approximation. */
static int
adjtbl(int i)
{
	float y;

	y = log(1 + i/128.0)/log(2.0);
	return (int)(y*128 + 0.5) - i;
}

static int
log2_main(unsigned int m)
{
	return m >> 8;
}

static int
log2_adj(unsigned int m)
{
	return adjtbl(m >> 8);
}

int
TXM::ComputeLod(PixelStamp &s)
{
	int lod;
	int q, m, b;
	int k, l, e;

	if (attr.LCM == 1)
		lod = attr.K;
	else {
		q = SearchQlevel(s);
		if (q == 1) {
			b = 0;
			m = 0;
		} else {
			int a;

			a = q >> 1;
			for (b = 15; b >= 0; b--)
				if (a & (1 << b))
					break;
			b = b < 0 ? 0 : b;
			m = ((q << (15 - b)) >> 1) & 0x7fff;
		}
		b--;
		k = attr.K;
		l = attr.L;
		e = s.m_2c - 141;
		b = (b*128 + log2_main(m)) << l >> 2;
		e = (e*128 + log2_adj(m)) << l >> 2;
		lod = k*2;
		lod -= e + b;
		lod >>= 1;
	}
	return ClampLod(lod);
}

/* ---- the stamp ------------------------------------------------------ */

void
TXM::Stamp(PixelStamp &s)
{
	int mask;
	int i;

	mask = s.mask;
	mask |= s.AAMask();
	if (s.m_30 == 1) {
		Gpu2RegFST fst;
		int lod;

		fst = (Gpu2RegFST)s.m_3c;
		lod = ComputeLod(s);
		for (i = 0; ; i++) {
			Pixel *p;

			if (i == 16)
				break;
			if (mask & (1 << i)) {
				p = &s.pix[i];
				Texturing(*p, lod, fst);
			}
		}
	}
	if (s.m_40 == 1)
		AA1(s);
	if (s.m_34 == 1)
		for (i = 0; ; i++) {
			Pixel *p;

			if (i == 16)
				break;
			if (mask & (1 << i)) {
				p = &s.pix[i];
				fog.Fogging(*p);
			}
		}
	if (s.m_40 != 0)
		s.ABE = 1;
	memif->Stamp(s);
}

/* ---- the DDA interface ---------------------------------------------- */

/* Colour, fog and Z leave the DDA as extended fixed point: four
 * fractional bits, then the value, then three bits that say whether the
 * interpolation ran off either end. */
inline int
ClampC(int v)
{
	int e, r, w;

	w = v >> 4;
	e = (v >> 11) & 7;
	if ((e != 2 && e != 3) & (e != 4)) {
		r = 0;
		if ((e != 6 && e != 7) & (e != 5))
			r = w & 0xff;
	} else
		r = 0xff;
	return r;
}

inline int
ClampZ(long long v)
{
	int e, r;
	long long w;

	w = v >> 4;
	e = (int)(v >> 35) & 7;
	if ((e != 2 && e != 3) & (e != 4)) {
		r = 0;
		if ((e != 6 && e != 7) & (e != 5))
			r = (int)w;
	} else
		r = 0xffffffff;
	return r;
}

/* Every field store in the stamp loops goes through its own `Pixel *p':
 * the 1998 object recomputes &s.pix[i] into a fresh pseudo for each one,
 * which is what a nested block gives and a shared pointer local does
 * not. */
#define SetPix(fld, val)	{			\
		Pixel *p = &s.pix[i];			\
							\
		p->fld = (val);				\
	}

void
TXM::Put(DDA *d)
{
	PixelStamp s;
	int i;
	int drdx, dgdx, dbdx, dadx, dfdx, dsdx, dtdx, dqdx;
	long long dzdx;
	int r0, g0, b0, a0, f0, s0, t0, q0;
	long long z0;
	int zc;

	s.m_30 = d->TME;
	s.m_34 = d->FGE;
	s.ABE = d->ABE;
	s.m_3c = d->FST;
	s.m_40 = d->AA1;
	s.ctxt = d->CTXT;
	s.m_20 = (d->mask >> 4) & 0xf0f;
	s.m_24 = (d->amask & 0xf) | ((d->amask & 0xf0) << 4);
	s.m_2c = d->maxexp;
	if (s.m_40 != 0) {
		s.aamask = s.m_20 | s.m_24;
		s.mask = d->mask & 0xf0f;
	} else {
		s.aamask = 0;
		s.mask = d->mask;
	}

	if (d->isreg) {
		switch (d->reg_addr) {
		case GS_TEX0_1:
			SetTEX0(0, d->reg_data);
			break;
		case GS_TEX0_2:
			SetTEX0(1, d->reg_data);
			break;
		case GS_TEX1_1:
			SetTEX1(0, d->reg_data);
			break;
		case GS_TEX1_2:
			SetTEX1(1, d->reg_data);
			break;
		case GS_TEX2_1:
			SetTEX2(0, d->reg_data);
			break;
		case GS_TEX2_2:
			SetTEX2(1, d->reg_data);
			break;
		case GS_TEXCLUT:
			SetTEXCLUT(d->reg_data);
			break;
		case GS_MIPTBP1_1:
			SetMIPTBP1(0, d->reg_data);
			break;
		case GS_MIPTBP1_2:
			SetMIPTBP1(1, d->reg_data);
			break;
		case GS_MIPTBP2_1:
			SetMIPTBP2(0, d->reg_data);
			break;
		case GS_MIPTBP2_2:
			SetMIPTBP2(1, d->reg_data);
			break;
		case GS_CLAMP_1:
			SetCLAMP(0, d->reg_data);
			break;
		case GS_CLAMP_2:
			SetCLAMP(1, d->reg_data);
			break;
		case GS_TEXA:
			SetTEXA(d->reg_data);
			break;
		case GS_FOGCOL:
			SetFOGCOL(d->reg_data);
			break;
		case GS_SCISSOR_1:
			SetSCISSOR(0, d->reg_data);
			break;
		case GS_SCISSOR_2:
			SetSCISSOR(1, d->reg_data);
			break;
		case GS_FRAME_1:
			SetFRAME(0, d->reg_data);
			s.type = 1;
			s.reg = d->reg_addr;
			s.data = d->reg_data;
			memif->Stamp(s);
			break;
		case GS_FRAME_2:
			SetFRAME(1, d->reg_data);
			s.type = 1;
			s.reg = d->reg_addr;
			s.data = d->reg_data;
			memif->Stamp(s);
			break;
		case GS_SCANMSK:
			break;
		case GS_PRIM:
		case GS_PRMODE:
			SetContext((Gpu2RegCtxt)d->CTXT);
			/* fall through: PRIM/PRMODE are forwarded too */
		default:
			s.type = 1;
			s.reg = d->reg_addr;
			s.data = d->reg_data;
			memif->Stamp(s);
			break;
		}
	} else {
		if (s.m_40) {
			aa.Set(d);
			ExtCov(s);
		}
		drdx = d->drdx;
		dgdx = d->dgdx;
		dbdx = d->dbdx;
		dadx = d->dadx;
		dfdx = d->dfdx;
		dsdx = d->dsdx;
		dtdx = d->dtdx;
		dqdx = d->dqdx;
		dzdx = d->dzdx;
		r0 = d->r0;
		g0 = d->g0;
		b0 = d->b0;
		a0 = d->a0;
		f0 = d->f0;
		s0 = d->s0;
		t0 = d->t0;
		q0 = d->q0;
		z0 = d->z0;
		zc = d->zc;
		for (i = 0; ; i++) {
			int k;

			if (i == 8)
				break;
			k = i - 2;
			SetPix(c.R, ClampC(drdx*k + r0));
			SetPix(c.G, ClampC(dgdx*k + g0));
			SetPix(c.B, ClampC(dbdx*k + b0));
			SetPix(c.A, ClampC(dadx*k + a0));
			SetPix(m_28, ClampC(dfdx*k + f0));
			SetPix(m_1c, ClampT(dsdx*k + s0));
			SetPix(m_20, ClampT(dtdx*k + t0));
			SetPix(m_24, ClampQ(dqdx*k + q0));
			if (i <= 3) {
				SetPix(z, ClampZ(z0 + (dzdx*k >> 2)));
			} else {
				SetPix(z, ClampZ(z0 + zc + (dzdx*k >> 2)));
			}
		}
		r0 = d->r1;
		g0 = d->g1;
		b0 = d->b1;
		a0 = d->a1;
		f0 = d->f1;
		s0 = d->s1;
		t0 = d->t1;
		q0 = d->q1;
		z0 = d->z1;
		for (; ; i++) {
			int k;

			if (i == 16)
				break;
			k = i - 10;
			SetPix(c.R, ClampC(drdx*k + r0));
			SetPix(c.G, ClampC(dgdx*k + g0));
			SetPix(c.B, ClampC(dbdx*k + b0));
			SetPix(c.A, ClampC(dadx*k + a0));
			SetPix(m_28, ClampC(dfdx*k + f0));
			SetPix(m_1c, ClampT(dsdx*k + s0));
			SetPix(m_20, ClampT(dtdx*k + t0));
			SetPix(m_24, ClampQ(dqdx*k + q0));
			if (i <= 11) {
				SetPix(z, ClampZ(z0 + (dzdx*k >> 2)));
			} else {
				SetPix(z, ClampZ(z0 + zc + (dzdx*k >> 2)));
			}
		}
		s.type = 0;
		s.pos.x = d->px;
		s.pos.y = d->py;
		Stamp(s);
	}
}

/* ---- antialiasing --------------------------------------------------- */

/* Re-walk the DDA's per-edge coverage across the stamp and leave the
 * result in each pixel; AA1() then substitutes it for alpha. */
void
TXM::ExtCov(PixelStamp &s)
{
	const DDA *d;
	int i;
	int m, am;
	int c0, c1, dc0, dc1;

	d = aa.dda;
	m = (d->mask >> 4) & 0xf0f;
	am = (d->amask & 0xf) | ((d->amask & 0xf0) << 4);
	c0 = d->cova0;
	c1 = d->covb0;
	dc0 = d->covdxa;
	dc1 = d->covdxb0;
	for (i = 0; ; i++) {
		int bit, t, v, w, e, r;

		if (i > 7)
			break;
		bit = 1 << i;
		t = am & bit;
		if (m & bit)
			v = c0 + (i - 2)*dc0;
		else if (t)
			v = c1 + (i - 2)*dc1;
		else
			v = 0x800;
		w = v >> 4;
		e = (v >> 10) & 3;
		if (e != 2) {
			r = 0;
			if (e != 3)
				r = (unsigned char)w;
		} else
			r = 0x80;
		s.pix[i].m_2c = r;
	}
	c0 = d->cova1;
	c1 = d->covb1;
	dc1 = d->covdxb1;
	for (i = 8; ; i++) {
		int bit, t, v, w, e, r;

		if (i > 15)
			break;
		bit = 1 << i;
		t = am & bit;
		if (m & bit)
			v = c0 + (i - 10)*dc0;
		else if (t)
			v = c1 + (i - 10)*dc1;
		else
			v = 0x800;
		w = v >> 4;
		e = (v >> 10) & 3;
		if (e != 2) {
			r = 0;
			if (e != 3)
				r = (unsigned char)w;
		} else
			r = 0x80;
		s.pix[i].m_2c = r;
	}
}

/* AA1: the coverage becomes the alpha of the pixels on the edge. */
void
TXM::AA1(PixelStamp &s)
{
	int abe;
	int i;

	abe = s.ABE;
	for (i = 0; ; i++) {
		Pixel *p;

		if (i > 3)
			break;
		if (abe == 0 || s.pix[i].c.A == 0x80) {
			p = &s.pix[i];
			p->c.A = p->m_2c;
		}
	}
	for (i = 8; ; i++) {
		Pixel *p;

		if (i > 11)
			break;
		if (abe == 0 || s.pix[i].c.A == 0x80) {
			p = &s.pix[i];
			p->c.A = p->m_2c;
		}
	}
}

/* ---- MTBA ----------------------------------------------------------- */

/* TEX1.MTBA: derive TBP1..TBP3 and TBW1..TBW3 from TBP0 and the texture
 * size instead of taking them from MIPTBP1. */
void
TexAttr::MipTbpAuto(void)
{
	int i;
	int w, d;

	if (W != H) {
		fprintf(stderr,
			"Width must be same as Height when MTBA mode\n");
		exit(1);
	}
	if (W <= 31) {
		fprintf(stderr,
			"Width and Height must be greter 32 when MTBA mode\n");
		exit(1);
	}
	if (PSM == PSMCT32 || PSM == PSMCT24 || PSM == PSMT8H ||
	    PSM == PSMT4HL || PSM == PSMT4HH) {
		w = W;
		for (i = 1; i != 4; i++) {
			if (w > 15)
				d = (w/16)*((w/16) << 8);
			else
				d = 0x40;
			TBP[i] = TBP[i-1] + d;
			w >>= 1;
			if (w < 64)
				TBW[i] = 64;
			else
				TBW[i] = w;
		}
	} else if (PSM == PSMCT16) {
		w = W;
		for (i = 1; i != 4; i++) {
			if (w > 15)
				d = (w/16)*((w/16) << 7);
			else
				d = 0x40;
			TBP[i] = TBP[i-1] + d;
			w >>= 1;
			if (w < 64)
				TBW[i] = 64;
			else
				TBW[i] = w;
		}
	} else if (PSM == PSMT8) {
		w = W;
		for (i = 1; i != 4; i++) {
			if (w > 15)
				d = (w/16)*((w/16) << 6);
			else
				d = 0x40;
			TBP[i] = TBP[i-1] + d;
			w >>= 1;
			if (w < 64)
				TBW[i] = 64;
			else
				TBW[i] = w;
		}
	} else if (PSM == PSMT4) {
		w = W;
		for (i = 1; i != 4; i++) {
			if (w > 31)
				d = (w/32)*((w/32) << 7);
			else
				d = 0x40;
			TBP[i] = TBP[i-1] + d;
			w >>= 1;
			if (w < 64)
				TBW[i] = 64;
			else
				TBW[i] = w;
		}
	} else
		fprintf(stderr, "MTBA::PSM [%d] is invalid.\n", PSM);
}

void
AA::Set(const DDA *d)
{
	dda = d;
}

void
Fog::Fogging(Pixel &p)
{
	int f, g;
	int cr, cg, cb;

	f = p.m_28;
	g = 256 - f;
	cr = f*p.c.R + g*R;
	cg = f*p.c.G + g*G;
	cb = f*p.c.B + g*B;
	p.c.R = cr >> 8;
	p.c.G = cg >> 8;
	p.c.B = cb >> 8;
}

/* Defined here, after every use, so that the uses above compile to a
 * call and the body comes out last in .text as this object's only weak
 * symbol - which is what txm.o has (see doc/notes/memif.md). */
inline int
PixelStamp::AAMask(void) const
{
	return aamask;
}
