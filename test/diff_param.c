/* diff_param - differential test: Sony's param.o vs src/param.c.
 *
 * Both objects are linked into one i386 binary with their (identical)
 * mangled symbols renamed by objcopy (old_/new_, see run_param.sh).
 *
 * param is 0x50 bytes and every whole-object operator returns one by
 * value, so each takes a hidden return-slot pointer as its first
 * argument; `this' (or the first operand) follows.  operator/ pulls in
 * __divdi3, which is supplied here so the test needs no libgcc.
 */
#include <stdio.h>
#include <string.h>

typedef struct {
	int x, y;
	long long z, r, g, b, a, f, s, t, q;
} param;

/* libgcc's 64-bit divide, open-coded so both objects can call it */
static unsigned long long
udiv64(unsigned long long a, unsigned long long b)
{
	unsigned long long q, r;
	int i;

	q = 0;
	r = 0;
	for (i = 63; i >= 0; i--) {
		r = (r << 1) | ((a >> i) & 1);
		if (r >= b) {
			r -= b;
			q |= (unsigned long long)1 << i;
		}
	}
	return q;
}

long long
__divdi3(long long a, long long b)
{
	unsigned long long ua, ub, q;
	int neg;

	neg = 0;
	if (a < 0) { ua = -(unsigned long long)a; neg ^= 1; } else ua = a;
	if (b < 0) { ub = -(unsigned long long)b; neg ^= 1; } else ub = b;
	q = udiv64(ua, ub);
	return neg ? -(long long)q : (long long)q;
}

extern param *old_ctor(param *), *new_ctor(param *);
extern param *old_asn(param *, param *, const param *);
extern param *new_asn(param *, param *, const param *);
extern void old_setxy(param *, const param *), new_setxy(param *, const param *);
extern void old_ifminus(param *, param *), new_ifminus(param *, param *);
extern void old_getabs(param *), new_getabs(param *);
extern void old_shift(param *, int), new_shift(param *, int);
extern param *old_shl(param *, const param *, int);
extern param *new_shl(param *, const param *, int);
extern param *old_shlr(param *, const param *, const int *);
extern param *new_shlr(param *, const param *, const int *);
extern param *old_shr(param *, const param *, int);
extern param *new_shr(param *, const param *, int);
extern param *old_add(param *, const param *, const param *);
extern param *new_add(param *, const param *, const param *);
extern param *old_sub(param *, const param *, const param *);
extern param *new_sub(param *, const param *, const param *);
extern param *old_mul(param *, const param *, const param *);
extern param *new_mul(param *, const param *, const param *);
extern param *old_muli(param *, const param *, int);
extern param *new_muli(param *, const param *, int);
extern param *old_divi(param *, const param *, int);
extern param *new_divi(param *, const param *, int);

static long mismatch, calls;

static void
chk(const char *what, param *a, param *b)
{
	calls++;
	if (memcmp(a, b, sizeof *a) != 0 && mismatch++ < 8) {
		int i;
		printf("MISMATCH %s\n", what);
		for (i = 0; i < 20; i++)
			if (((unsigned *)a)[i] != ((unsigned *)b)[i])
				printf("  word %d: %08x != %08x\n", i,
				    ((unsigned *)a)[i], ((unsigned *)b)[i]);
	}
}

static unsigned s = 13579;
static unsigned
rnd(void)
{
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	return s;
}

static void
fill(param *p)
{
	int i;

	for (i = 0; i < 20; i++)
		((unsigned *)p)[i] = rnd();
	/* keep some fields small/zero/extreme too */
	if (rnd() & 1)
		p->z = (long long)(int)rnd();
	if (rnd() & 1)
		p->q = 0;
	if (rnd() & 1)
		p->a = (long long)1 << (rnd() % 63);
}

int
main(void)
{
	param p, o, ao, an, ro, rn;
	int i, n, sh;

	if (sizeof(param) != 0x50) {
		printf("host param layout is %d bytes, expected 80\n",
		    (int)sizeof(param));
		return 1;
	}

	memset(&ao, 0x77, sizeof ao);
	memset(&an, 0x77, sizeof an);
	old_ctor(&ao);
	new_ctor(&an);
	chk("param()", &ao, &an);

	for (i = 0; i < 300000; i++) {
		fill(&p);
		fill(&o);
		n = rnd() % 63;

		ao = p; an = p;
		old_asn(&ro, &ao, &o);
		new_asn(&rn, &an, &o);
		chk("operator=", &ao, &an);
		chk("operator= ret", &ro, &rn);

		ao = p; an = p;
		old_setxy(&ao, &o);
		new_setxy(&an, &o);
		chk("SetXY", &ao, &an);

		ao = p; an = p;
		old_ifminus(&o, &ao);
		new_ifminus(&o, &an);
		chk("IfMinus", &ao, &an);

		ao = p; an = p;
		old_getabs(&ao);
		new_getabs(&an);
		chk("GetAbs", &ao, &an);

		ao = p; an = p;
		sh = (int)rnd() % 63 - 31;
		old_shift(&ao, sh);
		new_shift(&an, sh);
		chk("ShiftARGBSlope", &ao, &an);

		old_shl(&ao, &p, n);
		new_shl(&an, &p, n);
		chk("operator<<(int)", &ao, &an);

		old_shlr(&ao, &p, &n);
		new_shlr(&an, &p, &n);
		chk("operator<<(const int&)", &ao, &an);

		old_shr(&ao, &p, n);
		new_shr(&an, &p, n);
		chk("operator>>", &ao, &an);

		old_add(&ao, &p, &o);
		new_add(&an, &p, &o);
		chk("operator+", &ao, &an);

		old_sub(&ao, &p, &o);
		new_sub(&an, &p, &o);
		chk("operator-", &ao, &an);

		old_mul(&ao, &p, &o);
		new_mul(&an, &p, &o);
		chk("operator*", &ao, &an);

		sh = (int)rnd();
		old_muli(&ao, &p, sh);
		new_muli(&an, &p, sh);
		chk("operator*(int)", &ao, &an);

		n = (int)rnd();
		if (n == 0)
			n = 1;
		old_divi(&ao, &p, n);
		new_divi(&an, &p, n);
		chk("operator/", &ao, &an);
	}
	printf("%ld calls, %ld mismatches\n", calls, mismatch);
	return mismatch != 0;
}
