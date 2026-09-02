#include "slong.h"

slong
slong::Multiply(long long a, long long b)
{
	long long t;
	long long bh, bl;
	int sa, sb;

	sa = a < 0 ? -1 : 1;
	sb = b < 0 ? -1 : 1;
	sign = sa * sb;
	a = a > 0 ? a : -a;
	b = b > 0 ? b : -b;
	if (a > b) {
		t = a;
		a = b;
		b = t;
	}
	bh = b >> 32;
	bl = b & mask;
	hi = a * bh;
	lo = a * bl;
	hi += lo >> 32;
	lo &= mask;
	return *this;
}

slong
slong::operator=(slong s)
{
	hi = s.hi;
	lo = s.lo;
	sign = s.sign;
	return *this;
}

slong
operator>>(slong a, int i)
{
	slong s;
	unsigned long long t, u;
	long long mask = (1 << i) - 1;

	s = a;
	t = s.lo >> i;
	s.lo = t;
	u = s.hi & mask;
	s.lo += u << (32 - i);
	t = s.hi >> i;
	s.hi = t;
	return s;
}

slong
operator<<(slong a, int i)
{
	slong s;
	unsigned long long t, u;
	long long mask = -((long long)1 << (32 - i));

	s = a;
	t = s.hi << i;
	s.hi = t;
	u = s.lo & mask;
	s.hi += u >> (32 - i);
	t = s.lo << i;
	s.lo = t;
	s.lo &= s.mask;
	return s;
}

long long
slong::Combine(void)
{
	return ((hi << 32) + lo) * sign;
}
