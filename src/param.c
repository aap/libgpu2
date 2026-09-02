/* param - PCalc's 64-bit fixed-point parameter set.  See include/param.h.
 *
 * The per-field statement order differs between the operator groups and
 * is pinned by the object: the shifts run z,a,f,r,g,b,s,t,q while the
 * arithmetic operators run z,f,r,g,b,a,s,t,q.  The shift operators put
 * the shifted value through a source-level temporary before storing it
 * into the result (`v = p.z << n; d.z = v;'), which is what makes gcc
 * spill it - assigning the shift straight into d.z is 12 bytes shorter
 * per field and does not match.
 *
 * ShiftARGBSlope() ends with an unconditional extra >>2 of all five
 * fields, on top of the requested shift: the slopes it scales are
 * per-2x2-stamp, and this converts them to per-pixel.
 */
#include <stdlib.h>

#include "param.h"

param::param(void)
{
	x = 0;
	y = 0;
	z = 0;
	r = 0;
	g = 0;
	b = 0;
	a = 0;
	f = 0;
	s = 0;
	t = 0;
	q = 0;
}

param
param::operator=(const param &p)
{
	z = p.z;
	r = p.r;
	g = p.g;
	b = p.b;
	a = p.a;
	f = p.f;
	s = p.s;
	t = p.t;
	q = p.q;
	return *this;
}

void
param::SetXY(const param &p)
{
	x = p.x;
	y = p.y;
}

void
param::IfMinus(param &p)
{
	p.z = z >= 0 ? 1 : -1;
	p.r = r >= 0 ? 1 : -1;
	p.g = g >= 0 ? 1 : -1;
	p.b = b >= 0 ? 1 : -1;
	p.a = a >= 0 ? 1 : -1;
	p.f = f >= 0 ? 1 : -1;
	p.s = s >= 0 ? 1 : -1;
	p.t = t >= 0 ? 1 : -1;
	p.q = q >= 0 ? 1 : -1;
}

void
param::GetAbs(void)
{
	z = z > 0 ? z : -z;
	r = r > 0 ? r : -r;
	g = g > 0 ? g : -g;
	b = b > 0 ? b : -b;
	a = a > 0 ? a : -a;
	f = f > 0 ? f : -f;
	s = s > 0 ? s : -s;
	t = t > 0 ? t : -t;
	q = q > 0 ? q : -q;
}

void
param::ShiftARGBSlope(int n)
{
	if (n >= 0) {
		r = r >> n;
		g = g >> n;
		b = b >> n;
		a = a >> n;
		f = f >> n;
	} else {
		n = abs(n);
		r = r << n;
		g = g << n;
		b = b << n;
		a = a << n;
		f = f << n;
	}
	r = r >> 2;
	g = g >> 2;
	b = b >> 2;
	a = a >> 2;
	f = f >> 2;
}

param
operator<<(const param &p, int n)
{
	param d;
	long long v;

	v = p.z << n;
	d.z = v;
	v = p.a << n;
	d.a = v;
	v = p.f << n;
	d.f = v;
	v = p.r << n;
	d.r = v;
	v = p.g << n;
	d.g = v;
	v = p.b << n;
	d.b = v;
	v = p.s << n;
	d.s = v;
	v = p.t << n;
	d.t = v;
	v = p.q << n;
	d.q = v;
	return d;
}

param
operator<<(const param &p, const int &n)
{
	param d;
	long long v;

	v = p.z << n;
	d.z = v;
	v = p.a << n;
	d.a = v;
	v = p.f << n;
	d.f = v;
	v = p.r << n;
	d.r = v;
	v = p.g << n;
	d.g = v;
	v = p.b << n;
	d.b = v;
	v = p.s << n;
	d.s = v;
	v = p.t << n;
	d.t = v;
	v = p.q << n;
	d.q = v;
	return d;
}

param
operator>>(const param &p, int n)
{
	param d;
	long long v;

	v = p.z >> n;
	d.z = v;
	v = p.a >> n;
	d.a = v;
	v = p.f >> n;
	d.f = v;
	v = p.r >> n;
	d.r = v;
	v = p.g >> n;
	d.g = v;
	v = p.b >> n;
	d.b = v;
	v = p.s >> n;
	d.s = v;
	v = p.t >> n;
	d.t = v;
	v = p.q >> n;
	d.q = v;
	return d;
}

param
operator+(const param &p, const param &o)
{
	param d;

	d.z = p.z + o.z;
	d.f = p.f + o.f;
	d.r = p.r + o.r;
	d.g = p.g + o.g;
	d.b = p.b + o.b;
	d.a = p.a + o.a;
	d.s = p.s + o.s;
	d.t = p.t + o.t;
	d.q = p.q + o.q;
	return d;
}

param
operator-(const param &p, const param &o)
{
	param d;

	d.z = p.z - o.z;
	d.f = p.f - o.f;
	d.r = p.r - o.r;
	d.g = p.g - o.g;
	d.b = p.b - o.b;
	d.a = p.a - o.a;
	d.s = p.s - o.s;
	d.t = p.t - o.t;
	d.q = p.q - o.q;
	return d;
}

param
operator*(const param &p, const param &o)
{
	param d;

	d.z = p.z * o.z;
	d.f = p.f * o.f;
	d.r = p.r * o.r;
	d.g = p.g * o.g;
	d.b = p.b * o.b;
	d.a = p.a * o.a;
	d.s = p.s * o.s;
	d.t = p.t * o.t;
	d.q = p.q * o.q;
	return d;
}

param
operator*(const param &p, int n)
{
	param d;

	d.z = p.z * n;
	d.f = p.f * n;
	d.r = p.r * n;
	d.g = p.g * n;
	d.b = p.b * n;
	d.a = p.a * n;
	d.s = p.s * n;
	d.t = p.t * n;
	d.q = p.q * n;
	return d;
}

param
operator/(const param &p, int n)
{
	param d;

	d.z = p.z / n;
	d.f = p.f / n;
	d.r = p.r / n;
	d.g = p.g / n;
	d.b = p.b / n;
	d.a = p.a / n;
	d.s = p.s / n;
	d.t = p.t / n;
	d.q = p.q / n;
	return d;
}

