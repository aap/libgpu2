#include <stdio.h>
#include <stdlib.h>

#include "gsdefs.h"
#include "memif.h"

/* Load the live copy of every per-context unit.  Written out at each use
 * (memif.o has ten copies of it, all with the same shape), so it is a
 * macro here; Context() below is the out-of-line one MemIF's clients
 * call.
 *
 * The two 8-byte units are ternaries and the 0x10- and 0x18-byte ones
 * if/else, and the bytes insist on it: an 8-byte record has DImode, so a
 * ternary loads both words in each arm and shares one store pair, while
 * if/else stores in each arm.  doc/notes/memif.md. */
#define CONTEXT()			\
	if (ctxt == 0)			\
		atest = atestc[0];	\
	else				\
		atest = atestc[1];	\
	datest = ctxt == 0 ? datestc[0] : datestc[1];	\
	ztest = ctxt == 0 ? ztestc[0] : ztestc[1];	\
	if (ctxt == 0)			\
		blend = blendc[0];	\
	else				\
		blend = blendc[1];

#define SET_ALPHA(b)			\
	(b).A = data & 3;		\
	(b).B = (data >> 2) & 3;	\
	(b).C = (data >> 4) & 3;	\
	(b).D = (data >> 6) & 3;	\
	(b).FIX = (data >> 32) & 0xff;

#define SET_ATEST(a)			\
	(a).ATE = data & 1;		\
	(a).ATST = (data >> 1) & 7;	\
	(a).AFAIL = (data >> 12) & 3;	\
	(a).AREF = (data >> 4) & 0xff;

#define SET_DATEST(d)			\
	(d).DATE = (data >> 14) & 1;	\
	(d).DATM = (data >> 15) & 1;

#define SET_ZTEST(z)			\
	(z).ZTE = (data >> 16) & 1;	\
	(z).ZTST = (data >> 17) & 3;

#define SET_TEST(a, d, z)		\
	SET_ATEST(a)			\
	SET_DATEST(d)			\
	SET_ZTEST(z)

/* One 3-bit signed DIMX field. */
#define SET_DIMX(dst, sh)		\
	t = (data >> (sh)) & 7;		\
	u = t & 4 ? t | ~7 : t;		\
	(dst) = u;

#define SET_DIMX_ALL()			\
	SET_DIMX(dither.mat[0][0], 0)	\
	SET_DIMX(dither.mat[0][1], 4)	\
	SET_DIMX(dither.mat[0][2], 8)	\
	SET_DIMX(dither.mat[0][3], 12)	\
	SET_DIMX(dither.mat[1][0], 16)	\
	SET_DIMX(dither.mat[1][1], 20)	\
	SET_DIMX(dither.mat[1][2], 24)	\
	SET_DIMX(dither.mat[1][3], 28)	\
	SET_DIMX(dither.mat[2][0], 32)	\
	SET_DIMX(dither.mat[2][1], 36)	\
	SET_DIMX(dither.mat[2][2], 40)	\
	SET_DIMX(dither.mat[2][3], 44)	\
	SET_DIMX(dither.mat[3][0], 48)	\
	SET_DIMX(dither.mat[3][1], 52)	\
	SET_DIMX(dither.mat[3][2], 56)	\
	SET_DIMX(dither.mat[3][3], 60)

/* The alpha test itself, written as AREF <op> A. */
int
AlphaTest::Pass(int a)
{
	if (ATST == 0)
		return 0;
	if (ATST == 1)
		return 1;
	if (ATST == 2)
		return AREF > a;
	if (ATST == 3)
		return AREF >= a;
	if (ATST == 4)
		return AREF == a;
	if (ATST == 5)
		return AREF <= a;
	if (ATST == 6)
		return AREF < a;
	if (ATST == 7)
		return AREF != a;
	fprintf(stderr, "Illegal alpha test function\n");
	exit(0);
}

void
AlphaTest::ATest(PixelStamp &s)
{
	int i, mask;

	mask = s.mask;
	if (ATE == 0) {
		i = 0;
		for (;;) {
			Pixel *p;

			if (i == 16)
				break;
			p = &s.pix[i];
			p->pass = 1;
			i++;
		}
		return;
	}
	i = 0;
	for (;;) {
		Pixel *p;
		int a;

		if (i == 16)
			break;
		if (mask & 1<<i) {
			p = &s.pix[i];
			a = p->c.A;
			if (Pass(a))
				p->pass = 1;
			else {
				p->pass = 0;
				p->afail = AFAIL;
			}
		}
		i++;
	}
}

void
DAlphaTest::DATest(Memory *mem, PixelStamp &s)
{
	PixelStamp t;
	int i, mask;

	if (DATE == 0)
		return;
	if (Depth(mem->fb.PSM) == 24) {
		if (DATM == 0)
			s.mask = 0;
		return;
	}
	t.pos = s.pos;
	t.mask = s.mask;
	mem->fb.ReadStamp(mem, t);
	mask = s.mask;
	i = 0;
	for (;;) {
		int a, pass;

		if (i == 16)
			break;
		if (mask & 1<<i) {
			a = t.pix[i].c.A;
			if (DATM == 0)
				pass = ((unsigned)a >> 7 ^ 1) & 1;
			else
				pass = (a >> 7) & 1;
			if (pass == 0)
				mask ^= 1<<i;
		}
		i++;
	}
	s.mask = mask;
}

void
DepthTest::ZTest(Memory *mem, PixelStamp &s)
{
	PixelStamp t;
	int i, mask;

	i = 0;
	for (;;) {
		Pixel *p;
		unsigned z;

		if (i > 15)
			break;
		p = &s.pix[i];
		z = p->z < mem->zb.mask ? p->z : mem->zb.mask;
		p->z = z;
		i++;
	}
	if (ZTE == 0)
		return;
	if (ZTST == 1) {
		i = 0;
		for (;;) {
			Pixel *p;

			if (i > 15)
				break;
			p = &s.pix[i];
			p->z = p->z;
			i++;
		}
		return;
	}
	if (ZTST == 0) {
		s.mask = 0;
		return;
	}
	t.pos = s.pos;
	t.mask = s.mask;
	mem->zb.ReadStamp(mem, t);
	mask = s.mask;
	if (ZTST == 2) {
		i = 0;
		for (;;) {
			Pixel *p;

			if (i == 16)
				break;
			if (mask & 1<<i) {
				if (s.pix[i].z < t.pix[i].z)
					mask ^= 1<<i;
				else {
					p = &s.pix[i];
					p->z = p->z;
				}
			}
			i++;
		}
	} else {
		i = 0;
		for (;;) {
			Pixel *p;

			if (i == 16)
				break;
			if (mask & 1<<i) {
				if (s.pix[i].z <= t.pix[i].z)
					mask ^= 1<<i;
				else {
					p = &s.pix[i];
					p->z = p->z;
				}
			}
			i++;
		}
	}
	s.mask = mask;
}

void
AlphaBlend::Blend(Memory *mem, PixelStamp &s)
{
	PixColor ca, cb, cd;
	PixelStamp t;
	int i, mask;

	if (s.ABE == 0)
		return;
	t.pos = s.pos;
	t.mask = s.mask;
	mem->fb.ReadStamp(mem, t);
	mask = s.mask;
	for (i = 0; i != 16; i++) {
		int alpha;

		if (mask & 1<<i && (PABE != 1 || s.pix[i].c.A & 0x80)) {
			if (A == 0)
				ca = s.pix[i].c;
			else if (A == 1)
				ca = t.pix[i].c;
			else {
				ca.R = 0;
				ca.G = 0;
				ca.B = 0;
				ca.A = 0;
			}
			if (B == 0)
				cb = s.pix[i].c;
			else if (B == 1)
				cb = t.pix[i].c;
			else {
				cb.R = 0;
				cb.G = 0;
				cb.B = 0;
				cb.A = 0;
			}
			if (D == 0)
				cd = s.pix[i].c;
			else if (D == 1)
				cd = t.pix[i].c;
			else {
				cd.R = 0;
				cd.G = 0;
				cd.B = 0;
				cd.A = 0;
			}
			if (C == 0)
				alpha = s.pix[i].c.A;
			else if (C == 1)
				alpha = t.pix[i].c.A;
			else
				alpha = FIX;
			s.pix[i].c.R = ((ca.R - cb.R) * alpha >> 7) + cd.R;
			s.pix[i].c.G = ((ca.G - cb.G) * alpha >> 7) + cd.G;
			s.pix[i].c.B = ((ca.B - cb.B) * alpha >> 7) + cd.B;
		}
	}
}

void
Dither::Dithering(PixelStamp &s)
{
	int i, x, x0, y, xstep, mask;

	if (DTHE == 0)
		return;
	x0 = s.pos.x;
	x = x0;
	y = s.pos.y*2;
	xstep = 1;
	if (x & 1)
		xstep = -1;
	mask = s.mask;
	i = 0;
	for (;;) {
		if (i == 8)
			break;
		if (mask & 1<<i) {
			s.pix[i].c.R += mat[y%4][x%4];
			s.pix[i].c.G += mat[y%4][x%4];
			s.pix[i].c.B += mat[y%4][x%4];
		}
		i++;
		x += xstep;
	}
	x = x0;
	y++;
	for (;;) {
		if (i == 16)
			break;
		if (mask & 1<<i) {
			s.pix[i].c.R += mat[y%4][x%4];
			s.pix[i].c.G += mat[y%4][x%4];
			s.pix[i].c.B += mat[y%4][x%4];
		}
		i++;
		x += xstep;
	}
}

void
ColorClamp::Clamp(PixelStamp &s)
{
	int i, mask;

	mask = s.mask;
	if (CLAMP == 1) {
		i = 0;
		for (;;) {
			int v;

			if (i == 16)
				break;
			if (mask & 1<<i) {
				v = s.pix[i].c.R;
				if (v < 0)
					v = 0;
				else if (v > 255)
					v = 255;
				s.pix[i].c.R = v;
				v = s.pix[i].c.G;
				if (v < 0)
					v = 0;
				else if (v > 255)
					v = 255;
				s.pix[i].c.G = v;
				v = s.pix[i].c.B;
				if (v < 0)
					v = 0;
				else if (v > 255)
					v = 255;
				s.pix[i].c.B = v;
			}
			i++;
		}
	} else {
		i = 0;
		for (;;) {
			Pixel *p;

			if (i == 16)
				break;
			if (mask & 1<<i) {
				p = &s.pix[i];
				p->c.R = p->c.R & 0xff;
				p->c.G = p->c.G & 0xff;
				p->c.B = p->c.B & 0xff;
			}
			i++;
		}
	}
}

/* The stage's one virtual: either a stamp of pixels or a register write
 * travelling down the pipe. */
void
MemIF::Stamp(PixelStamp &s)
{
	int reg;
	long long data;
	int t, u;

	if (s.type != 0) {
		reg = s.reg;
		data = s.data;
		switch (reg) {
		case GS_ALPHA_1:
			SET_ALPHA(blendc[0])
			CONTEXT()
			break;

		case GS_ALPHA_2:
			SET_ALPHA(blendc[1])
			CONTEXT()
			break;

		case GS_DIMX:
			SET_DIMX_ALL()
			break;

		case GS_DTHE:
			dither.DTHE = data & 1;
			break;

		case GS_COLCLAMP:
			clamp.CLAMP = data & 1;
			break;

		case GS_TEST_1:
			SET_TEST(atestc[0], datestc[0], ztestc[0])
			CONTEXT()
			mem->SetRegister(reg, data);
			break;

		case GS_TEST_2:
			SET_TEST(atestc[1], datestc[1], ztestc[1])
			CONTEXT()
			mem->SetRegister(reg, data);
			break;

		case GS_PABE:
			blendc[0].PABE = data & 1;
			blendc[1].PABE = data & 1;
			CONTEXT()
			break;

		case GS_PRIM:
		case GS_PRMODE:
			ctxt = s.ctxt;
			CONTEXT()
			mem->SetRegister(reg, data);
			break;

		default:
			mem->SetRegister(reg, data);
		}
	} else {
		s.mask = s.mask | s.AAMask();
		atest.ATest(s);
		datest.DATest(mem, s);
		ztest.ZTest(mem, s);
		blend.Blend(mem, s);
		dither.Dithering(s);
		clamp.Clamp(s);
		mem->fb.WriteStamp(mem, s);
		if (ztest.ZTE == 1)
			mem->zb.WriteStamp(mem, s);
	}
}

int
MemIF::ReadWord(int i)
{
	return mem->vram[i];
}

MemIF::MemIF(Memory *m)
{
	mem = m;
}

void
MemIF::SetContext(Gpu2RegCtxt c)
{
	ctxt = c;
	CONTEXT()
}

void
MemIF::SetPABE(long long data)
{
	blendc[0].PABE = data & 1;
	blendc[1].PABE = data & 1;
	CONTEXT()
}

void
MemIF::SetCOLCLAMP(long long data)
{
	clamp.CLAMP = data & 1;
}

void
MemIF::SetDTHE(long long data)
{
	dither.DTHE = data & 1;
}

void
MemIF::SetDIMX(long long data)
{
	int t, u;

	SET_DIMX_ALL()
}

void
MemIF::SetALPHA(int ctx, long long data)
{
	AlphaBlend *b;

	b = &blendc[ctx];
	SET_ALPHA((*b))
	CONTEXT()
}

void
MemIF::SetTEST(int ctx, long long data)
{
	AlphaTest *a;
	DAlphaTest *d;
	DepthTest *z;

	a = &atestc[ctx];
	SET_ATEST((*a))
	d = &datestc[ctx];
	SET_DATEST((*d))
	z = &ztestc[ctx];
	SET_ZTEST((*z))
	CONTEXT()
}

void
MemIF::Context()
{
	CONTEXT()
}

/* Defined here, after every use, so the calls in Stamp() stay calls and
 * the body comes out last in .text - which is what memif.o (and txm.o)
 * carry: one weak `AAMask__C10PixelStamp'. */
inline int
PixelStamp::AAMask() const
{
	return aamask;
}
