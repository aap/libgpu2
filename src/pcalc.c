/* PCalc - GS front end stage 3: primitive setup.
 * Reconstructed from orig/lib/pcalc.o; see doc/notes/pcalc.md.
 *
 * abs() is declared here rather than pulled from <stdlib.h>: the era
 * header marks it __attribute__((const)), which changes how g++ 2.7
 * pops the argument.
 */

extern "C" int abs(int);

#include "pcalc.h"

/* divide one 64-bit slope numerator by twice the area, exactly */
#define SLOPEDIV(d, rc, n) {						\
	slong s;							\
									\
	s.Multiply(d, rc);						\
	s = sft >= 0 ? s >> sft : s << -sft;				\
	s = s >> (n);							\
	d = s.Combine();						\
}

void
PCalc::SwapLine(int &a, int &b, int &c, int &d)
{
	int t1, t2;

	t1 = a;
	t2 = b;
	a = c;
	b = d;
	c = t1;
	d = t2;
}

void
PCalc::SwapLine(unsigned int &a, unsigned int &b, unsigned int &c,
	unsigned int &d)
{
	unsigned int t1, t2;

	t1 = a;
	t2 = b;
	a = c;
	b = d;
	c = t1;
	d = t2;
}

/* Classify the triangle by the signs of its three edge Y deltas, then
 * relabel the vertices A/B/C so that the scan always runs the same way.
 * A is the vertex the DDA starts from, edge 0 is C->A, edge 1 A->B,
 * edge 2 B->C; ddx/ddy are that edge function's gradients (dE/dx is the
 * edge's dy, dE/dy its dx). */
void
PCalc::SortVertex(Pre3 *p, param *v)
{
	if (p->dy[0] < 0 && p->dy[1] < 0)
		sortcode = 1;
	else if (p->dy[0] < 0 && p->dy[2] < 0)
		sortcode = 2;
	else if (p->dy[1] < 0 && p->dy[2] < 0)
		sortcode = 3;
	else if (p->dy[0] > 0 && p->dy[1] > 0)
		sortcode = 4;
	else if (p->dy[0] > 0 && p->dy[2] > 0)
		sortcode = 5;
	else if (p->dy[1] > 0 && p->dy[2] > 0)
		sortcode = 6;
	else if (p->dyzero[0]) {
		if (p->dy[1] < 0 && p->dx[0] > 0)
			sortcode = 7;
		else if (p->dy[1] < 0 && p->dx[0] < 0)
			sortcode = 8;
		else if (p->dy[1] > 0 && p->dx[0] > 0)
			sortcode = 13;
		else if (p->dy[1] > 0 && p->dx[0] < 0)
			sortcode = 14;
	} else if (p->dyzero[1]) {
		if (p->dy[2] < 0 && p->dx[1] > 0)
			sortcode = 11;
		else if (p->dy[2] < 0 && p->dx[1] < 0)
			sortcode = 12;
		else if (p->dy[2] > 0 && p->dx[1] > 0)
			sortcode = 17;
		else if (p->dy[2] > 0 && p->dx[1] < 0)
			sortcode = 18;
	} else if (p->dyzero[2]) {
		if (p->dy[0] < 0 && p->dx[2] > 0)
			sortcode = 9;
		else if (p->dy[0] < 0 && p->dx[2] < 0)
			sortcode = 10;
		else if (p->dy[0] > 0 && p->dx[2] > 0)
			sortcode = 15;
		else if (p->dy[0] > 0 && p->dx[2] < 0)
			sortcode = 16;
	}
	if (sortcode == 4 || sortcode == 11 || sortcode == 14) {
		A = v[0];
		B = v[1];
		C = v[2];
		A.SetXY(v[0]);
		B.SetXY(v[1]);
		C.SetXY(v[2]);
		if (sortcode == 4)
			xdir = p->area >= 0 ? 1 : 0;
		else
			xdir = 0;
		ddx[0] = p->dy[2];
		ddy[0] = p->dx[2];
		ddx[1] = p->dy[0];
		ddy[1] = p->dx[0];
		ddx[2] = p->dy[1];
		ddy[2] = p->dx[1];
	} else if (sortcode == 3 || sortcode == 12 || sortcode == 15) {
		A = v[0];
		B = v[2];
		C = v[1];
		A.SetXY(v[0]);
		B.SetXY(v[2]);
		C.SetXY(v[1]);
		if (sortcode == 3)
			xdir = p->area < 0 ? 1 : 0;
		else
			xdir = 0;
		ddx[0] = p->dy[0];
		ddy[0] = p->dx[0];
		ddx[1] = p->dy[2];
		ddy[1] = p->dx[2];
		ddx[2] = p->dy[1];
		ddy[2] = p->dx[1];
	} else if (sortcode == 2 || sortcode == 10 || sortcode == 13) {
		A = v[1];
		B = v[0];
		C = v[2];
		A.SetXY(v[1]);
		B.SetXY(v[0]);
		C.SetXY(v[2]);
		if (sortcode == 2)
			xdir = p->area < 0 ? 1 : 0;
		else
			xdir = 0;
		ddx[0] = p->dy[1];
		ddy[0] = p->dx[1];
		ddx[1] = p->dy[0];
		ddy[1] = p->dx[0];
		ddx[2] = p->dy[2];
		ddy[2] = p->dx[2];
	} else if (sortcode == 6 || sortcode == 9 || sortcode == 18) {
		A = v[1];
		B = v[2];
		C = v[0];
		A.SetXY(v[1]);
		B.SetXY(v[2]);
		C.SetXY(v[0]);
		if (sortcode == 6)
			xdir = p->area >= 0 ? 1 : 0;
		else
			xdir = 0;
		ddx[0] = p->dy[0];
		ddy[0] = p->dx[0];
		ddx[1] = p->dy[1];
		ddy[1] = p->dx[1];
		ddx[2] = p->dy[2];
		ddy[2] = p->dx[2];
	} else if (sortcode == 5 || sortcode == 7 || sortcode == 16) {
		A = v[2];
		B = v[0];
		C = v[1];
		A.SetXY(v[2]);
		B.SetXY(v[0]);
		C.SetXY(v[1]);
		if (sortcode == 5)
			xdir = p->area >= 0 ? 1 : 0;
		else
			xdir = 0;
		ddx[0] = p->dy[1];
		ddy[0] = p->dx[1];
		ddx[1] = p->dy[2];
		ddy[1] = p->dx[2];
		ddx[2] = p->dy[0];
		ddy[2] = p->dx[0];
	} else if (sortcode == 1 || sortcode == 8 || sortcode == 17) {
		A = v[2];
		B = v[1];
		C = v[0];
		A.SetXY(v[2]);
		B.SetXY(v[1]);
		C.SetXY(v[0]);
		if (sortcode == 1)
			xdir = p->area < 0 ? 1 : 0;
		else
			xdir = 0;
		ddx[0] = p->dy[2];
		ddy[0] = p->dx[2];
		ddx[1] = p->dy[1];
		ddy[1] = p->dx[1];
		ddx[2] = p->dy[0];
		ddy[2] = p->dx[0];
	}
}

/* Pick the vertex the scan starts at, and the pair that bounds it. */
void
PCalc::GetSPoint(void)
{
	if (A.x > C.x) {
		if (xdir == 0) {
			if (A.x >= B.x) {
				spoint = 'C';
				ydir = 1;
				epointy = 'A';
				epointx = 'A';
			} else {
				spoint = 'C';
				ydir = 1;
				epointy = 'A';
				epointx = 'B';
			}
		} else {
			if (B.x >= C.x) {
				spoint = 'A';
				ydir = 0;
				epointy = 'C';
				epointx = 'C';
			} else {
				spoint = 'A';
				ydir = 0;
				epointy = 'C';
				epointx = 'B';
			}
		}
	} else if (A.x < C.x) {
		if (xdir == 0) {
			if (B.x > C.x) {
				spoint = 'A';
				ydir = 0;
				epointy = 'C';
				epointx = 'B';
			} else {
				spoint = 'A';
				ydir = 0;
				epointy = 'C';
				epointx = 'C';
			}
		} else {
			if (A.x > B.x) {
				spoint = 'C';
				ydir = 1;
				epointy = 'A';
				epointx = 'B';
			} else {
				spoint = 'C';
				ydir = 1;
				epointy = 'A';
				epointx = 'A';
			}
		}
	} else if (A.x == C.x) {
		spoint = 'A';
		ydir = 0;
		epointy = 'C';
		epointx = 'B';
	}
}
void
PCalc::CorrectSPoint(void)
{
	if (spoint == 'A') {
		sx = A.x;
		sy = A.y;
	} else if (spoint == 'C') {
		sx = C.x;
		sy = C.y;
	}
	if (xdir == 0)
		sxi = Ceil(sx);
	else
		sxi = Floor(sx);
	if (Subpixel(sx) == 0) {
		if (xdir == 0) {
			if (ddy[0] != 0)
				if (ydir != 0 || ddx[1] != 0)
					sxi++;
		} else
			sxi--;
	}
	if (AA1 == 0 && m_bd4 == 0) {
		if (xdir == 0)
			ddax = sxi & ~1;
		else
			ddax = sxi | one;
		if (ydir == 1)
			syi = Floor(sy - one);
		else if (ddx[1] != 0)
			syi = Floor(sy + pix);
		else
			syi = Ceil(sy);
		if (ydir == 1)
			dday = syi | one;
		else
			dday = syi & ~1;
	} else {
		int t;

		if (xdir == 0)
			sxi = sxi - 1;
		else
			sxi = sxi + 1;
		if (ydir == 0)
			t = sy - m_228;
		else
			t = sy + m_228;
		if (ydir == 0)
			syi = Ceil(t);
		else
			syi = Floor(t);
		if (xdir == 0) {
			if (abs(sxi % 2) == 1)
				ddax = sxi + 1 - stampw;
			else
				ddax = sxi;
		} else {
			if (abs(sxi % 2) == 1)
				ddax = sxi;
			else
				ddax = sxi - 1 + stampw;
		}
		if (ydir == 0)
			dday = syi & ~1;
		else
			dday = syi | one;
	}
}

void
PCalc::CorrectEPoint(void)
{
	if (epointx == 'A')
		ex = A.x;
	else if (epointx == 'B')
		ex = B.x;
	else if (epointx == 'C')
		ex = C.x;
	if (epointy == 'A')
		ey = A.y;
	else if (epointy == 'B')
		ey = B.y;
	else if (epointy == 'C')
		ey = C.y;
	if (AA1 == 0 && m_bd4 == 0) {
		if (xdir == 0)
			exi = Floor(ex - one);
		else
			exi = Ceil(ex);
		if (ddx[2] == 0) {
			if (ydir == 0)
				eyi = Floor(ey - one);
			else
				eyi = Ceil(ey);
		} else {
			if (ydir == 0)
				eyi = Floor(ey - one);
			else
				eyi = Ceil(ey + one);
		}
	} else {
		if (xdir == 0)
			ex = ex + m_228;
		else
			ex = ex - m_228;
		if (ydir == 0)
			ey = ey + m_228;
		else
			ey = ey - m_228;
		if (xdir == 0)
			exi = Floor(ex);
		else
			exi = Ceil(ex);
		if (ydir == 0)
			eyi = Floor(ey);
		else if (ddx[2] == 0)
			eyi = Ceil(ey);
		else
			eyi = Ceil(ey);
	}
}

void
PCalc::Slope(Pre3 *p, param *v)
{
	param sgnx;
	param sgny;
	int sft;
	long long v0, v1;
	int neg;
	int flag;
	int s1, s0;
	long long z1h, z1l, z0h, z0l;
	long long t, e;

	neg = 0;
	rcp.reciproc(p->area, sft, v0, v1, rem);
	sft = sft + 8;
	if (p->area < 0)
		neg = 1;
	v0 = v0 > 0 ? v0 : -v0;
	v1 = v1 > 0 ? v1 : -v1;
	dx = (v[0] - v[2]) * p->dy[1] + (v[1] - v[2]) * p->dy[2];
	dy = (v[0] - v[2]) * p->dx[1] + (v[1] - v[2]) * p->dx[2];
	dx.IfMinus(sgnx);
	dy.IfMinus(sgny);
	dx.GetAbs();
	dy.GetAbs();
	t = v0 >> 24;
	dx.r = t * dx.r;
	dy.r = t * dy.r;
	dx.g = t * dx.g;
	dy.g = t * dy.g;
	dx.b = t * dx.b;
	dy.b = t * dy.b;
	dx.a = t * dx.a;
	dy.a = t * dy.a;
	flag = 0;
	if (p->IIP == 0 && p->TME == 0) {
		e = v[1].z - v[2].z;
		e = e > 0 ? e : -e;
		if (e <= 0xffff) {
			e = v[0].z - v[2].z;
			e = e > 0 ? e : -e;
			if (e <= 0xffff)
				flag = 1;
		}
	}
	if (flag)
		t = v0 >> 24;
	else
		t = v1 >> 24;
	dx.f = t * dx.f;
	dy.f = t * dy.f;
	sft = -sft;
	dx.ShiftARGBSlope(sft);
	dy.ShiftARGBSlope(sft);
	flag = 0;
	e = v[1].z - v[2].z;
	e = e > 0 ? e : -e;
	if (e <= 0xffff) {
		e = v[0].z - v[2].z;
		e = e > 0 ? e : -e;
		if (e <= 0xffff)
			flag = 1;
	}
	if (flag) {
		SLOPEDIV(dx.z, v0, 26);
		SLOPEDIV(dy.z, v0, 26);
	}
	SLOPEDIV(dx.s, v0, 26);
	SLOPEDIV(dy.s, v0, 26);
	SLOPEDIV(dx.t, v0, 26);
	SLOPEDIV(dy.t, v0, 26);
	SLOPEDIV(dx.q, v0, 26);
	SLOPEDIV(dy.q, v0, 26);
	dx = dx * sgnx;
	dy = dy * sgny;
	flag = 0;
	e = v[1].z - v[2].z;
	e = e > 0 ? e : -e;
	if (e > 0xffff)
		flag = 1;
	else {
		e = v[0].z - v[2].z;
		e = e > 0 ? e : -e;
		if (e > 0xffff)
			flag = 1;
	}
	if (flag) {
		e = v[1].z - v[2].z;
		s1 = e >= 0 ? 1 : -1;
		e = v[0].z - v[2].z;
		s0 = e >= 0 ? 1 : -1;
		e = v[1].z - v[2].z;
		e = e > 0 ? e : -e;
		z1h = e & m_aac;
		z1h = z1h >> m_ab4;
		z1h = z1h * s1;
		e = v[1].z - v[2].z;
		e = e > 0 ? e : -e;
		z1l = e & m_aa4;
		z1l = z1l * s1;
		e = v[0].z - v[2].z;
		e = e > 0 ? e : -e;
		z0h = e & m_aac;
		z0h = z0h >> m_ab4;
		z0h = z0h * s0;
		e = v[0].z - v[2].z;
		e = e > 0 ? e : -e;
		z0l = e & m_aa4;
		z0l = z0l * s0;
		m_a84 = z0h * p->dy[1] + z1h * p->dy[2];
		m_a8c = z0l * p->dy[1] + z1l * p->dy[2];
		m_a94 = z0h * p->dx[1] + z1h * p->dx[2];
		m_a9c = z0l * p->dx[1] + z1l * p->dx[2];
		m_23c = m_a84 >= 0 ? 1 : -1;
		m_240 = m_a8c >= 0 ? 1 : -1;
		m_244 = m_a94 >= 0 ? 1 : -1;
		m_248 = m_a9c >= 0 ? 1 : -1;
		m_a84 = m_a84 > 0 ? m_a84 : -m_a84;
		m_a8c = m_a8c > 0 ? m_a8c : -m_a8c;
		m_a94 = m_a94 > 0 ? m_a94 : -m_a94;
		m_a9c = m_a9c > 0 ? m_a9c : -m_a9c;
		SLOPEDIV(m_a84, v1, 26 - m_ab4);
		SLOPEDIV(m_a8c, v1, 26);
		SLOPEDIV(m_a94, v1, 26 - m_ab4);
		SLOPEDIV(m_a9c, v1, 26);
		m_a84 = m_23c * m_a84;
		m_a8c = m_240 * m_a8c;
		m_a94 = m_244 * m_a94;
		m_a9c = m_248 * m_a9c;
		dx.z = m_a84 + m_a8c;
		dy.z = m_a94 + m_a9c;
	}
	if (neg == 1) {
		dx = dx * -1;
		dy = dy * -1;
		m_a84 = m_a84 * -1;
		m_a8c = m_a8c * -1;
		m_a94 = m_a94 * -1;
		m_a9c = m_a9c * -1;
		m_23c = -m_23c;
		m_240 = -m_240;
		m_244 = -m_244;
		m_248 = -m_248;
	}
}
void
PCalc::CheckOverFlow(void)
{
	char over;

	over = 0;
	if ((dx.r > 0 ? dx.r : -dx.r) > 0x3ffff ||
	    (dx.g > 0 ? dx.g : -dx.g) > 0x3ffff ||
	    (dx.b > 0 ? dx.b : -dx.b) > 0x3ffff ||
	    (dy.r > 0 ? dy.r : -dy.r) > 0x3ffff ||
	    (dy.g > 0 ? dy.g : -dy.g) > 0x3ffff ||
	    (dy.b > 0 ? dy.b : -dy.b) > 0x3ffff)
		over = 1;
	if (over) {
		dx.r = 0;
		dx.g = 0;
		dx.b = 0;
		dy.r = 0;
		dy.g = 0;
		dy.b = 0;
	}
	over = 0;
	if ((dx.a > 0 ? dx.a : -dx.a) > 0x3ffff ||
	    (dy.a > 0 ? dy.a : -dy.a) > 0x3ffff)
		over = 1;
	if (over) {
		dx.a = 0;
		dy.a = 0;
	}
	over = 0;
	if ((dx.f > 0 ? dx.f : -dx.f) > 0x3ffff ||
	    (dy.f > 0 ? dy.f : -dy.f) > 0x3ffff)
		over = 1;
	if (over) {
		dx.f = 0;
		dy.f = 0;
	}
	if (dx.z < -0x40000000000LL || dx.z > 0x3ffffffffffLL ||
	    dy.z < -0x40000000000LL || dy.z > 0x3ffffffffffLL) {
		dx.z = 0;
		dy.z = 0;
		m_a84 = 0;
		m_a8c = 0;
		m_a94 = 0;
		m_a9c = 0;
	}
}

/* The value of every attribute at the DDA start position: the vertex
 * value plus the gradient times the distance from the vertex.  Done in
 * signed-magnitude form (IfMinus / GetAbs / multiply the sign back in)
 * so the truncation matches the hardware. */
void
PCalc::StartVal(Pre3 *p, param *v, param w)
{
	param a, b, c, d, e, f;
	int ox, oy;
	int sgx, sgy;
	long long t0, t1, t2, t3;
	long long zz;
	int flag;

	if (xdir == 0)
		ox = (ddax << 4) - sx + 32;
	else
		ox = (ddax << 4) - sx - 32;
	oy = (dday << 4) - sy;
	sgx = ox >= 0 ? 1 : -1;
	sgy = oy >= 0 ? 1 : -1;
	ox = abs(ox);
	oy = abs(oy);
	e = dx;
	f = dy;
	e.IfMinus(c);
	f.IfMinus(d);
	e.GetAbs();
	f.GetAbs();
	a = e * ox;
	b = f * oy;
	a.a = a.a >> 4;
	a.f = a.f >> 4;
	a.r = a.r >> 4;
	a.g = a.g >> 4;
	a.b = a.b >> 4;
	b.a = b.a >> 4;
	b.f = b.f >> 4;
	b.r = b.r >> 4;
	b.g = b.g >> 4;
	b.b = b.b >> 4;
	a = a * c;
	b = b * d;
	a = a * sgx;
	b = b * sgy;
	flag = 0;
	if (p->type == 2) {
		zz = v[1].z - v[2].z;
		zz = zz > 0 ? zz : -zz;
		if (zz <= 0xffff) {
			zz = v[0].z - v[2].z;
			zz = zz > 0 ? zz : -zz;
			if (zz <= 0xffff)
				flag = 1;
		}
	}
	if (flag == 0 && p->type == 1) {
		zz = v[1].z - v[2].z;
		zz = zz > 0 ? zz : -zz;
		if (zz <= 0xffff)
			flag = 1;
	}
	if (flag)
		sv.z = (w.z << 10) + ((a.z + b.z) >> 4);
	else {
		t0 = ox * (m_a8c > 0 ? m_a8c : -m_a8c);
		t1 = ox * (m_a84 > 0 ? m_a84 : -m_a84);
		t2 = oy * (m_a9c > 0 ? m_a9c : -m_a9c);
		t3 = oy * (m_a94 > 0 ? m_a94 : -m_a94);
		t0 = t0 * m_240;
		t1 = t1 * m_23c;
		t2 = t2 * m_248;
		t3 = t3 * m_244;
		t0 = t0 * sgx;
		t1 = t1 * sgx;
		t2 = t2 * sgy;
		t3 = t3 * sgy;
		sv.z = ((t1 + t3) >> 4) + ((t0 + t2) >> 4) + (w.z << 10);
	}
	sv.f = (w.f << 10) + a.f + b.f;
	sv.a = (w.a << 10) + a.a + b.a;
	sv.r = (w.r << 10) + a.r + b.r;
	sv.g = (w.g << 10) + a.g + b.g;
	sv.b = (w.b << 10) + a.b + b.b;
	sv.s = (w.s << 10) + ((a.s + b.s) >> 4);
	sv.t = (w.t << 10) + ((a.t + b.t) >> 4);
	sv.q = (w.q << 10) + ((a.q + b.q) >> 4);
}

/* Evaluate the three edge functions at the DDA start position.  The
 * sample point of each edge is nudged by half a pixel in the direction
 * the scan runs, so that the fill rule comes out right; with AA1 the
 * nudge is m_228 along the edge's major axis instead. */
void
PCalc::GetDDAStart(Pre3 *p)
{
	int sg, t;
	int x0, x1, x2;
	int y0, y1, y2;
	int o0, o1, o2;
	long long z;

	if (ddx[2] == 0 || ddy[2] == 0)
		sg = 0;
	else {
		t = 1;
		if (ddx[2] < 0)
			t = -1;
		if (ddy[2] < 0)
			sg = t;
		else
			sg = -t;
	}
	if (xdir == 0) {
		x0 = ddax - 1;
		x0 = x0 + stampw;
		x1 = ddax + stampw;
		x2 = ddax + stampw;
	} else {
		x0 = ddax + 1;
		x0 = x0 - stampw;
		x1 = ddax - stampw;
		x2 = ddax - stampw;
	}
	if (ydir == 0) {
		y0 = dday + 2;
		y1 = dday + 1;
	} else {
		y0 = dday - 2;
		y1 = dday - 1;
	}
	if ((xdir == 0 && ydir == 1 && sg < 0) ||
	    (xdir == 1 && ydir == 1 && sg > 0))
		y2 = dday - 1;
	else if ((xdir == 0 && ydir == 0 && sg > 0) ||
	    (xdir == 1 && ydir == 0 && sg < 0))
		y2 = dday + 1;
	else
		y2 = dday;
	x0 = x0 << sft;
	x1 = x1 << sft;
	x2 = x2 << sft;
	y0 = y0 << sft;
	y1 = y1 << sft;
	y2 = y2 << sft;
	if (AA1 == 0 && m_bd4 == 0) {
		if (xdir == 0) {
			m_abc = ((x0 - sx) * ddx[0] + (y0 - sy) * ddy[0]) >> 3;
			if (ydir == 0 && ddx[1] == 0)
				m_ac0 = ((y1 - sy) * ddy[1]) >> 3;
			else
				m_ac0 = ((x1 - sx) * ddx[1] +
					(y1 - sy) * ddy[1] - one) >> 3;
			if (ydir == 1 && ddx[2] == 0)
				m_ac4 = ((y2 - sy) * ddy[2] + p->area) >> 3;
			else {
				z = (x2 - sx) * ddx[2] + (y2 - sy) * ddy[2] +
					p->area;
				m_ac4 = (z - one) >> 3;
			}
		} else {
			m_abc = ((x0 - sx) * ddx[0] +
				(y0 - sy) * ddy[0] - one) >> 3;
			m_ac0 = ((x1 - sx) * ddx[1] + (y1 - sy) * ddy[1]) >> 3;
			m_ac4 = ((x2 - sx) * ddx[2] + (y2 - sy) * ddy[2] +
				p->area) >> 3;
		}
	} else {
		o0 = 0;
		if (xdir != 0)
			o0 = one;
		if ((xdir == 0 && ydir == 0 && ddx[1] == 0) || xdir == 1)
			o1 = 0;
		else
			o1 = one;
		if ((xdir == 0 && ydir == 1 && ddx[2] == 0) || xdir == 1)
			o2 = 0;
		else
			o2 = one;
		if (steep[0] == 1) {
			if (xdir == 0)
				x0 = x0 + m_228;
			else
				x0 = x0 - m_228;
			m_abc = ((x0 - sx) * ddx[0] +
				(y0 - sy) * ddy[0] - o0) >> 3;
		} else if (ydir == 0)
			m_abc = ((x0 - sx) * ddx[0] +
				(y0 - m_228 - sy) * ddy[0] - o0) >> 3;
		else
			m_abc = ((x0 - sx) * ddx[0] +
				(y0 + m_228 - sy) * ddy[0] - o0) >> 3;
		if (steep[1] == 1) {
			if (xdir == 0)
				x1 = x1 - m_228;
			else
				x1 = x1 + m_228;
			m_ac0 = ((x1 - sx) * ddx[1] +
				(y1 - sy) * ddy[1] - o1) >> 3;
		} else if (ydir == 0)
			m_ac0 = ((x1 - sx) * ddx[1] +
				(y1 + m_228 - sy) * ddy[1] - o1) >> 3;
		else
			m_ac0 = ((x1 - sx) * ddx[1] +
				(y1 - m_228 - sy) * ddy[1] - o1) >> 3;
		if (steep[2] == 1) {
			if (xdir == 0)
				x2 = x2 - m_228;
			else
				x2 = x2 + m_228;
			z = (x2 - sx) * ddx[2] + (y2 - sy) * ddy[2] + p->area;
			m_ac4 = (z - o2) >> 3;
		} else if (ddy[2] < 0) {
			z = (x2 - sx) * ddx[2] +
				(y2 - m_228 - sy) * ddy[2] + p->area;
			m_ac4 = (z - o2) >> 3;
		} else {
			z = (x2 - sx) * ddx[2] +
				(y2 + m_228 - sy) * ddy[2] + p->area;
			m_ac4 = (z - o2) >> 3;
		}
	}
}

int
PCalc::AASlope(long long x, int n, int d)
{
	int sft;
	long long v0, v1;
	long long t;
	int sign;

	rcp.reciproc(d << 4, sft, v0, v1, rem);
	sft = sft + 8;
	if (x < 0)
		n = -n;
	sign = n >= 0 ? 1 : -1;
	t = abs(n);
	t = t * v0;
	sft = -sft;
	if (sft < 0)
		t = t << -sft;
	else
		t = t >> sft;
	t = t >> 18;
	t = t * sign;
	return t;
}

long long
PCalc::C_Hosei(long long v, int d)
{
	int sft;
	long long v0, v1;

	rcp.reciproc(d << 4, sft, v0, v1, rem);
	sft = sft + 8;
	v = v * v0;
	sft = -sft;
	if (sft < 0)
		v = v << -sft;
	else
		v = v >> sft;
	return v >> 23;
}

void
PCalc::SortCoverage(Pre3 *p)
{
	if (sortcode == 4 || sortcode == 11 || sortcode == 14) {
		steep[0] = p->steep[2];
		steep[1] = p->steep[0];
		steep[2] = p->steep[1];
		covdx[0] = cov[2];
		covdy[0] = cov[3];
		covdx[1] = cov[4];
		covdy[1] = cov[5];
		covdx[2] = cov[0];
		covdy[2] = cov[1];
	} else if (sortcode == 3 || sortcode == 12 || sortcode == 15) {
		steep[0] = p->steep[0];
		steep[1] = p->steep[2];
		steep[2] = p->steep[1];
		covdx[0] = cov[4];
		covdy[0] = cov[5];
		covdx[1] = cov[2];
		covdy[1] = cov[3];
		covdx[2] = cov[0];
		covdy[2] = cov[1];
	} else if (sortcode == 2 || sortcode == 10 || sortcode == 13) {
		steep[0] = p->steep[1];
		steep[1] = p->steep[0];
		steep[2] = p->steep[2];
		covdx[0] = cov[0];
		covdy[0] = cov[1];
		covdx[1] = cov[4];
		covdy[1] = cov[5];
		covdx[2] = cov[2];
		covdy[2] = cov[3];
	} else if (sortcode == 6 || sortcode == 9 || sortcode == 18) {
		steep[0] = p->steep[0];
		steep[1] = p->steep[1];
		steep[2] = p->steep[2];
		covdx[0] = cov[4];
		covdy[0] = cov[5];
		covdx[1] = cov[0];
		covdy[1] = cov[1];
		covdx[2] = cov[2];
		covdy[2] = cov[3];
	} else if (sortcode == 5 || sortcode == 7 || sortcode == 16) {
		steep[0] = p->steep[1];
		steep[1] = p->steep[2];
		steep[2] = p->steep[0];
		covdx[0] = cov[0];
		covdy[0] = cov[1];
		covdx[1] = cov[2];
		covdy[1] = cov[3];
		covdx[2] = cov[4];
		covdy[2] = cov[5];
	} else if (sortcode == 1 || sortcode == 8 || sortcode == 17) {
		steep[0] = p->steep[2];
		steep[1] = p->steep[1];
		steep[2] = p->steep[0];
		covdx[0] = cov[2];
		covdy[0] = cov[3];
		covdx[1] = cov[0];
		covdy[1] = cov[1];
		covdx[2] = cov[4];
		covdy[2] = cov[5];
	}
}

int
PCalc::AAStartVal(int a, int b, int c, int d)
{
	int sa, sb, sc, sd;
	long long e, f;

	sa = a >= 0 ? 1 : -1;
	sb = b >= 0 ? 1 : -1;
	a = abs(a);
	b = abs(b);
	sc = c >= 0 ? 1 : -1;
	sd = d >= 0 ? 1 : -1;
	c = abs(c);
	d = abs(d);
	e = (long long)a * c;
	f = (long long)b * d;
	e = e * sa;
	f = f * sb;
	e = e * sc;
	f = f * sd;
	return (int)(e + f + m_230) >> 5;
}

void
PCalc::AACoverage(Pre3 *p)
{
	int t;
	int h;

	cov[0] = AASlope(p->area, p->dy[1],
		abs(p->steep[1] == 0 ? p->dx[1] : p->dy[1]));
	cov[1] = AASlope(p->area, p->dx[1],
		abs(p->steep[1] == 0 ? p->dx[1] : p->dy[1]));
	cov[2] = AASlope(p->area, p->dy[2],
		abs(p->steep[2] == 0 ? p->dx[2] : p->dy[2]));
	cov[3] = AASlope(p->area, p->dx[2],
		abs(p->steep[2] == 0 ? p->dx[2] : p->dy[2]));
	cov[4] = AASlope(p->area, p->dy[0],
		abs(p->steep[0] == 0 ? p->dx[0] : p->dy[0]));
	cov[5] = AASlope(p->area, p->dx[0],
		abs(p->steep[0] == 0 ? p->dx[0] : p->dy[0]));
	SortCoverage(p);
	if (spoint == 'C') {
		SwapLine(covdx[1], covdy[1], covdx[2], covdy[2]);
		t = steep[2];
		steep[2] = steep[1];
		steep[1] = t;
	}
	h = C_Hosei(p->area > 0 ? p->area : -p->area,
		abs(steep[2] == 0 ? ddy[2] : ddx[2]));
	if (xdir == 0)
		t = (ddax << 4) - sx + 32;
	else
		t = (ddax << 4) - sx - 32;
	covs[0] = AAStartVal(covdx[0], covdy[0], t, (dday << 4) - sy);
	covs[1] = AAStartVal(covdx[1], covdy[1], t, (dday << 4) - sy);
	covs[2] = AAStartVal(covdx[2], covdy[2], t, (dday << 4) - sy);
	covs[2] += h;
}

void
PCalc::DrawTriangle(Pre3 *p)
{
	param v[3];
	int i;
	int t;
	int e;

	if (m_bd4 != 0) {
		t = 1 << sft;
		m_22c = t;
		m_228 = t / 2;
		m_230 = (1 << msft) / 2;
	} else if (AA1 != 0) {
		t = 1 << sft;
		m_22c = t;
		m_228 = t;
		m_230 = 1 << msft;
	}
	for (i = 0; i <= 2; i++) {
		v[i].x = p->v[i].x;
		v[i].y = p->v[i].y;
		v[i].z = p->v[i].z;
		v[i].r = p->v[i].r;
		v[i].g = p->v[i].g;
		v[i].b = p->v[i].b;
		v[i].a = p->v[i].a;
		v[i].f = p->v[i].f;
		v[i].s = p->v[i].s;
		v[i].t = p->v[i].t;
		v[i].q = p->v[i].q;
	}
	if (p->IIP == 0) {
		for (i = 0; i <= 1; i++) {
			v[i].r = v[2].r;
			v[i].g = v[2].g;
			v[i].b = v[2].b;
			v[i].a = v[2].a;
		}
	}
	SortVertex(p, v);
	GetSPoint();
	if (spoint == 'C')
		SwapLine(ddx[1], ddy[1], ddx[2], ddy[2]);
	if (p->FIX != 0) {
		if (spoint == 'A') {
			B.r = A.r;
			B.g = A.g;
			B.b = A.b;
			B.a = A.a;
			B.z = A.z;
			B.f = A.f;
			B.s = A.s;
			B.t = A.t;
			B.q = A.q;
			C.r = A.r;
			C.g = A.g;
			C.b = A.b;
			C.a = A.a;
			C.z = A.z;
			C.f = A.f;
			C.s = A.s;
			C.t = A.t;
			C.q = A.q;
		} else if (spoint == 'C') {
			B.r = C.r;
			B.g = C.g;
			B.b = C.b;
			B.a = C.a;
			B.z = C.z;
			B.f = C.f;
			B.s = C.s;
			B.t = C.t;
			B.q = C.q;
			A.r = C.r;
			A.g = C.g;
			A.b = C.b;
			A.a = C.a;
			A.z = C.z;
			A.f = C.f;
			A.s = C.s;
			A.t = C.t;
			A.q = C.q;
		}
	}
	CorrectSPoint();
	CorrectEPoint();
	dx.x = 0;
	dx.y = 0;
	dx.z = 0;
	dx.a = 0;
	dx.f = 0;
	dx.r = 0;
	dx.g = 0;
	dx.b = 0;
	dx.s = 0;
	dx.t = 0;
	dx.q = 0;
	dy.x = 0;
	dy.y = 0;
	dy.z = 0;
	dy.a = 0;
	dy.f = 0;
	dy.r = 0;
	dy.g = 0;
	dy.b = 0;
	dy.s = 0;
	dy.t = 0;
	dy.q = 0;
	m_a84 = 0;
	m_a8c = 0;
	m_a94 = 0;
	m_a9c = 0;
	if (FIX == 0)
		Slope(p, v);
	if (AA1 != 0)
		CheckOverFlow();
	if (spoint == 'A')
		StartVal(p, v, A);
	else if (spoint == 'C')
		StartVal(p, v, C);
	if (AA1 != 0 || m_bd4 != 0)
		AACoverage(p);
	if (p->area < 0) {
		ddx[0] = -ddx[0];
		ddy[0] = -ddy[0];
		ddx[1] = -ddx[1];
		ddy[1] = -ddy[1];
		ddx[2] = -ddx[2];
		ddy[2] = -ddy[2];
		p->area = p->area * -1;
	}
	GetDDAStart(p);
	BBox();
	e = 0;
	if (ydir == 0) {
		e = one;
		m_234 = B.y - ((dday << sft) + pix);
	} else
		m_234 = (dday << sft) - pix - B.y;
	m_af0 = Floor(m_234 - e);
}

void
PCalc::SortLine(param *v)
{
	if (v[2].y < v[1].y) {
		if (v[2].x > v[1].x) {
			xdir = 0;
			ydir = 1;
		} else if (v[2].x < v[1].x) {
			xdir = 1;
			ydir = 1;
		} else if (v[2].x == v[1].x) {
			xdir = 1;
			ydir = 1;
		}
	} else if (v[2].y > v[1].y) {
		if (v[2].x > v[1].x) {
			xdir = 0;
			ydir = 0;
		} else if (v[2].x < v[1].x) {
			xdir = 1;
			ydir = 0;
		} else if (v[2].x == v[1].x) {
			xdir = 1;
			ydir = 0;
		}
	} else if (v[2].y == v[1].y) {
		if (v[2].x > v[1].x) {
			xdir = 0;
			ydir = 0;
		} else if (v[2].x < v[1].x) {
			xdir = 1;
			ydir = 0;
		} else if (v[2].x == v[1].x) {
			xdir = 1;
			ydir = 0;
		}
	}
}

void
PCalc::CorrectLineStart(void)
{
	int a, b;
	int d1, d2;

	if (AA1 == 0) {
		a = sx;
		b = sy;
	} else if (steep[0] == 0) {
		a = sx;
		if (ydir == 0)
			b = sy - pix;
		else
			b = sy + pix;
	} else {
		if (xdir == 0)
			a = sx - pix;
		else
			a = sx + pix;
		b = sy;
	}
	if (steep[0] == 0) {
		if (xdir != 0)
			sxi = Ceil(a);
		else
			sxi = Floor(a);
		syi = Floor(pix / 2 + b);
	} else {
		sxi = Floor(pix / 2 + a);
		if (ydir != 0)
			syi = Ceil(b);
		else
			syi = Floor(b);
	}
	d1 = abs(a - (sxi << 4));
	d2 = abs(b - (syi << 4));
	if (d1 + d2 > pix / 2 || (d1 + d2 >= pix / 2 &&
	    (steep[0] == 0 ? b < (syi << 4) : a < (sxi << 4)) == 0)) {
		if (steep[0] == 0) {
			if (xdir == 0)
				sxi = sxi + 1;
			else
				sxi = sxi - 1;
		} else {
			if (ydir == 0)
				syi = syi + 1;
			else
				syi = syi - 1;
		}
	}
	if (AA1 == 0) {
		if (xdir == 0)
			ddax = sxi & ~1;
		else
			ddax = sxi | one;
	} else {
		if (xdir == 0) {
			if (abs(sxi % 2) == 1)
				ddax = sxi + 1 - stampw;
			else
				ddax = sxi;
		} else {
			if (abs(sxi % 2) == 1)
				ddax = sxi;
			else
				ddax = sxi - 1 + stampw;
		}
	}
	if (ydir == 0)
		dday = syi & ~1;
	else
		dday = syi | one;
}

void
PCalc::CorrectLineEnd(void)
{
	int a, b;
	int d1, d2;

	if (AA1 == 0) {
		a = ex;
		b = ey;
	} else if (steep[0] == 0) {
		a = ex;
		if (ydir == 1)
			b = ey - pix;
		else
			b = ey + pix;
	} else {
		if (xdir == 1)
			a = ex - pix;
		else
			a = ex + pix;
		b = ey;
	}
	if (steep[0] == 0) {
		if (xdir != 0)
			exi = Ceil(a);
		else
			exi = Floor(a);
		eyi = Floor(pix / 2 + b);
	} else {
		exi = Floor(pix / 2 + a);
		if (ydir != 0)
			eyi = Ceil(b);
		else
			eyi = Floor(b);
	}
	d1 = abs(a - (exi << 4));
	d2 = abs(b - (eyi << 4));
	if (d1 + d2 > pix / 2 || (d1 + d2 >= pix / 2 &&
	    (steep[0] == 0 ? b < (eyi << 4) : a < (exi << 4)) == 0)) {
		if (steep[0] == 0) {
			if (xdir == 0)
				exi = exi + 1;
			else
				exi = exi - 1;
		} else {
			if (ydir == 0)
				eyi = eyi + 1;
			else
				eyi = eyi - 1;
		}
	}
	if (steep[0] == 0) {
		if (xdir == 0)
			exi = exi - 1;
		else
			exi = exi + 1;
	} else {
		if (ydir == 0)
			eyi = eyi - 1;
		else
			eyi = eyi + 1;
	}
}

/* The line's per-step gradient: the same reciprocal-multiply as Slope(),
 * but divided by the major-axis extent rather than by the area, and with
 * only one direction to interpolate along. */
void
PCalc::LineSlope(param *v, int n, param &d, long long &a, long long &b,
	int &c, int &e)
{
	param sgn;
	int sft;
	long long v0, v1;
	int sn;
	long long zz;
	int sz;

	rcp.reciproc(n << 4, sft, v0, v1, rem);
	sft = sft + 8;
	sn = (n << 4) >= 0 ? 1 : -1;
	v0 = v0 > 0 ? v0 : -v0;
	v1 = v1 > 0 ? v1 : -v1;
	d = v[2] - v[1];
	d = d << 4;
	d.IfMinus(sgn);
	d.GetAbs();
	d.r = (v0 >> 24) * d.r;
	d.g = (v0 >> 24) * d.g;
	d.b = (v0 >> 24) * d.b;
	d.a = (v0 >> 24) * d.a;
	d.f = (v0 >> 24) * d.f;
	sft = -sft;
	d.ShiftARGBSlope(sft);
	zz = v[2].z - v[1].z;
	zz = zz > 0 ? zz : -zz;
	if (zz <= 0xffff) {
		SLOPEDIV(d.z, v0, 26);
	}
	SLOPEDIV(d.s, v0, 26);
	SLOPEDIV(d.t, v0, 26);
	SLOPEDIV(d.q, v0, 26);
	d = d * sgn;
	zz = v[2].z - v[1].z;
	zz = zz > 0 ? zz : -zz;
	if (zz > 0xffff) {
		zz = v[2].z - v[1].z;
		sz = zz >= 0 ? 1 : -1;
		zz = v[2].z - v[1].z;
		zz = zz > 0 ? zz : -zz;
		a = zz & m_aac;
		a = a >> m_ab4;
		a = a * sz;
		zz = v[2].z - v[1].z;
		zz = zz > 0 ? zz : -zz;
		b = zz & m_aa4;
		b = b * sz;
		c = a >= 0 ? 1 : -1;
		e = b >= 0 ? 1 : -1;
		a = a > 0 ? a : -a;
		b = b > 0 ? b : -b;
		a = a << 4;
		b = b << 4;
		SLOPEDIV(a, v1, 26 - m_ab4);
		SLOPEDIV(b, v1, 26);
		a = c * a;
		b = e * b;
		d.z = a + b;
	}
	if (sn == -1) {
		d = d * -1;
		a = a * -1;
		b = b * -1;
		c = -c;
		e = -e;
	}
}

/* The two edge functions that bound a line, evaluated at the DDA start.
 * The half-pixel nudge h goes along the minor axis for a shallow line
 * and along X for a steep one. */
void
PCalc::LineDDAEdgeStart(void)
{
	int h;

	h = pix;
	if (AA1 == 0)
		h = h / 2;
	if (steep[0] == 0) {
		if (xdir == 0 && ydir == 0) {
			m_abc = ((((ddax + stampw - 1) << 4) - sx) * ddx[0] +
				(((dday + 2) << 4) - sy - h) * ddy[0]) >> 3;
			m_ac0 = ((((ddax + stampw) << 4) - sx) * ddx[1] +
				(((dday + 1) << 4) - sy + h) * ddy[1] - one) >> 3;
		} else if (xdir == 0 && ydir == 1) {
			m_abc = ((((ddax + stampw - 1) << 4) - sx) * ddx[0] +
				(((dday - 2) << 4) - sy + h) * ddy[0] - one) >> 3;
			m_ac0 = ((((ddax + stampw) << 4) - sx) * ddx[1] +
				(((dday - 1) << 4) - sy - h) * ddy[1]) >> 3;
		} else if (xdir == 1 && ydir == 0) {
			m_abc = ((((ddax + 1 - stampw) << 4) - sx) * ddx[0] +
				(((dday + 2) << 4) - sy - h) * ddy[0]) >> 3;
			m_ac0 = ((((ddax - stampw) << 4) - sx) * ddx[1] +
				(((dday + 1) << 4) - sy + h) * ddy[1] - one) >> 3;
		} else if (xdir == 1 && ydir == 1) {
			m_abc = ((((ddax + 1 - stampw) << 4) - sx) * ddx[0] +
				(((dday - 2) << 4) - sy + h) * ddy[0] - one) >> 3;
			m_ac0 = ((((ddax - stampw) << 4) - sx) * ddx[1] +
				(((dday - 1) << 4) - sy - h) * ddy[1]) >> 3;
		}
	} else {
		if (xdir == 0 && ydir == 0) {
			m_abc = ((((ddax + stampw - 1) << 4) - sx + h) * ddx[0] +
				(((dday + 2) << 4) - sy) * ddy[0] - one) >> 3;
			m_ac0 = ((((ddax + stampw) << 4) - sx - h) * ddx[1] +
				(((dday + 1) << 4) - sy) * ddy[1]) >> 3;
		} else if (xdir == 0 && ydir == 1) {
			m_abc = ((((ddax + stampw - 1) << 4) - sx + h) * ddx[0] +
				(((dday - 2) << 4) - sy) * ddy[0] - one) >> 3;
			m_ac0 = ((((ddax + stampw) << 4) - sx - h) * ddx[1] +
				(((dday - 1) << 4) - sy) * ddy[1]) >> 3;
		} else if (xdir == 1 && ydir == 0) {
			m_abc = ((((ddax + 1 - stampw) << 4) - sx - h) * ddx[0] +
				(((dday + 2) << 4) - sy) * ddy[0]) >> 3;
			m_ac0 = ((((ddax - stampw) << 4) - sx + h) * ddx[1] +
				(((dday + 1) << 4) - sy) * ddy[1] - one) >> 3;
		} else if (xdir == 1 && ydir == 1) {
			m_abc = ((((ddax + 1 - stampw) << 4) - sx - h) * ddx[0] +
				(((dday - 2) << 4) - sy) * ddy[0]) >> 3;
			m_ac0 = ((((ddax - stampw) << 4) - sx + h) * ddx[1] +
				(((dday - 1) << 4) - sy) * ddy[1] - one) >> 3;
		}
	}
}

void
PCalc::LineAACov(Pre3 *p)
{
	int t;

	covdx[0] = AASlope(1, ddx[0], abs(p->steep[1] == 0 ? ddy[0] : ddx[0]));
	covdy[0] = AASlope(1, ddy[0], abs(p->steep[1] == 0 ? ddy[0] : ddx[0]));
	covdx[1] = AASlope(1, ddx[1], abs(p->steep[1] == 0 ? ddy[1] : ddx[1]));
	covdy[1] = AASlope(1, ddy[1], abs(p->steep[1] == 0 ? ddy[1] : ddx[1]));
	if (xdir == 0)
		t = (ddax << 4) - sx + 32;
	else
		t = (ddax << 4) - sx - 32;
	covs[0] = AAStartVal(covdx[0], covdy[0], t, (dday << 4) - sy);
	covs[1] = AAStartVal(covdx[1], covdy[1], t, (dday << 4) - sy);
}

void
PCalc::DrawLine(Pre3 *p)
{
	param v[3];
	int i;

	ddx[2] = 0;
	ddy[2] = 0;
	m_ac4 = 0;
	if (AA1 != 0) {
		m_228 = 1 << sft;
		m_230 = 1 << msft;
	}
	for (i = 1; i <= 2; i++) {
		v[i].x = p->v[i].x;
		v[i].y = p->v[i].y;
		v[i].z = p->v[i].z;
		v[i].r = p->v[i].r;
		v[i].g = p->v[i].g;
		v[i].b = p->v[i].b;
		v[i].a = p->v[i].a;
		v[i].f = p->v[i].f;
		v[i].s = p->v[i].s;
		v[i].t = p->v[i].t;
		v[i].q = p->v[i].q;
	}
	v[0].x = 0;
	v[0].y = 0;
	v[0].z = 0;
	v[0].a = 0;
	v[0].f = 0;
	v[0].r = 0;
	v[0].g = 0;
	v[0].b = 0;
	v[0].s = 0;
	v[0].t = 0;
	v[0].q = 0;
	if (p->IIP == 0) {
		v[1].r = v[2].r;
		v[1].g = v[2].g;
		v[1].b = v[2].b;
		v[1].a = v[2].a;
	}
	if (p->FIX != 0) {
		v[2].r = v[1].r;
		v[2].g = v[1].g;
		v[2].b = v[1].b;
		v[2].a = v[1].a;
		v[2].z = v[1].z;
		v[2].f = v[1].f;
		v[2].s = v[1].s;
		v[2].t = v[1].t;
		v[2].q = v[1].q;
	}
	SortLine(v);
	sx = v[1].x;
	sy = v[1].y;
	ex = v[2].x;
	ey = v[2].y;
	steep[0] = p->steep[1];
	steep[1] = p->steep[1];
	CorrectLineStart();
	CorrectLineEnd();
	if (xdir != ydir) {
		ddx[0] = -p->dy[1];
		ddy[0] = -p->dx[1];
	} else {
		ddx[0] = p->dy[1];
		ddy[0] = p->dx[1];
	}
	ddx[1] = -ddx[0];
	ddy[1] = -ddy[0];
	if (steep[0] == 0) {
		LineSlope(v, v[2].x - v[1].x, dx, m_a84, m_a8c, m_23c, m_240);
		dy.x = 0;
		dy.y = 0;
		dy.z = 0;
		dy.a = 0;
		dy.f = 0;
		dy.r = 0;
		dy.g = 0;
		dy.b = 0;
		dy.s = 0;
		dy.t = 0;
		dy.q = 0;
		m_a94 = 0;
		m_a9c = 0;
		m_244 = 1;
		m_248 = 1;
	} else {
		dx.x = 0;
		dx.y = 0;
		dx.z = 0;
		dx.a = 0;
		dx.f = 0;
		dx.r = 0;
		dx.g = 0;
		dx.b = 0;
		dx.s = 0;
		dx.t = 0;
		dx.q = 0;
		m_a84 = 0;
		m_a8c = 0;
		m_23c = 1;
		m_240 = 1;
		LineSlope(v, v[2].y - v[1].y, dy, m_a94, m_a9c, m_244, m_248);
	}
	StartVal(p, v, v[1]);
	LineDDAEdgeStart();
	BBox();
	if (AA1 != 0)
		LineAACov(p);
	if (ydir == 0)
		m_af0 = Floor(v[2].y - ((dday + 1) << 4));
	else
		m_af0 = Floor(((dday - 1) << 4) - v[2].y);
}

void
PCalc::SpriteSlope(long long x, int n, long long &r)
{
	int sft;
	long long v0, v1;
	int sn, sx;
	int d;

	d = n << 4;
	rcp.reciproc(d, sft, v0, v1, rem);
	sft = sft + 8;
	sn = d >= 0 ? 1 : -1;
	v0 = v0 > 0 ? v0 : -v0;
	sx = x >= 0 ? 1 : -1;
	r = (long long)abs(x) << 4;
	r = r * v0;
	sft = -sft;
	if (sft < 0)
		r = r << -sft;
	else
		r = r >> sft;
	r = r >> 26;
	r = sx * r;
	r = sn * r;
}

void
PCalc::SpriteStartVal(long long &r, long long a, long long b, int c)
{
	int sb, sc;
	long long t;

	sb = b >= 0 ? 1 : -1;
	b = abs(b);
	sc = c >= 0 ? 1 : -1;
	c = abs(c);
	t = b * c;
	t = sb * t;
	t = t * sc;
	t = t >> 4;
	r = t + (a << 10);
}

/* A sprite is axis-aligned, so there are no edge functions and no
 * colour/Z gradients at all - only S and T step, one per axis.
 *
 * Note the FIX block below: R/G/B/A/Z/F/Q are copied from vertex 2 onto
 * vertex 1, but S and T are copied the *other* way.  That asymmetry is
 * in the 1998 object; it is not a transcription error. */
void
PCalc::DrawSprite(Pre3 *p)
{
	param v[3];
	int i;
	int t;

	ddx[0] = 0;
	ddy[0] = 0;
	ddx[1] = 0;
	ddy[1] = 0;
	ddx[2] = 0;
	ddy[2] = 0;
	m_abc = 0;
	m_ac0 = 0;
	m_ac4 = 0;
	dx.x = 0;
	dx.y = 0;
	dx.z = 0;
	dx.a = 0;
	dx.f = 0;
	dx.r = 0;
	dx.g = 0;
	dx.b = 0;
	dx.s = 0;
	dx.t = 0;
	dx.q = 0;
	dy.x = 0;
	dy.y = 0;
	dy.z = 0;
	dy.a = 0;
	dy.f = 0;
	dy.r = 0;
	dy.g = 0;
	dy.b = 0;
	dy.s = 0;
	dy.t = 0;
	dy.q = 0;
	for (i = 1; i <= 2; i++) {
		v[i].x = p->v[i].x;
		v[i].y = p->v[i].y;
		v[i].z = p->v[i].z;
		v[i].r = p->v[i].r;
		v[i].g = p->v[i].g;
		v[i].b = p->v[i].b;
		v[i].a = p->v[i].a;
		v[i].f = p->v[i].f;
		v[i].s = p->v[i].s;
		v[i].t = p->v[i].t;
		v[i].q = p->v[i].q;
	}
	if (p->FIX != 0) {
		v[1].r = v[2].r;
		v[1].g = v[2].g;
		v[1].b = v[2].b;
		v[1].a = v[2].a;
		v[1].z = v[2].z;
		v[1].f = v[2].f;
		v[2].s = v[1].s;
		v[2].t = v[1].t;
		v[1].q = v[2].q;
	}
	xdir = v[1].x >= v[2].x;
	ydir = v[1].y > v[2].y;
	if (xdir == 0)
		sxi = Ceil(v[1].x);
	else
		sxi = Floor(v[1].x - one);
	if (ydir == 0)
		syi = Ceil(v[1].y);
	else
		syi = Floor(v[1].y - one);
	if (xdir == 0)
		exi = Floor(v[2].x - one);
	else
		exi = Ceil(v[2].x);
	if (ydir == 0)
		eyi = Floor(v[2].y - one);
	else
		eyi = Ceil(v[2].y);
	if (xdir == 0)
		ddax = sxi & ~1;
	else
		ddax = sxi | one;
	if (ydir == 0)
		dday = syi & ~1;
	else
		dday = syi | one;
	BBox();
	if (v[2].x == v[1].x)
		m_bf4 |= 1;
	if (v[2].y == v[1].y)
		m_bf4 |= 2;
	SpriteSlope(v[2].s - v[1].s, v[2].x - v[1].x, dx.s);
	SpriteSlope(v[1].t - v[2].t, v[1].y - v[2].y, dy.t);
	sv = v[2] << 10;
	if (xdir == 0)
		t = (ddax << 4) - v[1].x + 32;
	else
		t = (ddax << 4) - v[1].x - 32;
	SpriteStartVal(sv.s, v[1].s, dx.s, t);
	SpriteStartVal(sv.t, v[1].t, dy.t, (dday << 4) - v[1].y);
	if (ydir == 0)
		m_af0 = Floor(v[2].y - ((dday + 1) << 4));
	else
		m_af0 = Floor(((dday - 1) << 4) - v[2].y);
}

void
PCalc::DrawPoint(Pre3 *p)
{
	int x0, x1, y0, y1;
	int h;

	xdir = 0;
	ydir = 0;
	ddx[0] = 0;
	ddy[0] = 0;
	ddx[1] = 0;
	ddy[1] = 0;
	ddx[2] = 0;
	ddy[2] = 0;
	m_abc = 0;
	m_ac0 = 0;
	m_ac4 = 0;
	m_af0 = 0;
	dx.x = 0;
	dx.y = 0;
	dx.z = 0;
	dx.a = 0;
	dx.f = 0;
	dx.r = 0;
	dx.g = 0;
	dx.b = 0;
	dx.s = 0;
	dx.t = 0;
	dx.q = 0;
	dy.x = 0;
	dy.y = 0;
	dy.z = 0;
	dy.a = 0;
	dy.f = 0;
	dy.r = 0;
	dy.g = 0;
	dy.b = 0;
	dy.s = 0;
	dy.t = 0;
	dy.q = 0;
	sx = p->v[2].x;
	sy = p->v[2].y;
	if (CTXT == 0) {
		x0 = scissor[0].scax0;
		x1 = scissor[0].scax1;
		y0 = scissor[0].scay0;
		y1 = scissor[0].scay1;
	} else {
		x0 = scissor[1].scax0;
		x1 = scissor[1].scax1;
		y0 = scissor[1].scay0;
		y1 = scissor[1].scay1;
	}
	h = pix / 2;
	sxi = Floor(sx + h);
	syi = Floor(sy + h);
	ddax = Floor((sxi << 4) >> 1) * 2;
	dday = Floor((syi << 4) >> 1) * 2;
	if (sxi < x0)
		bbl = ddax - x0;
	else
		bbl = ddax - sxi;
	bbl = bbl - 1 + stampw;
	if (syi < y0)
		bbt = dday - y0;
	else
		bbt = dday - syi;
	bbt = bbt + 1;
	if (ddax >= x0 && ddax <= x1)
		bbr = sxi - ddax - stampw;
	else
		bbr = -0x2000;
	if (dday >= y0 && dday <= y1)
		bbb = syi - dday - 2;
	else
		bbb = -0x2000;
	m_af0 = Floor(p->v[2].y - ((dday + 1) << 4));
	sv.z = (long long)p->v[2].z << 10;
	sv.r = (long long)p->v[2].r << 10;
	sv.g = (long long)p->v[2].g << 10;
	sv.b = (long long)p->v[2].b << 10;
	sv.a = (long long)p->v[2].a << 10;
	sv.f = (long long)p->v[2].f << 10;
	sv.s = (long long)p->v[2].s << 10;
	sv.t = (long long)p->v[2].t << 10;
	sv.q = (long long)p->v[2].q << 10;
}

/* Clip the primitive's bounding box against the scissor and express it
 * as distances from the DDA start position. */
void
PCalc::BBox(void)
{
	int x0, x1, y0, y1;

	if (CTXT == 0) {
		x0 = scissor[0].scax0;
		x1 = scissor[0].scax1;
		y0 = scissor[0].scay0;
		y1 = scissor[0].scay1;
	} else {
		x0 = scissor[1].scax0;
		x1 = scissor[1].scax1;
		y0 = scissor[1].scay0;
		y1 = scissor[1].scay1;
	}
	if (xdir == 0) {
		if (sxi < x0)
			bbl = ddax - x0;
		else
			bbl = ddax - sxi;
		if (exi < x1)
			bbr = exi - ddax;
		else
			bbr = x1 - ddax;
	} else {
		if (sxi < x1)
			bbl = sxi - ddax;
		else
			bbl = x1 - ddax;
		if (exi < x0)
			bbr = ddax - x0;
		else
			bbr = ddax - exi;
	}
	if (ydir == 0) {
		if (syi < y0)
			bbt = dday - y0;
		else
			bbt = dday - syi;
		if (eyi < y1)
			bbb = eyi - dday;
		else
			bbb = y1 - dday;
	} else {
		if (syi < y1)
			bbt = syi - dday;
		else
			bbt = y1 - dday;
		if (eyi < y0)
			bbb = dday - y0;
		else
			bbb = dday - eyi;
	}
	bbl = bbl + stampw - 1;
	bbr = bbr - stampw;
	bbt = bbt + 1;
	bbb = bbb - 2;
}

void
PCalc::ReverseDir(void)
{
	if (xdir == 1) {
		ddx[0] = -ddx[0];
		ddx[1] = -ddx[1];
		ddx[2] = -ddx[2];
		covdx[0] = -covdx[0];
		covdx[1] = -covdx[1];
		covdx[2] = -covdx[2];
		dx = dx * -1;
	}
	if (ydir == 1) {
		ddy[0] = -ddy[0];
		ddy[1] = -ddy[1];
		ddy[2] = -ddy[2];
		covdy[0] = -covdy[0];
		covdy[1] = -covdy[1];
		covdy[2] = -covdy[2];
		dy = dy * -1;
	}
}

void
PCalc::Primitive(Pre3 *p)
{
	flat = p->AA1 == 0 && p->m_120 == 0 && p->FGE == 0 && p->TME == 0;
	stampw = flat ? 8 : 4;
	maxexp = p->maxexp;
	FIX = p->FIX;
	m_bf4 = 0;
	type = p->type;
	if (p->type == 2) {
		if (p->area == 0)
			return;
		DrawTriangle(p);
		ReverseDir();
		covdx[0] >>= 1;
		covdx[1] >>= 1;
		covdx[2] >>= 1;
		covdy[0] >>= 1;
		covdy[1] >>= 1;
		covdy[2] >>= 1;
		covdx[0] &= 0x3ffff;
		covdx[1] &= 0x3ffff;
		covdx[2] &= 0x3ffff;
		covdy[0] &= 0x3ffff;
		covdy[1] &= 0x3ffff;
		covdy[2] &= 0x3ffff;
	} else if (p->type == 1) {
		if (p->dxzero[1] == 1 && p->dyzero[1] == 1)
			return;
		DrawLine(p);
		ReverseDir();
		covdx[0] >>= 1;
		covdx[1] >>= 1;
		covdy[0] >>= 1;
		covdy[1] >>= 1;
		covdx[0] &= 0x3ffff;
		covdx[1] &= 0x3ffff;
		covdy[0] &= 0x3ffff;
		covdy[1] &= 0x3ffff;
	} else if (p->type == 3) {
		DrawSprite(p);
		if (xdir == 1) {
			ddx[0] = -ddx[0];
			ddx[1] = -ddx[1];
			ddx[2] = -ddx[2];
			covdx[0] = -covdx[0];
			covdx[1] = -covdx[1];
			covdx[2] = -covdx[2];
			dx = dx * -1;
		}
		if (ydir == 1) {
			ddy[0] = -ddy[0];
			ddy[1] = -ddy[1];
			ddy[2] = -ddy[2];
			covdy[0] = -covdy[0];
			covdy[1] = -covdy[1];
			covdy[2] = -covdy[2];
			dy = dy * -1;
		}
	} else if (p->type == 0)
		DrawPoint(p);
	ddax = (unsigned int)ddax >> 1;
	dday = (unsigned int)dday >> 1;
	covs[0] >>= 1;
	covs[1] >>= 1;
	covs[2] >>= 1;
	ozv = sv.z >> 1;
	ofv = sv.f >> 1;
	oav = sv.a >> 1;
	orv = sv.r >> 1;
	ogv = sv.g >> 1;
	obv = sv.b >> 1;
	osv = sv.s;
	otv = sv.t;
	oqv = sv.q;
	ozdx = dx.z;
	ofdx = dx.f;
	oadx = dx.a;
	ordx = dx.r;
	ogdx = dx.g;
	obdx = dx.b;
	osdx = dx.s;
	otdx = dx.t;
	oqdx = dx.q;
	ozdy = dy.z;
	ofdy = dy.f;
	oady = dy.a;
	ordy = dy.r;
	ogdy = dy.g;
	obdy = dy.b;
	osdy = dy.s;
	otdy = dy.t;
	oqdy = dy.q;
	out->Put(this);
}

void
PCalc::Register(Pre3 *p)
{
	ddax = p->send_addr >> 1;
	xdir = p->send_addr & 1;
	ozv = ((p->send_reg >> 40) & 0xffffff) << 9;
	ofv = ((p->send_reg >> 32) & 0xff) << 9;
	oav = ((p->send_reg >> 24) & 0xff) << 9;
	obv = ((p->send_reg >> 16) & 0xff) << 9;
	ogv = ((p->send_reg >> 8) & 0xff) << 9;
	orv = (p->send_reg & 0xff) << 9;
	if (p->send_addr == 0x40) {
		scissor[0].scax0 = p->send_reg & 0x7ff;
		scissor[0].scax1 = (p->send_reg >> 16) & 0x7ff;
		scissor[0].scay0 = (p->send_reg >> 32) & 0x7ff;
		scissor[0].scay1 = (p->send_reg >> 48) & 0x7ff;
	} else if (p->send_addr == 0x41) {
		scissor[1].scax0 = p->send_reg & 0x7ff;
		scissor[1].scax1 = (p->send_reg >> 16) & 0x7ff;
		scissor[1].scay0 = (p->send_reg >> 32) & 0x7ff;
		scissor[1].scay1 = (p->send_reg >> 48) & 0x7ff;
	} else if (p->send_addr == 0x22)
		SCANMSK = p->send_reg & 3;
	out->Put(this);
}

void
PCalc::Put(Pre3 *p)
{
	send_type = p->send_type;
	send_addr = p->send_addr;
	send_reg = p->send_reg;
	TME = p->TME;
	FGE = p->FGE;
	ABE = p->ABE;
	AA1 = p->AA1;
	m_bd4 = p->m_120;
	CTXT = p->CTXT;
	FST = p->FST;
	if (p->send_type)
		Register(p);
	else
		Primitive(p);
}
