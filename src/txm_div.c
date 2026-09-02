/* The 1998 <math.h> marked the pure math functions __const__ (libc5
 * style); the era header set we build with does not, and the attribute
 * is load-bearing for byte matching - gcc 2.7 pops the arguments of a
 * const call immediately instead of deferring the adjustment. */
extern "C" double rint(double) __attribute__((__const__));
extern "C" double fabs(double) __attribute__((__const__));

#include "txm_div.h"

int NormTexCoord::OFFSET_TBL[128];
int NormTexCoord::SLOPE_TBL[128];
int NormTexCoord::table_init;

void
NormTexCoord::TexDiv(int u, int q, int f)
{
	int n, e, i;
	unsigned int m;

	if (q == 1) {
		n = 0;
		m = 0;
	} else {
		for (n = 15; n >= 0; n--)
			if ((q >> 1) & (1 << n))
				break;
		n = n < 0 ? 0 : n;
		m = ((q << (15 - n)) >> 1) & 0x7fff;
	}
	if (f == 0) {
		e = -n;
		if (m == 0)
			re = e + 6;
		else
			re = e + 5;
		i = (m >> 6) & 0x1fc;
		rm = (*(int *)((char *)OFFSET_TBL + i) -
		    ((m & 0xff) * *(int *)((char *)SLOPE_TBL + i) >> 6))
		    >> 1 & 0x7fff;
		qzero = 0;
	} else {
		re = 0;
		rm = 0;
		qzero = 1;
	}
	if (u < 0) {
		u = -u;
		sign = 1;
	} else
		sign = 0;
	if (u == 1) {
		ue = 0;
		um = 0;
	} else {
		for (ue = 15; ue >= 0; ue--)
			if ((u >> 1) & (1 << ue))
				break;
		ue = ue < 0 ? 0 : ue;
		um = ((u << (15 - ue)) >> 2) & 0x7fff;
	}
}

void
NormTexCoord::InitTable(void)
{
	if (table_init == 0) {
		mktable();
		table_init = 1;
	}
}

void
NormTexCoord::mktable(void)
{
	double c, min, d;
	int x, offset, slope, best_slope, best_offset, i;
	double a, b, e;

	best_slope = 0;
	best_offset = 0;
	for (x = 0x8000; x <= 0xffff; x += 0x100) {
		min = 1.0;
		d = 0.0;
		do {
			b = 1.0/(x/32768.0 + 0.0078125);
			a = 1.0/(x/32768.0);
			c = b + d;
			offset = (int)rint((a + d) * 131072.0);
			slope = (int)rint((offset/131072.0 - c)/0.0078125*256.0);
			offset = (offset + 1) & 0xffff;
			slope = slope & 0xff;
			e = evalute(x, offset, slope);
			if (e < min) {
				min = e;
				best_slope = slope;
				best_offset = offset;
			}
			d = d - 0.000001;
		} while (-0.0001 < d);
		i = (x >> 6) & 0x1fc;
		*(int *)((char *)SLOPE_TBL + i) = best_slope;
		*(int *)((char *)OFFSET_TBL + i) = best_offset;
	}
}

double
NormTexCoord::evalute(int x, int off, int slope)
{
	double max, v;
	int i, y;

	max = 0.0;
	for (i = x; i < x + 0x100; i++) {
		y = cal_y(i, off, slope);
		v = fabs(y/65536.0 - 1.0/(i/32768.0));
		if (v > max)
			max = v;
	}
	return max;
}

int
NormTexCoord::cal_y(int x, int off, int slope)
{
	int y;

	y = (off - ((unsigned char)x * slope >> 6)) >> 1 & 0xffff;
	if ((x & 0x7fff) == 0)
		y = y | 0x10000;
	else
		y = y | 0x8000;
	return y;
}
