/* diff_div - differential test: Sony's div.o vs src/div.c.
 *
 * Both objects are linked into one i386 binary with their (identical)
 * mangled symbols renamed by objcopy (old_/new_, see run_div.sh).
 *
 * Reciproc::Reciproc() is a ctor: hidden `this' is the only argument and
 * it returns this.  It allocates two 256-entry long long tables with
 * __builtin_vec_new (supplied below) and fills them; the test compares
 * all 512 entries bit-exactly, which is also the test of the x87 code
 * that generates them.
 *
 * Reciproc::reciproc(long long, int&, long long&, long long&, unsigned&)
 * has no return value; references are pointers at the ABI level.
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct { long long *tbl0, *tbl1; } Reciproc;

void *
__builtin_vec_new(unsigned long n)
{
	void *p = malloc(n);
	if (p == 0) {
		printf("out of memory\n");
		exit(1);
	}
	return p;
}

extern Reciproc *old_ctor(Reciproc *);
extern Reciproc *new_ctor(Reciproc *);
extern void old_rec(Reciproc *, long long, int *, long long *,
    long long *, unsigned *);
extern void new_rec(Reciproc *, long long, int *, long long *,
    long long *, unsigned *);

static Reciproc ro, rn;
static long mismatch, calls;

static void
one(long long x)
{
	int e0, e1;
	long long a0, b0, a1, b1;
	unsigned f0, f1;

	e0 = e1 = -12345;
	a0 = a1 = b0 = b1 = -1;
	f0 = f1 = 0xdeadbeef;
	old_rec(&ro, x, &e0, &a0, &b0, &f0);
	new_rec(&rn, x, &e1, &a1, &b1, &f1);
	calls++;
	if (e0 != e1 || a0 != a1 || b0 != b1 || f0 != f1) {
		if (mismatch++ < 8)
			printf("MISMATCH x=%lld: old(%d,%lld,%lld,%u) "
			    "new(%d,%lld,%lld,%u)\n", x, e0, a0, b0, f0,
			    e1, a1, b1, f1);
	}
}

static unsigned s = 987654321;
static unsigned
rnd(void)
{
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	return s;
}

int
main(void)
{
	int i, bad;
	long long x;

	old_ctor(&ro);
	new_ctor(&rn);

	bad = 0;
	for (i = 0; i < 256; i++) {
		if (ro.tbl0[i] != rn.tbl0[i]) {
			if (bad++ < 5)
				printf("tbl0[%d]: %lld != %lld\n", i,
				    ro.tbl0[i], rn.tbl0[i]);
		}
		if (ro.tbl1[i] != rn.tbl1[i]) {
			if (bad++ < 5)
				printf("tbl1[%d]: %lld != %lld\n", i,
				    ro.tbl1[i], rn.tbl1[i]);
		}
	}
	printf("tables: %s\n", bad ? "FAIL" : "512/512 entries identical");
	if (bad)
		return 1;

	/* every power of two and its neighbourhood, both signs */
	for (i = 0; i < 64; i++) {
		x = (long long)1 << i;
		one(x); one(-x); one(x - 1); one(x + 1);
		one(-(x - 1)); one(-(x + 1));
	}
	one(0);
	one(1);
	one(-1);
	/* exhaustive over the low 20 bits, then random 64-bit */
	for (i = 0; i < (1 << 20); i++)
		one(i);
	for (i = 0; i < 4000000; i++) {
		x = ((long long)rnd() << 32) | rnd();
		one(x);
		one(x >> (rnd() & 63));
		one((long long)(int)rnd());
	}
	printf("%ld calls, %ld mismatches\n", calls, mismatch);
	return mismatch != 0;
}
