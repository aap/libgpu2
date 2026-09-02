/* Pre3 - GS front end stage 2: primitive assembly.
 * Reconstructed from orig/lib/pre3.o; see doc/notes/pre3.md.
 *
 * abs() is declared here rather than pulled from <stdlib.h>: the era
 * header marks it __attribute__((const)), which changes how g++ 2.7
 * pops the argument (one combined `add $8,%esp` after both calls in the
 * 1998 object vs. two separate pops with the const attribute).
 */

extern "C" int abs(int);

#include "pre1.h"
#include "pre3.h"

void
Pre3::Register(Pre1 *p)
{
	send_type = 1;
	send_addr = p->send_addr;
	send_reg = p->send_reg;
	pcalc->Put(this);
}

/* val is an IEEE-754 single shifted right by 8: bit 23 sign, bits 22:15
 * exponent, bits 14:0 the top of the mantissa.  Rebuild the mantissa with
 * its hidden bit, apply the sign, and shift it down to the primitive's
 * common exponent (Pre1::MaxExp()).  More than 14 bits of shift means the
 * value is negligible, so saturate to 0 / -1. */
int
Pre3::Float2Fix(int val, unsigned int maxexp)
{
	int sign, exp, man;

	sign = (val >> 23) & 1;
	exp = (val >> 15) & 0xff;
	man = val & 0x7fff;
	if (exp)
		man |= 0x8000;
	if (sign)
		man = -man;
	if (exp)
		man >>= 1;
	if (maxexp - exp > 14)
		man = man >= 0 ? 0 : -1;
	else {
		exp = maxexp - exp;
		man >>= exp;
	}
	return man;
}

void
Pre3::SetAttr(Pre1 *p)
{
	CTXT = p->CTXT & 1;
	FST = p->FST;
	if (p->PRIM == 0 || p->PRIM == 6)
		AA1 = 0;
	else
		AA1 = p->AA1;
	ABE = p->ABE;
	FGE = p->FGE;
	TME = p->TME;
	IIP = p->IIP;
	FIX = p->FIX;
}

/* Edge setup.  Returns 0 while the queue is still filling, 1 once the
 * primitive has been consumed (drawn, or skipped because of XYZ3/XYZF3). */
int
Pre3::Triangle(Pre1 *p)
{
	int i;

	if (nvtx != 3)
		return 0;
	if (p->nodraw == 0) {
		dx[0] = v[0].x - v[1].x;
		dx[1] = v[1].x - v[2].x;
		dx[2] = v[2].x - v[0].x;
		dy[0] = v[1].y - v[0].y;
		dy[1] = v[2].y - v[1].y;
		dy[2] = v[0].y - v[2].y;
		dxzero[0] = v[0].x == v[1].x;
		dxzero[1] = v[1].x == v[2].x;
		dxzero[2] = v[2].x == v[0].x;
		dyzero[0] = v[1].y == v[0].y;
		dyzero[1] = v[2].y == v[1].y;
		dyzero[2] = v[0].y == v[2].y;
		steep[0] = abs(v[0].x - v[1].x) < abs(v[0].y - v[1].y);
		steep[1] = abs(v[1].x - v[2].x) < abs(v[1].y - v[2].y);
		steep[2] = abs(v[2].x - v[0].x) < abs(v[2].y - v[0].y);
		area = (long long)dx[2] * dy[0] - (long long)dx[0] * dy[2];
		if (p->FST == 1) {
			for (i = 0; i != 3; i++) {
				v[i].s = S[i] * 2;
				v[i].t = T[i] * 2;
				v[i].q = Float2Fix(Q[i], p->maxexp);
			}
		} else {
			for (i = 0; i != 3; i++) {
				v[i].s = Float2Fix(S[i], p->maxexp);
				v[i].t = Float2Fix(T[i], p->maxexp);
				v[i].q = Float2Fix(Q[i], p->maxexp);
			}
		}
		pcalc->Put(this);
	}
	return 1;
}

void
Pre3::Point(Pre1 *p)
{
	if (p->nodraw == 0) {
		if (p->FST == 1) {
			v[2].s = S[2] * 2;
			v[2].t = T[2] * 2;
			v[2].q = Float2Fix(Q[2], p->maxexp);
		} else {
			v[2].s = Float2Fix(S[2], p->maxexp);
			v[2].t = Float2Fix(T[2], p->maxexp);
			v[2].q = Float2Fix(Q[2], p->maxexp);
		}
		pcalc->Put(this);
	}
}

int
Pre3::Line(Pre1 *p)
{
	int i;

	if (nvtx != 3)
		return 0;
	if (p->nodraw == 0) {
		dx[1] = v[1].x - v[2].x;
		dy[1] = v[2].y - v[1].y;
		dxzero[1] = v[1].x == v[2].x;
		dyzero[1] = v[2].y == v[1].y;
		steep[1] = abs(v[1].x - v[2].x) < abs(v[1].y - v[2].y);
		if (p->FST == 1) {
			for (i = 1; i != 3; i++) {
				v[i].s = S[i] * 2;
				v[i].t = T[i] * 2;
				v[i].q = Float2Fix(Q[i], p->maxexp);
			}
		} else {
			for (i = 1; i != 3; i++) {
				v[i].s = Float2Fix(S[i], p->maxexp);
				v[i].t = Float2Fix(T[i], p->maxexp);
				v[i].q = Float2Fix(Q[i], p->maxexp);
			}
		}
		pcalc->Put(this);
	}
	return 1;
}

int
Pre3::Sprite(Pre1 *p)
{
	return Line(p);
}

/* Push one vertex into the queue and, when it is complete, draw.  The
 * queue is loaded so that the last vertex of a primitive always lands in
 * slot 2: points use slot 2 only, lines and sprites slots 1-2, triangles
 * 0-2.  After a draw the surviving vertices are shifted down so the strip
 * / fan / line-strip continuation needs no special case downstream. */
void
Pre3::Primitive(Pre1 *p)
{
	send_type = 0;
	if (p->newprim)
		restart = 1;
	maxexp = p->maxexp;
	if (restart) {
		if (p->PRIM == 0) {
			type = 0;
			nvtx = 2;
		} else if (p->PRIM == 1 || p->PRIM == 2) {
			type = 1;
			nvtx = 1;
		} else if (p->PRIM == 6) {
			type = 3;
			nvtx = 1;
		} else {
			type = 2;
			nvtx = 0;
		}
	}
	v[nvtx].x = p->X;
	v[nvtx].y = p->Y;
	v[nvtx].z = p->Z & 0xffffffffLL;
	v[nvtx].r = p->RGBA & 0xff;
	v[nvtx].g = (p->RGBA >> 8) & 0xff;
	v[nvtx].b = (p->RGBA >> 16) & 0xff;
	v[nvtx].a = (p->RGBA >> 24) & 0xff;
	v[nvtx].f = (p->Z >> 32) & 0xff;
	S[nvtx] = p->send_U;
	T[nvtx] = p->send_V;
	Q[nvtx] = p->send_Q;
	nvtx++;
	if (p->PRIM == 0) {
		Point(p);
		nvtx = 2;
	} else if (p->PRIM == 1) {
		if (Line(p))
			nvtx = 1;
	} else if (p->PRIM == 2) {
		if (Line(p)) {
			v[1] = v[2];
			S[1] = S[2];
			T[1] = T[2];
			Q[1] = Q[2];
			nvtx = 2;
		}
	} else if (p->PRIM == 3) {
		if (Triangle(p))
			nvtx = 0;
	} else if (p->PRIM == 4) {
		if (Triangle(p)) {
			v[0] = v[1];
			v[1] = v[2];
			S[0] = S[1];
			T[0] = T[1];
			Q[0] = Q[1];
			S[1] = S[2];
			T[1] = T[2];
			Q[1] = Q[2];
			nvtx = 2;
		}
	} else if (p->PRIM == 5) {
		if (Triangle(p)) {
			v[1] = v[2];
			S[1] = S[2];
			T[1] = T[2];
			Q[1] = Q[2];
			nvtx = 2;
		}
	} else if (p->PRIM == 6) {
		if (Sprite(p)) {
			v[1] = v[2];
			S[1] = S[2];
			T[1] = T[2];
			Q[1] = Q[2];
			nvtx = 1;
		}
	}
}

void
Pre3::Put(Pre1 *p)
{
	SetAttr(p);
	if (p->send_type)
		Register(p);
	else
		Primitive(p);
}
