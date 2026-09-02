/* Differential test: reconstructed dda.o against the 1998 object.
 *
 * Both DDAs are driven with the same randomised PCalc output block
 * through DDA::Put.  The DDA forwards to TXM through DDATXM's vtable;
 * we install a fake DDATXM whose single virtual entry snapshots the
 * whole DDA object, so every stamp the real TXM would have seen is
 * compared - position, pixel mask, both scanlines' Z/RGBA/STQ, the AA
 * coverage block and all the flags.  The DDA object itself is compared
 * after every call as well.
 *
 * Built by test/run_dda.sh, which renames the 1998 symbols to o_* and
 * the reconstructed ones to n_* first.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PCALC_SIZE	0xc00
#define DDA_SIZE	0x254
#define DDA_VPTR	0x250

/* g++ 2.7 non-thunk vtable: 8 zero bytes, then {short delta; short pad; fn} */
struct vtable {
	int zero0, zero1;
	short delta;
	short pad;
	void (*fn)(void *txm, void *dda);
};

extern void *o_dda_ctor(void *dda, void *txm);
extern void o_dda_put(void *dda, void *pcalc);
extern void *n_dda_ctor(void *dda, void *txm);
extern void n_dda_put(void *dda, void *pcalc);

/* --- the fake DDATXM ------------------------------------------------ */

#define MAXCAP	4096
static unsigned char cap[2][MAXCAP][DDA_SIZE];
static int ncap[2];
static int side;

static void
capture(void *txm, void *dda)
{
	(void)txm;
	if (ncap[side] < MAXCAP)
		memcpy(cap[side][ncap[side]], dda, DDA_SIZE);
	ncap[side]++;
}

static struct vtable txm_vt;
static void *txm_obj[2];

/* --- PCalc field offsets the DDA reads ------------------------------ */

#define F(p, off)	(*(int *)((char *)(p) + (off)))
#define FL(p, off)	(*(long long *)((char *)(p) + (off)))

static unsigned int seed = 20260902;

static unsigned int
rnd(void)
{
	seed = seed*1103515245 + 12345;
	return seed >> 8;
}

static int
rndrange(int lo, int hi)
{
	return lo + (int)(rnd() % (unsigned)(hi - lo + 1));
}

/* A slope-shaped value: mostly small, sometimes extreme. */
static int
rndslope(void)
{
	switch (rnd() % 8) {
	case 0: return (int)rnd();
	case 1: return -(int)(rnd() & 0xfffff);
	case 2: return 0;
	default: return rndrange(-0x40000, 0x40000);
	}
}

static long long
rndslope64(void)
{
	switch (rnd() % 8) {
	case 0: return ((long long)rnd() << 20) ^ rnd();
	case 1: return -(long long)(rnd() & 0xffffff);
	case 2: return 0;
	default: return rndrange(-0x400000, 0x400000);
	}
}

static void
fillpcalc(void *p, int prim)
{
	int i;

	memset(p, 0, PCALC_SIZE);

	/* the three edge functions and their gradients */
	F(p, 0xabc) = rndrange(-0x80, 0x600);
	F(p, 0xac0) = rndrange(-0x80, 0x600);
	F(p, 0xac4) = rndrange(-0x80, 0x600);
	for (i = 0; i < 3; i++) {
		F(p, 0xac8 + i*4) = rndrange(-0x18, 0x18);
		F(p, 0xad4 + i*4) = rndrange(-0x18, 0x18);
	}

	/* clipped bounding box, as distances - keep the walk bounded */
	F(p, 0xae0) = rndrange(-2, 64);		/* bbl */
	F(p, 0xae4) = rndrange(-2, 32);		/* bbt */
	F(p, 0xae8) = rndrange(-2, 48);		/* bbr */
	F(p, 0xaec) = rndrange(-2, 24);		/* bbb */
	F(p, 0xaf0) = rndrange(-8, 24);		/* scanlines left */
	F(p, 0xaf4) = rndrange(0, 0x140);	/* ddax */
	F(p, 0xaf8) = rndrange(0, 0xf0);	/* dday */

	for (i = 0; i < 3; i++) {
		F(p, 0xafc + i*4) = (int)rnd();		/* covs */
		F(p, 0xb08 + i*4) = rndslope();		/* covdx */
		F(p, 0xb14 + i*4) = rndslope();		/* covdy */
	}

	FL(p, 0xb20) = rndslope64();		/* z */
	for (i = 0; i < 8; i++)
		F(p, 0xb28 + i*4) = rndslope();	/* f a r g b s t q */
	FL(p, 0xb48) = rndslope64();		/* dz/dx */
	for (i = 0; i < 8; i++)
		F(p, 0xb50 + i*4) = rndslope();
	FL(p, 0xb70) = rndslope64();		/* dz/dy */
	for (i = 0; i < 8; i++)
		F(p, 0xb78 + i*4) = rndslope();

	F(p, 0xb98) = rnd() & 1;		/* xdir */
	F(p, 0xb9c) = rnd() & 1;		/* ydir */
	for (i = 0; i < 3; i++)
		F(p, 0xba0 + i*4) = rnd() & 1;	/* steep */
	F(p, 0xbac) = rnd() & 1;		/* flat */
	F(p, 0xbb0) = rnd() % 4 == 0 ? (int)(rnd() & 7) : 0;	/* SCANMSK */

	F(p, 0xbb4) = prim ? 0 : 1;		/* send_type */
	F(p, 0xbb8) = (int)(rnd() & 0x1ff);	/* send_addr */
	FL(p, 0xbbc) = ((long long)rnd() << 32) ^ rnd();

	F(p, 0xbc4) = rnd() & 1;		/* TME */
	F(p, 0xbc8) = rnd() & 1;		/* FGE */
	F(p, 0xbcc) = rnd() & 1;		/* ABE */
	F(p, 0xbd0) = rnd() & 1;		/* AA1 */
	F(p, 0xbd4) = rnd() & 1;
	F(p, 0xbd8) = rnd() & 1;		/* CTXT */
	F(p, 0xbdc) = rnd() & 1;		/* FST */
	F(p, 0xbe0) = (int)(rnd() & 0xf);	/* maxexp */
	F(p, 0xbf8) = (int)(rnd() & 3);		/* type */
}

/* The two instances legitimately differ in their pointer fields:
   DDA+0x00 = PCalc*, DDA+0x04 = DDATXM*, DDA+0x250 = its own _vt.3DDA. */
static int
cmpdda(const void *a, const void *b)
{
	return memcmp((const char *)a + 8, (const char *)b + 8,
		DDA_VPTR - 8);
}

static void
dump(const unsigned char *a, const unsigned char *b)
{
	int i;

	for (i = 8; i < DDA_VPTR; i += 4)
		if (memcmp(a + i, b + i, 4) != 0)
			printf("    +0x%03x: orig %08x  new %08x\n", i,
				*(unsigned int *)(a + i),
				*(unsigned int *)(b + i));
}

int
main(int argc, char **argv)
{
	static unsigned char pc[PCALC_SIZE];
	static unsigned char dda[2][DDA_SIZE];
	int n, iter, i, bad, fail;
	long long stamps;

	iter = argc > 1 ? atoi(argv[1]) : 200000;

	txm_vt.delta = 0;
	txm_vt.fn = capture;
	txm_obj[0] = &txm_vt;
	txm_obj[1] = &txm_vt;

	fail = 0;
	stamps = 0;
	for (n = 0; n < iter; n++) {
		fillpcalc(pc, (int)(rnd() % 8) != 0);

		memset(dda[0], 0, DDA_SIZE);
		memset(dda[1], 0, DDA_SIZE);
		o_dda_ctor(dda[0], &txm_obj[0]);
		n_dda_ctor(dda[1], &txm_obj[1]);

		ncap[0] = ncap[1] = 0;
		side = 0;
		o_dda_put(dda[0], pc);
		side = 1;
		n_dda_put(dda[1], pc);

		bad = 0;
		if (ncap[0] != ncap[1]) {
			printf("iter %d: TXM call count %d vs %d\n", n,
				ncap[0], ncap[1]);
			bad = 1;
		} else {
			for (i = 0; i < ncap[0] && i < MAXCAP; i++)
				if (cmpdda(cap[0][i], cap[1][i]) != 0) {
					printf("iter %d: TXM call %d differs\n",
						n, i);
					dump(cap[0][i], cap[1][i]);
					bad = 1;
					break;
				}
		}
		if (!bad && cmpdda(dda[0], dda[1]) != 0) {
			printf("iter %d: final DDA state differs\n", n);
			dump(dda[0], dda[1]);
			bad = 1;
		}
		stamps += ncap[0];
		if (bad && ++fail >= 10)
			break;
	}
	printf("%d iterations, %lld TXM calls, %d mismatches\n",
		n, stamps, fail);
	return fail != 0;
}
