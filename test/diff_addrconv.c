/* diff_addrconv - differential test: Sony's addrconv.o vs src/addrconv.c.
 *
 * Both objects get their (identical) mangled symbol renamed via objcopy
 * (old_ac / new_ac, see run_addrconv.sh) and are linked into this one
 * i386 binary.  this is passed as the first cdecl arg; AddrConv has no
 * state, so NULL serves.  int& == pointer at ABI level.
 *
 * Sweeps every valid psm over an exhaustive (x,y) grid for several
 * (bw,tbp) pairs, then random parameters incl. negatives.  Exits 1 on
 * the first psm with mismatches.
 */
#include <stdio.h>

/* neither implementation's error path runs (only valid psms are fed),
 * but the linker wants the old libio object both reference */
char _IO_stderr_[256];

typedef void convfn(void *thisp, int x, int y, int psm, int bw, int tbp,
    int *page, int *blk, int *bnk, int *pos, int *wd, int *np);
extern convfn old_ac, new_ac;

static const int psms[] = {0x00, 0x01, 0x02, 0x0a, 0x13, 0x14, 0x1b,
    0x24, 0x2c, 0x30, 0x31, 0x32, 0x3a};
static const int bwtbp[][2] = {{10, 0}, {10, 0x46}, {2, 0x3fff}, {63, 777}};

static long mismatch, calls;

static void
one(int x, int y, int psm, int bw, int tbp)
{
	int a[6], b[6], i;

	old_ac(0, x, y, psm, bw, tbp, a, a+1, a+2, a+3, a+4, a+5);
	new_ac(0, x, y, psm, bw, tbp, b, b+1, b+2, b+3, b+4, b+5);
	calls++;
	for (i = 0; i < 6; i++)
		if (a[i] != b[i]) {
			if (mismatch++ < 5)
				printf("MISMATCH x=%d y=%d psm=%02x bw=%d "
				    "tbp=%d out%d: %d != %d\n",
				    x, y, psm, bw, tbp, i, a[i], b[i]);
			return;
		}
}

static unsigned s = 12345;
static unsigned
rnd(void)
{
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	return s;
}

int
main(void)
{
	int p, c, x, y, i;
	long n;

	for (p = 0; p < 13; p++) {
		for (c = 0; c < 4; c++)
			for (y = 0; y < 1024; y++)
				for (x = 0; x < 1024; x++)
					one(x, y, psms[p],
					    bwtbp[c][0], bwtbp[c][1]);
		for (i = 0; i < 2000000; i++) {
			x = rnd() % 8192 - 2048;
			y = rnd() % 8192 - 2048;
			one(x, y, psms[p], rnd() % 64, rnd() % 0x4000);
		}
		n = mismatch;
		printf("psm %02x: %s\n", psms[p], n ? "FAIL" : "ok");
		if (n)
			return 1;
	}
	printf("%ld calls, %ld mismatches\n", calls, mismatch);
	return mismatch != 0;
}
