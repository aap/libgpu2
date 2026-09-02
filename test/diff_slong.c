/* diff_slong - differential test: Sony's slong.o vs src/slong.c.
 *
 * The two objects are linked into one i386 binary with their (identical)
 * mangled symbols renamed by objcopy (old_/new_, see run_slong.sh).
 *
 * ABI notes: slong is 28 bytes and is returned by value, so every one of
 * these functions takes a hidden return-slot pointer as its FIRST
 * argument; `this' follows for the member functions.  The by-value slong
 * argument of operator<< / operator>> is pushed as 7 stack words, which
 * is what the `struct sl' below is.  The struct is described with
 * unsigned words rather than long long so that no host alignment rule
 * can perturb the layout.
 */
#include <stdio.h>

struct sl {
	unsigned hi_lo, hi_hi;		/* +0x00 unsigned long long hi */
	unsigned lo_lo, lo_hi;		/* +0x08 unsigned long long lo */
	unsigned sign;			/* +0x10 int sign */
	unsigned mask_lo, mask_hi;	/* +0x14 unsigned long long mask */
};

extern struct sl *old_mul(struct sl *r, struct sl *t, long long a, long long b);
extern struct sl *new_mul(struct sl *r, struct sl *t, long long a, long long b);
extern long long old_comb(struct sl *t);
extern long long new_comb(struct sl *t);
extern struct sl *old_shr(struct sl *r, struct sl a, int i);
extern struct sl *new_shr(struct sl *r, struct sl a, int i);
extern struct sl *old_shl(struct sl *r, struct sl a, int i);
extern struct sl *new_shl(struct sl *r, struct sl a, int i);
extern struct sl *old_asn(struct sl *r, struct sl *t, struct sl a);
extern struct sl *new_asn(struct sl *r, struct sl *t, struct sl a);

static long mismatch, calls;

static int
same(struct sl *a, struct sl *b)
{
	return a->hi_lo == b->hi_lo && a->hi_hi == b->hi_hi &&
	    a->lo_lo == b->lo_lo && a->lo_hi == b->lo_hi &&
	    a->sign == b->sign && a->mask_lo == b->mask_lo &&
	    a->mask_hi == b->mask_hi;
}

static void
show(const char *what, struct sl *a, struct sl *b)
{
	printf("MISMATCH %s: old %08x%08x %08x%08x %d / new %08x%08x "
	    "%08x%08x %d\n", what, a->hi_hi, a->hi_lo, a->lo_hi, a->lo_lo,
	    a->sign, b->hi_hi, b->hi_lo, b->lo_hi, b->lo_lo, b->sign);
}

/* a freshly "constructed" slong: only mask is set by the ctor */
static void
init(struct sl *s)
{
	s->hi_lo = s->hi_hi = s->lo_lo = s->lo_hi = 0;
	s->sign = 0;
	s->mask_lo = 0xffffffff;
	s->mask_hi = 0;
}

static void
one(long long a, long long b, int sh)
{
	struct sl o, n, ro, rn, so, sn;
	long long co, cn;

	init(&o); init(&n);
	old_mul(&ro, &o, a, b);
	new_mul(&rn, &n, a, b);
	calls++;
	if (!same(&o, &n) || !same(&ro, &rn)) {
		if (mismatch++ < 5)
			show("Multiply", &o, &n);
		return;
	}
	co = old_comb(&o);
	cn = new_comb(&n);
	if (co != cn) {
		if (mismatch++ < 5)
			printf("MISMATCH Combine: %lld != %lld\n", co, cn);
		return;
	}
	old_shr(&so, o, sh);
	new_shr(&sn, n, sh);
	if (!same(&so, &sn)) {
		if (mismatch++ < 5)
			show("operator>>", &so, &sn);
		return;
	}
	old_shl(&so, o, sh);
	new_shl(&sn, n, sh);
	if (!same(&so, &sn)) {
		if (mismatch++ < 5)
			show("operator<<", &so, &sn);
		return;
	}
	init(&so); init(&sn);
	old_asn(&ro, &so, o);
	new_asn(&rn, &sn, n);
	if (!same(&so, &sn) || !same(&ro, &rn)) {
		if (mismatch++ < 5)
			show("operator=", &so, &sn);
		return;
	}
}

static unsigned s = 22222;
static unsigned
rnd(void)
{
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	return s;
}

int
main(void)
{
	int i, j, sh;
	long long a, b;

	/* powers of two and their neighbours, both signs, all shifts 1..31 */
	for (i = 0; i < 63; i++)
		for (j = 0; j < 63; j++)
			for (sh = 1; sh < 32; sh++) {
				a = (long long)1 << i;
				b = (long long)1 << j;
				one(a, b, sh);
				one(-a, b + 1, sh);
				one(a - 1, -b, sh);
			}
	for (i = 0; i < 2000000; i++) {
		a = ((long long)rnd() << 32) | rnd();
		b = ((long long)rnd() << 32) | rnd();
		sh = 1 + rnd() % 31;
		one(a, b, sh);
		one(a >> (rnd() & 63), b >> (rnd() & 63), sh);
		one((long long)(int)rnd(), (long long)(int)rnd(), sh);
	}
	one(0, 0, 1);
	one(0, 0, 31);
	printf("%ld calls, %ld mismatches\n", calls, mismatch);
	return mismatch != 0;
}
