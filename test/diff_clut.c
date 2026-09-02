/* diff_clut - differential test: Sony's clut.o vs src/clut.c.
 *
 * Both objects have their four mangled TexClut entry points renamed apart
 * with objcopy (o_* / n_*, see run_clut.sh) and are linked into one i386
 * binary together with the *original* addrconv.o (byte-identical to ours,
 * so a single shared copy is honest).
 *
 * A TexClut is 0x498 bytes: AddrConv head 0x00-0x1f, MemIF* at 0x20,
 * clut[256] at 0x24, cbp0/cbp1 at 0x424/0x428, then three 0x24-byte
 * ClutAttr's at 0x42c/0x450/0x474.  We build two of them side by side,
 * point both at the same 16 MB fake local memory, and compare *every*
 * byte of the object after each call - including the trailing slack,
 * because load1 really does run off the end of clut[] when CSA is large
 * (see doc/notes/clut.md).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the era libio object both halves reference on their error paths (never
 * taken here: we only feed parameter combinations the model accepts) */
char _IO_stderr_[256];

#define OBJSZ	0x1200			/* 0x498 + the slack load1/Lookup run into */
#define VRAMW	(4*1024*1024)		/* words - 4x the real 4 MB, so an
					   out-of-range CBP still reads real
					   memory instead of faulting */

struct ClutAttr {
	int CBP, CBW, PSM, CPSM, CSM, COU, COV, CSA, CLD;
};

typedef void loadfn(void *thisp, struct ClutAttr *a);
typedef int lookfn(void *thisp, int i);

extern loadfn o_load1, n_load1, o_load2, n_load2, o_LoadData, n_LoadData;
extern lookfn o_Lookup, n_Lookup;

static int *vram;
static void *memif[2];			/* MemIF: [0] = Memory* */
static unsigned char oo[OBJSZ], nn[OBJSZ];

static long calls, mismatch;

static unsigned s = 987654321;
static unsigned
rnd(void)
{
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	return s;
}

static void
init(void)
{
	memset(oo, 0, sizeof oo);
	memset(nn, 0, sizeof nn);
	*(void **)(oo + 0x20) = memif;
	*(void **)(nn + 0x20) = memif;
}

static int
cmpobj(const char *what, struct ClutAttr *a)
{
	int i;

	calls++;
	if (memcmp(oo, nn, OBJSZ) == 0)
		return 0;
	if (mismatch++ < 5) {
		printf("MISMATCH in %s: CBP=%d CBW=%d PSM=%02x CPSM=%02x "
		    "CSM=%d COU=%d COV=%d CSA=%d CLD=%d\n", what,
		    a->CBP, a->CBW, a->PSM, a->CPSM, a->CSM,
		    a->COU, a->COV, a->CSA, a->CLD);
		for (i = 0; i < OBJSZ; i += 4)
			if (*(int *)(oo+i) != *(int *)(nn+i))
				printf("   +%04x: %08x != %08x\n", i,
				    *(int *)(oo+i), *(int *)(nn+i));
	}
	return 1;
}

/* random but *legal* attributes; load2 exits(1) on the illegal ones */
static void
mkattr(struct ClutAttr *a, int csm)
{
	a->CBP = (rnd() % 0x4000) * 64;
	a->CBW = (rnd() % 64) * 64;
	a->PSM = rnd() % 64;
	a->CPSM = (rnd() & 1) ? 0 : ((rnd() & 1) ? 0x02 : 0x0a);
	a->CSM = csm;
	a->COU = (rnd() % 64) * 16;
	a->COV = rnd() % 1024;
	a->CSA = (rnd() % 32) * 16;
	a->CLD = rnd() % 6;
	if (csm) {
		a->CSA = 0;			/* else "CSA must be 0" */
		if (a->CPSM == 0)		/* else "CPSM must be ..." */
			a->CPSM = 0x02;
	}
}

int
main(void)
{
	struct ClutAttr a;
	int i, j, r1, r2;
	long n;

	vram = malloc(VRAMW * sizeof(int));
	if (vram == 0) {
		printf("out of memory\n");
		return 1;
	}
	for (i = 0; i < VRAMW; i++)
		vram[i] = rnd();
	memif[0] = vram;

	/* 1. load1 (CSM1), every branch: 32/16 bit CLUT x 8/4 bit texture */
	for (n = 0; n < 300000; n++) {
		mkattr(&a, 0);
		init();
		o_load1(oo, &a);
		n_load1(nn, &a);
		if (cmpobj("load1", &a))
			return 1;
	}
	printf("load1: %ld calls ok\n", calls);

	/* 2. load2 (CSM2) */
	for (n = 0; n < 300000; n++) {
		mkattr(&a, 1);
		init();
		o_load2(oo, &a);
		n_load2(nn, &a);
		if (cmpobj("load2", &a))
			return 1;
	}
	printf("load2: %ld calls ok\n", calls);

	/* 3. LoadData - all six CLD codes, with the cbp0/cbp1 history kept
	 *    across calls so the "load only if changed" arms get exercised */
	init();
	for (n = 0; n < 400000; n++) {
		mkattr(&a, rnd() & 1);
		a.CLD = rnd() % 6;
		if ((rnd() & 3) == 0)		/* force CBP repeats */
			a.CBP = (rnd() % 4) * 64 * 100;
		o_LoadData(oo, &a);
		n_LoadData(nn, &a);
		if (cmpobj("LoadData", &a))
			return 1;
	}
	printf("LoadData: %ld calls ok\n", calls);

	/* 4. Lookup - exhaustive index sweep for many CLUT states */
	for (n = 0; n < 4000; n++) {
		mkattr(&a, 0);
		init();
		o_load1(oo, &a);
		n_load1(nn, &a);
		memcpy(oo + 0x42c, &a, sizeof a);
		memcpy(nn + 0x42c, &a, sizeof a);
		for (j = 0; j < 528; j++) {
			r1 = o_Lookup(oo, j);
			r2 = n_Lookup(nn, j);
			calls++;
			if (r1 != r2) {
				if (mismatch++ < 5)
					printf("MISMATCH Lookup(%d) CPSM=%02x "
					    "CSA=%d: %08x != %08x\n", j,
					    a.CPSM, a.CSA, r1, r2);
				return 1;
			}
		}
	}
	printf("%ld calls, %ld mismatches\n", calls, mismatch);
	return mismatch != 0;
}
