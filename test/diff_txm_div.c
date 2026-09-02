/* diff_txm_div - differential test: Sony's txm_div.o vs src/txm_div.c.
 *
 * Both objects are linked into one i386 binary with their (identical)
 * mangled symbols renamed by objcopy (old_/new_, see run_txm_div.sh);
 * the three static data members are renamed too, since each object
 * carries its own global copy.
 *
 * The tables are built by calling InitTable() on both sides and then
 * compared entry by entry - that is the real test of mktable()'s x87
 * code, which is where the byte residuals are.  evalute() returns a
 * double; it is compared as a bit pattern, not approximately.
 */
#include <stdio.h>
#include <string.h>

typedef struct {
	int sign, qzero, um, rm, ue, re;
} NTC;

extern void old_texdiv(NTC *, int, int, int);
extern void new_texdiv(NTC *, int, int, int);
extern void old_init(NTC *);
extern void new_init(NTC *);
extern double old_eval(NTC *, int, int, int);
extern double new_eval(NTC *, int, int, int);
extern int old_caly(NTC *, int, int, int);
extern int new_caly(NTC *, int, int, int);
extern int old_slope[128], new_slope[128];
extern int old_offset[128], new_offset[128];
extern int old_tinit, new_tinit;

static long mismatch, calls;

static void
one(int u, int q, int f)
{
	NTC a, b;

	memset(&a, 0x5a, sizeof a);
	memset(&b, 0x5a, sizeof b);
	old_texdiv(&a, u, q, f);
	new_texdiv(&b, u, q, f);
	calls++;
	if (memcmp(&a, &b, sizeof a) != 0) {
		if (mismatch++ < 8)
			printf("MISMATCH TexDiv(%d,%d,%d): old %d %d %d %d %d %d"
			    " / new %d %d %d %d %d %d\n", u, q, f,
			    a.sign, a.qzero, a.um, a.rm, a.ue, a.re,
			    b.sign, b.qzero, b.um, b.rm, b.ue, b.re);
	}
}

static unsigned s = 4242424;
static unsigned
rnd(void)
{
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	return s;
}

int
main(void)
{
	NTC o, n;
	int i, j, f, bad;
	int yo, yn;
	double eo, en;

	old_init(&o);
	new_init(&n);
	bad = 0;
	for (i = 0; i < 128; i++) {
		if (old_slope[i] != new_slope[i]) {
			if (bad++ < 5)
				printf("SLOPE_TBL[%d]: %d != %d\n", i,
				    old_slope[i], new_slope[i]);
		}
		if (old_offset[i] != new_offset[i]) {
			if (bad++ < 5)
				printf("OFFSET_TBL[%d]: %d != %d\n", i,
				    old_offset[i], new_offset[i]);
		}
	}
	if (old_tinit != 1 || new_tinit != 1) {
		printf("table_init not set (%d/%d)\n", old_tinit, new_tinit);
		return 1;
	}
	printf("tables: %s\n", bad ? "FAIL" : "256/256 entries identical");
	if (bad)
		return 1;

	/* cal_y over the whole 16-bit x range for a spread of table entries */
	for (i = 0; i < 65536; i += 7)
		for (j = 0; j < 8; j++) {
			yo = old_caly(&o, i, j * 8191, j * 37);
			yn = new_caly(&n, i, j * 8191, j * 37);
			calls++;
			if (yo != yn && mismatch++ < 8)
				printf("MISMATCH cal_y(%d,%d,%d): %d != %d\n",
				    i, j * 8191, j * 37, yo, yn);
		}
	/* evalute over every table bucket, several (offset,slope) pairs */
	for (i = 0x8000; i <= 0xffff; i += 0x100)
		for (j = 0; j < 4; j++) {
			eo = old_eval(&o, i, old_offset[(i >> 8) & 0x7f] + j,
			    old_slope[(i >> 8) & 0x7f] + j);
			en = new_eval(&n, i, new_offset[(i >> 8) & 0x7f] + j,
			    new_slope[(i >> 8) & 0x7f] + j);
			calls++;
			if (memcmp(&eo, &en, sizeof eo) != 0 &&
			    mismatch++ < 8)
				printf("MISMATCH evalute(%d,%d): %.20g != "
				    "%.20g\n", i, j, eo, en);
		}
	/* TexDiv: exhaustive over 16-bit q for a few u, then random */
	for (f = 0; f < 2; f++) {
		for (j = 0; j < 65536; j++)
			one(0x1234, j, f);
		for (j = -70000; j < 70000; j += 3)
			one(j, 0x4321, f);
		for (i = 0; i < 2000000; i++)
			one((int)rnd(), (int)rnd(), f ? (int)rnd() : 0);
	}
	printf("%ld calls, %ld mismatches\n", calls, mismatch);
	return mismatch != 0;
}
