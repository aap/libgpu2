#include "div.h"

Reciproc::Reciproc(void)
{
	unsigned int i;
	double x, y, z;
	long long a, b;

	tbl0 = new long long[256];
	tbl1 = new long long[256];
	for (i = 0; i < 256; i++) {
		x = (i + 0.5) / 256.0;
		y = (1.0/(1.0 + x) - 0.5) * 256.0;
		z = 1.0/((1.0 + x)*(1.0 + x)) * 256.0;
		a = (long long)(y * 268435456.0);
		b = (long long)(z * 131072.0);
		a += 9;
		b += 1;
		if (i == 0) { a += 2; b -= 2; }
		if (i == 2) { a += 2; b -= 2; }
		if (i == 3) { a += 2; b -= 2; }
		if (i == 4) { a += 2; b -= 2; }
		if (i == 6) { a += 2; b -= 2; }
		if (i == 7) { a += 2; b -= 2; }
		if (i == 13) { a += 2; b -= 2; }
		if (i == 15) { a += 2; b -= 2; }
		if (i == 16) { a += 2; b -= 2; }
		if (i == 17) { a += 2; b -= 2; }
		if (i == 18) { a += 2; b -= 2; }
		if (i == 22) { a += 2; b -= 2; }
		if (i == 26) { a += 2; b -= 2; }
		if (i == 28) { a += 2; b -= 2; }
		if (i == 30) { a += 2; b -= 2; }
		if (i == 31) { a += 2; b -= 2; }
		if (i == 33) { a += 2; b -= 2; }
		if (i == 34) { a += 2; b -= 2; }
		if (i == 35) { a += 2; b -= 2; }
		if (i == 36) { a += 2; b -= 2; }
		if (i == 39) { a += 2; b -= 2; }
		if (i == 42) { a += 2; b -= 2; }
		if (i == 45) { a += 2; b -= 2; }
		if (i == 51) { a += 2; b -= 2; }
		if (i == 53) { a += 2; b -= 2; }
		if (i == 55) { a += 2; b -= 2; }
		if (i == 56) { a += 2; b -= 2; }
		if (i == 61) { a += 2; b -= 2; }
		if (i == 64) { a += 2; b -= 2; }
		if (i == 67) { a += 2; b -= 2; }
		if (i == 68) { a += 2; b -= 2; }
		if (i == 84) { a += 2; b -= 2; }
		if (i == 86) { a += 2; b -= 2; }
		if (i == 87) { a += 2; b -= 2; }
		if (i == 89) { a += 2; b -= 2; }
		if (i == 90) { a += 2; b -= 2; }
		if (i == 91) { a += 2; b -= 2; }
		if (i == 95) { a += 2; b -= 2; }
		if (i == 96) { a += 2; b -= 2; }
		if (i == 109) { a += 2; b -= 2; }
		if (i == 112) { a += 2; b -= 2; }
		if (i == 116) { a += 2; b -= 2; }
		if (i == 118) { a += 2; b -= 2; }
		if (i == 124) { a += 2; b -= 2; }
		if (i == 130) { a += 2; b -= 2; }
		if (i == 134) { a += 2; b -= 2; }
		if (i == 136) { a += 2; b -= 2; }
		if (i == 141) { a += 2; b -= 2; }
		if (i == 157) { a += 2; b -= 2; }
		if (i == 161) { a += 2; b -= 2; }
		if (i == 167) { a += 2; b -= 2; }
		if (i == 170) { a += 2; b -= 2; }
		if (i == 175) { a += 2; b -= 2; }
		if (i == 176) { a += 2; b -= 2; }
		if (i == 183) { a += 2; b -= 2; }
		if (i == 184) { a += 2; b -= 2; }
		if (i == 186) { a += 2; b -= 2; }
		if (i == 193) { a += 2; b -= 2; }
		if (i == 194) { a += 2; b -= 2; }
		if (i == 204) { a += 2; b -= 2; }
		if (i == 210) { a += 2; b -= 2; }
		if (i == 213) { a += 2; b -= 2; }
		if (i == 217) { a += 2; b -= 2; }
		if (i == 228) { a += 2; b -= 2; }
		if (i == 236) { a += 2; b -= 2; }
		if (i == 239) { a += 2; b -= 2; }
		if (i == 241) { a += 2; b -= 2; }
		if (i == 242) { a += 2; b -= 2; }
		if (i == 250) { a += 2; b -= 2; }
		if (i == 255) { a += 2; b -= 2; }
		tbl0[i] = (a + 1) >> 1;
		tbl1[i] = (b + 1) >> 1;
	}
}


void
Reciproc::reciproc(long long x, int &sft, long long &v0, long long &v1,
	unsigned int &rem)
{
	long long a, t, y, s, frac, flag, r0, r1, idx, u, mask;
	int n, m;

	mask = 0x8000000000000000LL;
	a = x;
	if (a < 0)
		a = -a;
	for (n = 63; n >= 0; n--) {
		if (a & mask)
			break;
		mask >>= 1;
	}
	if (n < 0) {
		v1 = 0;
		v0 = 0;
		sft = 0;
		return;
	}
	if (n <= 31)
		t = (a & ((1 << n) - 1)) << (31 - n);
	else
		t = (a & ((1 << n) - 1)) >> (n - 31);
	m = n;
	if ((t & 0x7fffffff) == 0) {
		v0 = 0x80000000LL;
		v1 = 0x80000000LL;
		sft = 1 - m;
		return;
	}
	idx = (t >> 23) & 0xff;
	frac = t & 0x7fffff;
	y = tbl0[idx] + 0x400000000LL;
	s = tbl1[idx];
	if (frac <= 0x3fffff) {
		frac = 0x400000 - frac;
		flag = 1;
	} else {
		frac = frac - 0x400000;
		flag = 0;
	}
	s = (s & 0xffffff) * (frac & 0x7fffff);
	if (flag == 0) {
		y = y + ~(s >> 20);
		if ((s & 0xfffff) == 0)
			y = y + 1;
	} else
		y = y + (s >> 20);
	s = s >> 25;
	r0 = y >> 3;
	s = (s & 0xffffff) * (frac & 0x7fffff);
	s = s >> 25;
	u = y >> 16;
	s = (s & 0xffffff) * (u & 0x7fffff);
	y = y + (s >> 20);
	r1 = y >> 3;
	v0 = r0;
	v1 = r1;
	sft = -m;
	rem = t;
}
