/* DDA - the GS scanline rasterizer.  See include/dda.h for the class
 * layout and doc/notes/dda.md for the algorithm, the fixed-point
 * formats and the compiler lessons.
 *
 * Several spellings here are pinned by the bytes and must not be
 * "simplified": see the comments at each of them.
 *
 * abs() is declared here rather than pulled from <stdlib.h>, but -
 * unlike pre3.c and pcalc.c - dda.o wants the era libc5 __const__
 * attribute: the 1998 object pops each abs() argument immediately
 * after the call instead of deferring it.
 */

#include "dda.h"

extern "C" int abs(int) __attribute__((__const__));

/* Sign-extend an n-bit field held in the low bits of v. */
static int
sign_extent(int v, int n)
{
	int r;

	if ((v >> (n-1) & 1) == 0)
		r = ((1 << n) - 1) & v;
	else
		r = (((1 << (32-n)) - 1) << n) | v;
	return r;
}

static long long
sign_extent(long long v, int n)
{
	long long t, r;

	t = (v >> (n-1)) & 1;
	if (t == 0) {
		long long m = ((long long)1 << n) - 1;
		r = v & m;
	} else
		r = v | ((((long long)1 << (32-n)) - 1) << n);
	return r;
}

static long long
ExtZslope(long long v)
{
	return sign_extent((v & 0xfffffffffffLL) >> 4, 40);
}

static long long
ExtZvalue(long long v)
{
	return sign_extent((v & 0xfffffffffffLL) >> 6, 38);
}

static int
ExtCvalue(int v)
{
	v = v & 0xfffff;
	return sign_extent(v >> 6, 14);
}

static int
ExtCovvalue(int v)
{
	v = v & 0x3ffff;
	return sign_extent(v >> 6, 12);
}

static int
ExtTvalue(int v)
{
	v = v & 0xfffffff;
	return sign_extent(v, 28);
}

static int
IsMinusDCDX(long v)
{
	return (unsigned long)v >> 19 & 1;
}

static int
ExtCslope(int v)
{
	int r;

	r = v >> 6;
	if (IsMinusDCDX(v) && (v & 0x3f))
		r++;
	return sign_extent(r, 14);
}

static int
IsMinusDZDX(long long v)
{
	return (v & 0x80000000000LL) != 0;
}

/* Add one scanline's worth of slope to a Z value, rounding towards
 * zero: the low 6 bits are dropped, and a negative slope with a
 * non-zero remainder rounds up. */
static long long
ZaddSpan(long long v, long long d)
{
	long long r;

	if (IsMinusDZDX(d)) {
		r = (v >> 6) + (d >> 6);
		if (d & 0x3f)
			r++;
	} else
		r = (v >> 6) + (d >> 6);
	return r << 6;
}

static int
CaddSpan(int v, int d)
{
	int r;

	r = d >> 6;
	if (IsMinusDCDX(d) && (d & 0x3f))
		r++;
	r = r + (v >> 6);
	return r << 6;
}

/* Everything that stays constant over the whole primitive. */
void
DDA::InitStamp(void)
{
	if (IsMinusDZDX(pcalc->ozdx)) {
		dzdx = ExtZslope(pcalc->ozdx);
		if (pcalc->ozdx & 0xf)
			dzdx++;
	} else
		dzdx = ExtZslope(pcalc->ozdx);
	dadx = ExtCslope(pcalc->oadx);
	dbdx = ExtCslope(pcalc->obdx);
	dgdx = ExtCslope(pcalc->ogdx);
	drdx = ExtCslope(pcalc->ordx);
	dsdx = ExtTvalue(pcalc->osdx);
	dtdx = ExtTvalue(pcalc->otdx);
	dqdx = ExtTvalue(pcalc->oqdx);
	dfdx = ExtCslope(pcalc->ofdx);
	/* declared here, not at the top: the calls above reload
	 * this->pcalc every time, this block keeps it in a register */
	PCalc *p = pcalc;
	ydir = p->ydir;
	TME = p->TME;
	FGE = p->FGE;
	ABE = p->ABE;
	FST = p->FST;
	AA1 = p->AA1;
	maxexp = p->maxexp;
	CTXT = p->CTXT;
	if (p->AA1)
		covdxa = ExtCovvalue(p->covdx[0]);
	else {
		cova0 = covb0 = cova1 = covb1 = 0;
		covdxa = covdxb0 = covdxb1 = 0;
		esel0 = esel1 = amask = 0;
	}
}

/* SCANMSK: 0/1 draw every scanline, 2 only even ones, 3 only odd. */
static int
Scnmsk(int msk, int y)
{
	if (msk == 0 || msk == 1)
		return 1;
	if ((msk & 2) && !(msk & 1))
		return y & 1;
	if ((msk & 2) && (msk & 1))
		return ~y & 1;
	return 0;
}

static int
isbbox(int l, int r, int t, int b)
{
	return l >= 0 && r >= 0 && t >= 0 && b >= 0;
}

static int
isvld(int e0, int e1, int e2, int bbox, int msk)
{
	return e0 >= 0 && e1 >= 0 && e2 >= 0 && bbox > 0 && msk != 0;
}

static int
isaps(int a, int e0, int e1, int e2, int n, int bbox, int msk)
{
	return a >= 0 && e0 < 0 && (n >= 0 ? e1 >= 0 : e2 >= 0) &&
		bbox > 0 && msk != 0;
}

static int
isape_1(int a, int b, int e0, int e1, int e2, int n, int bbox, int msk)
{
	return ((n >= 0 && e1 < 0 && a >= 0 && e2 >= 0) ||
		(n < 0 && e2 < 0 && b >= 0 && e1 >= 0)) &&
		e0 >= 0 && bbox > 0 && msk != 0;
}

/* Evaluate one 2x{4,8} stamp at the current position: which pixels are
 * inside, and the interpolated values of both scanlines.  Returns the
 * pixel mask, zero if the stamp is entirely outside. */
int
DDA::Stamping(int n)
{
	int e0, e1, e2;
	int xl, xr;
	int nn;
	int bok;
	int bt1, bb1;			/* the bbox arguments are computed
					 * into locals before `nn' - the
					 * 1998 object orders them that way */
	PCalc *p;
	int sw;
	int mska, mskb;
	int apsa, apsb;
	int apea, apeb;
	int ea0, ea1, ea2;
	int aa0, aa1, aa2;
	int i, msk;

	p = pcalc;
	isreg = 0;
	px = v.x;
	py = y >> 1;
	if (p->ydir == 0) {
		z0 = ExtZvalue(v.z);
		f0 = ExtCvalue(v.f);
		a0 = ExtCvalue(v.a);
		b0 = ExtCvalue(v.b);
		g0 = ExtCvalue(v.g);
		r0 = ExtCvalue(v.r);
		s0 = ExtTvalue(v.s);
		t0 = ExtTvalue(v.t);
		q0 = ExtTvalue(v.q);
		z1 = ExtZvalue(ZaddSpan(v.z, p->ozdy));
		f1 = ExtCvalue(CaddSpan(v.f, p->ofdy));
		a1 = ExtCvalue(CaddSpan(v.a, p->oady));
		b1 = ExtCvalue(CaddSpan(v.b, p->obdy));
		g1 = ExtCvalue(CaddSpan(v.g, p->ogdy));
		r1 = ExtCvalue(CaddSpan(v.r, p->ordy));
		s1 = ExtTvalue(v.s + p->osdy);
		t1 = ExtTvalue(v.t + p->otdy);
		q1 = ExtTvalue(v.q + p->oqdy);
	} else {
		z1 = ExtZvalue(v.z);
		f1 = ExtCvalue(v.f);
		a1 = ExtCvalue(v.a);
		b1 = ExtCvalue(v.b);
		g1 = ExtCvalue(v.g);
		r1 = ExtCvalue(v.r);
		s1 = ExtTvalue(v.s);
		t1 = ExtTvalue(v.t);
		q1 = ExtTvalue(v.q);
		z0 = ExtZvalue(ZaddSpan(v.z, p->ozdy));
		f0 = ExtCvalue(CaddSpan(v.f, p->ofdy));
		a0 = ExtCvalue(CaddSpan(v.a, p->oady));
		b0 = ExtCvalue(CaddSpan(v.b, p->obdy));
		g0 = ExtCvalue(CaddSpan(v.g, p->ogdy));
		r0 = ExtCvalue(CaddSpan(v.r, p->ordy));
		s0 = ExtTvalue(v.s + p->osdy);
		t0 = ExtTvalue(v.t + p->otdy);
		q0 = ExtTvalue(v.q + p->oqdy);
	}
	if (IsMinusDZDX(p->ozdx)) {
		if ((p->ozdx & 0xf) == 0)
			zc = 0;
		else
			zc = ((int)v.z & 0x3f) +
				((int)p->ozdx*4 & 0x3c) > 0x3f ? 0 : -1;
	} else
		zc = ((int)v.z & 0x3f) + ((int)p->ozdx*4 & 0x3c) > 0x3f;

	if (pcalc->AA1) {
		/* Three spellings here are byte-forced.  `? 1 : 0' makes
		 * gcc flush the pending argument pop before the statement
		 * instead of after it, and everything downstream is
		 * allocated around that.  The block-scoped `sel' is what
		 * turns the first coverage test into `test %reg,%reg' -
		 * reading `esel0' back would emit a memory compare, because
		 * the store to `esel1' has already killed gcc's CSE entry
		 * for it.  And `& 0xffffffc0' compiles to `andb $0xc0': it
		 * is a 32-bit mask, not a byte one. */
		if (pcalc->ydir == 0) {
			int sel;
			cova0 = ExtCovvalue(v.cov[0]);
			cova1 = ExtCovvalue((p->covdy[0] & 0xffffffc0) + v.cov[0]);
			esel0 = sel = yn + 1 < 0 ? 1 : 0;
			esel1 = yn < 0 ? 1 : 0;
			covb0 = ExtCovvalue(sel == 0 ? v.cov[1] : v.cov[2]);
			covb1 = ExtCovvalue(esel1 == 0 ?
				(p->covdy[1] & 0xffffffc0) + v.cov[1] :
				(p->covdy[2] & 0xffffffc0) + v.cov[2]);
			covdxb0 = ExtCovvalue(esel0 == 0 ?
				p->covdx[1] : p->covdx[2]);
			covdxb1 = ExtCovvalue(esel1 == 0 ?
				p->covdx[1] : p->covdx[2]);
		} else {
			int sel;
			cova0 = ExtCovvalue((p->covdy[0] & 0xffffffc0) + v.cov[0]);
			cova1 = ExtCovvalue(v.cov[0]);
			esel0 = sel = yn < 0 ? 1 : 0;
			esel1 = yn + 1 < 0 ? 1 : 0;
			covb0 = ExtCovvalue(sel == 0 ?
				(p->covdy[1] & 0xffffffc0) + v.cov[1] :
				(p->covdy[2] & 0xffffffc0) + v.cov[2]);
			covb1 = ExtCovvalue(esel1 == 0 ? v.cov[1] : v.cov[2]);
			covdxb0 = ExtCovvalue(esel0 == 0 ?
				p->covdx[1] : p->covdx[2]);
			covdxb1 = ExtCovvalue(esel1 == 0 ?
				p->covdx[1] : p->covdx[2]);
		}
	}

	sw = 4;
	if (p->flat)
		sw = 8;
	if (pcalc->AA1) {
		aa0 = abs((pcalc->steep[0] == 0 ?
			pcalc->ddy[0] : pcalc->ddx[0]) * 2);
		aa1 = abs((pcalc->steep[1] == 0 ?
			pcalc->ddy[1] : pcalc->ddx[1]) * 2);
		aa2 = abs((pcalc->steep[2] == 0 ?
			pcalc->ddy[2] : pcalc->ddx[2]) * 2);
		ea0 = v.e[0] - aa0;
		ea1 = v.e[1] - aa1;
		ea2 = v.e[2] - aa2;
	} else {
		aa0 = aa1 = aa2 = 0;
		ea0 = v.e[0];
		ea1 = v.e[1];
		ea2 = v.e[2];
	}
	mska = mskb = 0;
	apsa = apsb = 0;
	apea = apeb = 0;
	for (i = 0; i < sw; i++) {
		e0 = ea0 - ((sw - (i+1))*p->ddx[0] + p->ddy[0]*2)*2;
		e1 = ea1 - ((sw - i)*p->ddx[1] + p->ddy[1])*2;
		e2 = ea2 - (p->ddy[2] >= 0 ?
			(sw - i)*p->ddx[2] + p->ddy[2] :
			(sw - i)*p->ddx[2])*2;
		xl = v.el - (sw - (i+1));
		xr = v.er + (sw - i);
		bt1 = bt - 1;
		bb1 = bb + 2;
		nn = yn + 1;
		bok = isbbox(xl, xr, bt1, bb1);
		msk = Scnmsk(p->SCANMSK, p->ydir == 0 ? y : y + 1);
		mska |= isvld(e0, e1, e2, bok, msk) << i;
		if (pcalc->AA1) {
			apsa |= isaps(e0 + aa0, e0, e1, e2, nn, bok, msk) << i;
			apea |= isape_1(e1 + aa1, e2 + aa2, e0, e1, e2,
				nn, bok, msk) << i;
		}

		e0 = ea0 - ((sw - (i+1))*p->ddx[0] + p->ddy[0])*2;
		e1 = ea1 - ((sw - i)*p->ddx[1])*2;
		e2 = ea2 - (p->ddy[2] >= 0 ?
			(sw - i)*p->ddx[2] :
			(sw - i)*p->ddx[2] - p->ddy[2])*2;
		bt1 = bt;
		bb1 = bb + 1;
		nn = yn;
		bok = isbbox(xl, xr, bt1, bb1);
		msk = Scnmsk(p->SCANMSK, p->ydir == 0 ? y + 1 : y);
		mskb |= isvld(e0, e1, e2, bok, msk) << i;
		if (pcalc->AA1) {
			apsb |= isaps(e0 + aa0, e0, e1, e2, nn, bok, msk) << i;
			apeb |= isape_1(e1 + aa1, e2 + aa2, e0, e1, e2,
				nn, bok, msk) << i;
		}
	}
	if (p->ydir == 0)
		mask = (mskb << 8) | mska;
	else
		mask = (mska << 8) | mskb;
	if (AA1) {
		if (p->ydir == 0) {
			mask = (apsa << 4 | mska) | (apsb << 12 | mskb << 8);
			amask = apea | (apeb << 4);
		} else {
			mask = (apsb << 4 | mskb) | (apsa << 12 | mska << 8);
			amask = apeb | (apea << 4);
		}
	} else
		amask = 0;
	if (n == 0 && (mask | amask) != 0)
		first = 1;
	else
		first = 0;
	return mask | amask;
}

/* Load the DDA start values from PCalc and build the x and y steps. */
void
DDA::InitWalk(void)
{
	PCalc *p, *q;			/* two, not one: the struct copy
					 * below ends gcc's CSE region */
	int sw;

	p = pcalc;
	y = p->dday + p->dday;
	bt = p->bbt;
	bb = p->bbb;
	yn = p->m_af0;
	v.x = p->ddax*2 | p->xdir;
	v.z = p->ozv << 1;		/* shld/shl - dy.z's *2 is add/adc */
	v.f = p->ofv*2;
	v.a = p->oav*2;
	v.b = p->obv*2;
	v.g = p->ogv*2;
	v.r = p->orv*2;
	v.s = p->osv;
	v.t = p->otv;
	v.q = p->oqv;
	v.cov[0] = p->covs[0]*2;
	v.cov[1] = p->covs[1]*2;
	v.cov[2] = p->covs[2]*2;
	v.e[0] = p->m_abc;
	v.e[1] = p->m_ac0;
	v.e[2] = p->m_ac4;
	v.el = p->bbl;
	v.er = p->bbr;
	sv = v;

	if (pcalc->flat)		/* if/else here, but `sw = 4; if (...)'
					 * in Stamping - the two forms load
					 * pcalc at different points */
		sw = 8;
	else
		sw = 4;
	dx.x = pcalc->xdir ? -sw : sw;
	q = pcalc;
	dx.z = (long long)sw * q->ozdx;
	dx.f = sw * q->ofdx;
	dx.a = sw * q->oadx;
	dx.b = sw * q->obdx;
	dx.g = sw * q->ogdx;
	dx.r = sw * q->ordx;
	dx.s = sw * q->osdx;
	dx.t = sw * q->otdx;
	dx.q = sw * q->oqdx;
	dx.cov[0] = sw * q->covdx[0];
	dx.cov[1] = sw * q->covdx[1];
	dx.cov[2] = sw * q->covdx[2];
	dx.e[0] = sw * q->ddx[0]*2;
	dx.e[1] = sw * q->ddx[1]*2;
	dx.e[2] = sw * q->ddx[2]*2;
	dx.el = sw;
	dx.er = -sw;

	dy.x = 0;
	dy.z = q->ozdy*2;
	dy.f = q->ofdy*2;
	dy.a = q->oady*2;
	dy.b = q->obdy*2;
	dy.g = q->ogdy*2;
	dy.r = q->ordy*2;
	dy.s = q->osdy*2;
	dy.t = q->otdy*2;
	dy.q = q->oqdy*2;
	dy.cov[0] = q->covdy[0]*2;
	dy.cov[1] = q->covdy[1]*2;
	dy.cov[2] = q->covdy[2]*2;
	dy.e[0] = q->ddy[0] << 2;
	dy.e[1] = q->ddy[1] << 2;
	dy.e[2] = q->ddy[2] << 2;
	dy.el = 0;
	dy.er = 0;

	dyy = q->ydir == 0 ? 2 : -2;
	dybt = 2;
	dybb = -2;
	dyyn = -2;
}

/* One stamp to the right (or left, when xdir). */
void
DDA::HorizontalWalk(void)
{
	int save;

	save = v.e[0] < 0 || v.el < 0;
	v.x += dx.x;
	v.z += dx.z;
	v.f += dx.f;
	v.a += dx.a;
	v.b += dx.b;
	v.g += dx.g;
	v.r += dx.r;
	v.s += dx.s;
	v.t += dx.t;
	v.q += dx.q;
	if (pcalc->AA1) {
		v.cov[0] += dx.cov[0];
		v.cov[1] += dx.cov[1];
		v.cov[2] += dx.cov[2];
	}
	v.e[0] += dx.e[0];
	v.e[1] += dx.e[1];
	v.e[2] += dx.e[2];
	v.el += dx.el;
	v.er += dx.er;
	if (save)
		sv = v;
}

/* Back to the saved scanline start, one stamp row down (or up). */
void
DDA::VerticalWalk(void)
{
	v.x = sv.x + dy.x;
	v.z = sv.z + dy.z;
	v.f = sv.f + dy.f;
	v.a = sv.a + dy.a;
	v.b = sv.b + dy.b;
	v.g = sv.g + dy.g;
	v.r = sv.r + dy.r;
	v.s = sv.s + dy.s;
	v.t = sv.t + dy.t;
	v.q = sv.q + dy.q;
	if (pcalc->AA1) {
		v.cov[0] = sv.cov[0] + dy.cov[0];
		v.cov[1] = sv.cov[1] + dy.cov[1];
		v.cov[2] = sv.cov[2] + dy.cov[2];
	}
	v.e[0] = sv.e[0] + dy.e[0];
	v.e[1] = sv.e[1] + dy.e[1];
	v.e[2] = sv.e[2] + dy.e[2];
	v.el = sv.el + dy.el;
	v.er = sv.er + dy.er;
	y += dyy;
	bt += dybt;
	bb += dybb;
	yn += dyyn;
	sv = v;
}

int
DDA::IsVerticalWalk(void)
{
	return v.e[1] < 0 || v.e[2] < 0 || v.er < 0 || bt < 0;
}

int
DDA::IsWalk(void)
{
	return (bb >= 0) |
		(v.e[1] >= 0 && (v.e[2] >= 0 || v.el < 0) &&
		v.er >= 0 && bt >= 0);
}

/* A register write rides down to TXM inside the interpolator's start
 * values: the 64-bit datum is unpacked into the colour, Z and ST
 * channels and the address goes where the x position would be. */
void
DDA::Register(void)
{
	PCalc *p;

	isreg = 1;
	p = pcalc;
	amask = p->send_addr;
	mask = ((int)(p->send_reg >> 60) & 0xf) << 8;
	mask |= (int)(p->send_reg >> 56) & 0xf;
	t1 = ((int)(p->send_reg >> 42) & 0x3fff) << 10;
	t0 = ((int)(p->send_reg >> 28) & 0x3fff) << 10;
	s1 = ((int)(p->send_reg >> 14) & 0x3fff) << 10;
	s0 = ((int)p->send_reg & 0x3fff) << 10;
	px = p->send_addr;
	z0 = ((p->send_reg >> 40) & 0xffffff) << 4;
	f0 = ((int)(p->send_reg >> 32) & 0xff) << 4;
	a0 = ((int)(p->send_reg >> 24) & 0xff) << 4;
	b0 = ((int)(p->send_reg >> 16) & 0xff) << 4;
	g0 = ((int)(p->send_reg >> 8) & 0xff) << 4;
	r0 = ((int)p->send_reg & 0xff) << 4;
	CTXT = p->CTXT;
	ydir = p->ydir;
	TME = p->TME;
	FGE = p->FGE;
	ABE = p->ABE;
	FST = p->FST;
	AA1 = p->AA1;
	maxexp = p->maxexp;
	reg_addr = p->send_addr;
	reg_data = p->send_reg;
	txm->Put(this);
}

/* Walk the primitive: right along a scanline until it leaves the
 * triangle, then back to the saved scanline start and down. */
void
DDA::Primitive(void)
{
	int n;

	n = 0;
	InitStamp();
	InitWalk();
	if (Stamping(n)) {
		txm->Put(this);
		n++;
	}
	do {
		if (IsVerticalWalk())
			VerticalWalk();
		else
			HorizontalWalk();
		if (Stamping(n)) {
			txm->Put(this);
			n++;
		}
	} while (IsWalk());
}

void
DDA::Put(PCalc *p)
{
	pcalc = p;
	if (p->send_type)
		Register();
	else {
		type = p->type;
		Primitive();
	}
}
