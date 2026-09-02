/* Differential test: reconstructed txm.o against the 1998 object.
 *
 * Both TXMs get their own 96 MB mapped VRAM arena (the first 16 MB
 * filled identically, the rest left as zero pages for the wild reads the
 * clut[] overrun causes), their own fake MemIF - a 0xfc-byte block whose
 * only live fields are the Memory* at 0x00 and the vptr at 0xf8 - and
 * their own 0x72c-byte TXM with 0x800 bytes of slack behind it, because
 * TexClut::load1 writes clut[CSA..CSA+15] with CSA already multiplied by
 * 16 and runs off the end of the object for CSA >= 16
 * (doc/notes/clut.md).  The fake MemIF's single virtual snapshots every
 * PixelStamp TXM hands downstream, so the whole fragment path is
 * compared, not just the object.
 *
 * Phases:
 *   1  the leaf clamps and filters, called directly
 *   2  GetOneTexel over every PSM, both CLUT depths, random addresses
 *   3  register writes through TXM::Put (every register TXM decodes,
 *      plus five it only forwards)
 *   4  MTBA (TEX1.MTBA with square textures), which rewrites MIPTBP1
 *   4b the clut[] overrun with CSA >= 16, and the repair it needs
 *   5  stamps through TXM::Put: all filter modes, TME/FGE/AA1/ABE,
 *      both contexts, the LOD path and the fog path
 *   6  a deterministic sweep of the fatal arms
 *
 * The era libc5 `_IO_stderr_' does not exist on a modern host, so
 * fprintf(), exit() and __assert_fail() are defined here: exit()
 * longjmps back, which lets every fatal arm (illegal texture PSM,
 * unknown filter mode, the three MTBA complaints and SearchQlevel's
 * assert) be compared - message and status - instead of killing the run,
 * and lets the random phases generate illegal states on purpose.
 *
 * Both TXM objects are compared byte for byte (minus the vptr and the
 * two MemIF pointers) after every call, and the slack behind them with
 * them.  Built by test/run_txm.sh, which renames the 1998 symbols to
 * o_* and the reconstructed ones to n_*.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <stdarg.h>
#include <sys/mman.h>

#define TXM_SIZE	0x72c
#define TXM_SLACK	0x800		/* load1's clut[] overrun */
#define TXM_TOTAL	(TXM_SIZE + TXM_SLACK)
#define DDA_SIZE	0x254
#define STAMP_SIZE	0x3cc
#define MEMIF_SIZE	0xfc
/* The arena is much larger than the 4 MB the GS has, and mapped rather
 * than allocated: TexClut::load1's overrun (clut[CSA..CSA+15] with CSA
 * already multiplied by 16) reaches the ClutAttr it is reading from, so
 * from the corrupted iteration on the CBP it addresses with is a VRAM
 * word.  Our words are 24 bits, which bounds the resulting word address
 * at about 17.8 M; the tail stays untouched and reads as zero on both
 * sides. */
#define VRAM_WORDS	0x1800000	/* 96 MB mapped */
#define VRAM_FILL	0x400000	/* 16 MB of random texels */

#define TXM_MEMIF	0x24		/* the two MemIF pointers */
#define TXM_CLUT_MEMIF	0x278

/* g++ 2.7 non-thunk vtable: 8 zero bytes, then {short delta; short pad; fn} */
struct vtable {
	int zero0, zero1;
	short delta;
	short pad;
	void (*fn)(void *memif, void *stamp);
};

extern void *o_txm_ctor(void *txm, void *memif);
extern void o_put(void *txm, void *dda);
extern void o_getonetexel(void *txm, int u, int v, int lod, void *c);
extern int o_clampq(void *txm, int v);
extern int o_clampt(void *txm, int v);
extern int o_clamplod(void *txm, int v);
extern int o_mfilter1(void *txm, int a, int x, int y);
extern int o_lfilter1(void *txm, int a, int b, int p0, int p1, int p2, int p3);
extern void o_valid8_init(void);

extern void *n_txm_ctor(void *txm, void *memif);
extern void n_put(void *txm, void *dda);
extern void n_getonetexel(void *txm, int u, int v, int lod, void *c);
extern int n_clampq(void *txm, int v);
extern int n_clampt(void *txm, int v);
extern int n_clamplod(void *txm, int v);
extern int n_mfilter1(void *txm, int a, int x, int y);
extern int n_lfilter1(void *txm, int a, int b, int p0, int p1, int p2, int p3);
extern void n_valid8_init(void);

/* --- the era runtime the 1998 objects expect ------------------------ */

char _IO_stderr_[256];

static jmp_buf jb;
static int armed, trapped, trapcode;
static char msgbuf[1024];
static int msglen;

int
fprintf(FILE *f, const char *fmt, ...)
{
	va_list ap;

	(void)f;
	va_start(ap, fmt);
	if (msglen < (int)sizeof msgbuf - 1)
		msglen += vsnprintf(msgbuf + msglen,
			sizeof msgbuf - msglen, fmt, ap);
	va_end(ap);
	return 0;
}

void
exit(int code)
{
	if (!armed)
		_exit(code);
	trapped = 1;
	trapcode = code;
	longjmp(jb, 1);
}

/* SearchQlevel asserts that the stamp has a live centre pixel; a stamp
 * whose mask misses all eight is a fatal path like the others. */
void
__assert_fail(const char *a, const char *f, unsigned int l, const char *fn)
{
	fprintf((FILE *)_IO_stderr_, "assert %s %s %u %s\n", a, f, l, fn);
	exit(-1);
}

void
__pure_virtual(void)
{
	_exit(99);
}

/* Run one side's call, catching the fatal arms. */
#define TRAP(stmt) do {						\
		msglen = 0; msgbuf[0] = 0;			\
		trapped = 0; trapcode = 0;			\
		armed = 1;					\
		if (setjmp(jb) == 0) { stmt; }			\
		armed = 0;					\
	} while (0)

static int trapinfo[2][2];
static char trapmsg[2][1024];

static void
savetrap(int s)
{
	trapinfo[s][0] = trapped;
	trapinfo[s][1] = trapcode;
	memcpy(trapmsg[s], msgbuf, sizeof msgbuf);
}

/* --- the two sides -------------------------------------------------- */

#define MAXCAP	64

static unsigned char *txmbuf[2];
static unsigned char *memif[2];
static int *vram[2];
static struct vtable memif_vt;

static unsigned char cap[2][MAXCAP][STAMP_SIZE];
static int ncap[2];
static int side;

static void
capture(void *m, void *stamp)
{
	(void)m;
	if (ncap[side] < MAXCAP)
		memcpy(cap[side][ncap[side]], stamp, STAMP_SIZE);
	ncap[side]++;
}

/* --- random --------------------------------------------------------- */

static unsigned int seed = 20260903;

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

static long long
rnd64(void)
{
	return ((long long)rnd() << 40) ^ ((long long)rnd() << 16) ^ rnd();
}

/* An interpolator value: mostly in range, sometimes off both ends. */
static int
rndval(void)
{
	switch (rnd() % 8) {
	case 0: return (int)rnd();
	case 1: return -(int)(rnd() & 0x3ffff);
	case 2: return 0;
	case 3: return rndrange(-0x2000, 0x4000);
	default: return rndrange(0, 0xff0);
	}
}

static long long
rndval64(void)
{
	switch (rnd() % 6) {
	case 0: return rnd64();
	case 1: return -(long long)(rnd() & 0xffffff);
	case 2: return 0;
	default: return (long long)rndrange(0, 0xffffff) << 4;
	}
}

/* --- comparison ----------------------------------------------------- */

static long long ncmp, nfail;

static int
cmptxm(const char *what, int n)
{
	int i;

	if (memcmp(txmbuf[0] + 4, txmbuf[1] + 4, TXM_MEMIF - 4) == 0 &&
	    memcmp(txmbuf[0] + TXM_MEMIF + 4, txmbuf[1] + TXM_MEMIF + 4,
		TXM_CLUT_MEMIF - TXM_MEMIF - 4) == 0 &&
	    memcmp(txmbuf[0] + TXM_CLUT_MEMIF + 4,
		txmbuf[1] + TXM_CLUT_MEMIF + 4,
		TXM_TOTAL - TXM_CLUT_MEMIF - 4) == 0)
		return 0;

	printf("%s iter %d: TXM object differs\n", what, n);
	for (i = 4; i < TXM_TOTAL; i += 4) {
		if (i == TXM_MEMIF || i == TXM_CLUT_MEMIF)
			continue;
		if (memcmp(txmbuf[0] + i, txmbuf[1] + i, 4) != 0)
			printf("    +0x%03x: orig %08x  new %08x\n", i,
				*(unsigned int *)(txmbuf[0] + i),
				*(unsigned int *)(txmbuf[1] + i));
	}
	nfail++;
	return 1;
}

/* PixelStamp fields TXM leaves untouched, so their contents are whatever
 * was on that side's stack.  m_10 and m_44 are never written at all;
 * reg/data only for a register write and pos/pix only for a stamp. */
static int
stampfield(int off, int isreg)
{
	if (off == 0x10 || off == 0x44)
		return 0;
	if (off >= 0x04 && off < 0x10)		/* reg, data */
		return isreg;
	if (off >= 0x14 && off < 0x1c)		/* pos */
		return !isreg;
	if (off >= 0x4c) {			/* pix[16] */
		int k = (off - 0x4c) % 0x38;

		if (!isreg && (k < 0x10 || k == 0x18 ||
		    (k >= 0x1c && k < 0x30)))
			return 1;
		return 0;
	}
	return 1;
}

static int
cmpstamps(const char *what, int n)
{
	int i, k, isreg, bad;

	if (ncap[0] != ncap[1]) {
		printf("%s iter %d: MemIF call count %d vs %d\n", what, n,
			ncap[0], ncap[1]);
		nfail++;
		return 1;
	}
	bad = 0;
	for (i = 0; i < ncap[0] && i < MAXCAP; i++) {
		isreg = *(int *)cap[0][i] != 0;
		for (k = 0; k < STAMP_SIZE; k += 4) {
			if (!stampfield(k, isreg))
				continue;
			if (memcmp(cap[0][i] + k, cap[1][i] + k, 4) == 0)
				continue;
			if (!bad)
				printf("%s iter %d: stamp %d differs\n",
					what, n, i);
			bad = 1;
			printf("    +0x%03x: orig %08x  new %08x\n", k,
				*(unsigned int *)(cap[0][i] + k),
				*(unsigned int *)(cap[1][i] + k));
		}
	}
	if (bad)
		nfail++;
	return bad;
}

static int
cmptrap(const char *what, int n)
{
	if (trapinfo[0][0] == trapinfo[1][0] &&
	    trapinfo[0][1] == trapinfo[1][1] &&
	    strcmp(trapmsg[0], trapmsg[1]) == 0)
		return 0;
	printf("%s iter %d: trap %d/%d \"%s\" vs %d/%d \"%s\"\n", what, n,
		trapinfo[0][0], trapinfo[0][1], trapmsg[0],
		trapinfo[1][0], trapinfo[1][1], trapmsg[1]);
	nfail++;
	return 1;
}

static long long ntrap;

/* --- register writes ------------------------------------------------ */

#define D(off)		(*(int *)(dda + (off)))
#define DL(off)		(*(long long *)(dda + (off)))

static unsigned char dda[DDA_SIZE];

static void
reg(int addr, long long data, int ctxt)
{
	memset(dda, 0, DDA_SIZE);
	D(0x158) = 1;			/* isreg */
	D(0x160) = addr;
	DL(0x164) = data;
	D(0x244) = ctxt;
	D(0x22c) = (int)(rnd() & 1);	/* TME */
	D(0x230) = (int)(rnd() & 1);	/* FGE */
	D(0x234) = (int)(rnd() & 1);	/* ABE */
	D(0x238) = (int)(rnd() & 1);	/* FST */
	D(0x23c) = (int)(rnd() & 1);	/* AA1 */
	D(0x248) = (int)(rnd() & 0x1f);	/* maxexp */
	D(0x174) = (int)rnd();		/* mask */
	D(0x1f8) = (int)rnd();		/* amask */

	ncap[0] = ncap[1] = 0;
	side = 0;
	TRAP(o_put(txmbuf[0], dda));
	savetrap(0);
	side = 1;
	TRAP(n_put(txmbuf[1], dda));
	savetrap(1);
}

/* The legal texture formats; anything else makes GetOneTexel exit. */
static const int psms[] = {
	0x00, 0x01, 0x02, 0x0a, 0x13, 0x14, 0x1b, 0x24, 0x2c,
	0x30, 0x31, 0x32, 0x3a
};
#define NPSM	((int)(sizeof psms / sizeof psms[0]))

/* CSA is capped at 15 by default.  With CSA >= 16 TexClut::load1 runs
 * off the end of clut[] and rewrites the ClutAttr it is reading from
 * (doc/notes/clut.md); phase 4b does that deliberately and then repairs
 * the state, because otherwise every later Lookup indexes with a CSA
 * that is a VRAM word. */
static int csa = 0;

static long long
mktex0(int psm, int tw, int th)
{
	long long d;

	csa = (int)(rnd() & 0xf);

	d  = (long long)(rnd() & 0x3fff);		/* TBP0 */
	d |= (long long)(rnd() % 40 + 1) << 14;		/* TBW */
	d |= (long long)psm << 20;
	d |= (long long)tw << 26;
	d |= (long long)th << 30;
	d |= (long long)(rnd() & 1) << 34;		/* TCC */
	d |= (long long)(rnd() & 3) << 35;		/* TFX */
	d |= (long long)(rnd() & 0x3fff) << 37;		/* CBP */
	d |= (long long)(rnd() & 1 ? 0 : 2) << 51;	/* CPSM */
	d |= (long long)(rnd() % 4 == 0) << 55;		/* CSM */
	d |= (long long)csa << 56;			/* CSA */
	d |= (long long)(rnd() & 7) << 61;		/* CLD */
	return d;
}

static long long
mktex1(int mmin, int mxl)
{
	long long d;

	d  = (long long)(rnd() & 1);			/* LCM */
	d |= (long long)mxl << 2;			/* MXL */
	d |= (long long)(rnd() & 1) << 5;		/* MMAG */
	d |= (long long)mmin << 6;			/* MMIN */
	/* MTBA off: phase 4 turns it on where the size is legal */
	d |= (long long)(rnd() & 3) << 19;		/* L */
	d |= (long long)(rnd() & 0xfff) << 32;		/* K */
	return d;
}

static long long
mkclamp(void)
{
	long long d;

	d  = (long long)(rnd() & 3);			/* WMS */
	d |= (long long)(rnd() & 3) << 2;		/* WMT */
	d |= (long long)(rnd() & 0x3ff) << 4;		/* MINU */
	d |= (long long)(rnd() & 0x3ff) << 14;		/* MAXU */
	d |= (long long)(rnd() & 0x3ff) << 24;		/* MINV */
	d |= (long long)(rnd() & 0x3ff) << 34;		/* MAXV */
	return d;
}

/* Give both sides an identical, usable texture state. */
static void
setupstate(int mmin)
{
	int c;

	for (c = 0; c < 2; c++) {
		reg(0x06 + c, mktex0(psms[rnd() % NPSM],
			rndrange(0, 10), rndrange(0, 10)), c);
		reg(0x14 + c, mktex1(mmin, rndrange(0, 6)), c);
		reg(0x08 + c, mkclamp(), c);
		reg(0x34 + c, rnd64(), c);
		reg(0x36 + c, rnd64(), c);
		reg(0x1c, rnd64(), c);
		reg(0x3b, rnd64(), c);
		reg(0x3d, rnd64(), c);
		reg(0x40 + c, rnd64(), c);
		reg(0x4c + c, rnd64(), c);
	}
}

/* --- main ----------------------------------------------------------- */

int
main(int argc, char **argv)
{
	static unsigned char c[2][16];
	int iter, n, i, k, s;
	int a, b, p[4];
	long long calls;

	iter = argc > 1 ? atoi(argv[1]) : 200000;

	o_valid8_init();
	n_valid8_init();

	memif_vt.delta = 0;
	memif_vt.fn = capture;

	for (s = 0; s < 2; s++) {
		txmbuf[s] = malloc(TXM_TOTAL);
		memif[s] = calloc(1, MEMIF_SIZE);
		vram[s] = mmap(0, (size_t)VRAM_WORDS*4,
			PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
		if (vram[s] == MAP_FAILED)
			vram[s] = 0;
		if (!txmbuf[s] || !memif[s] || !vram[s]) {
			printf("out of memory\n");
			return 1;
		}
		*(void **)(memif[s] + 0x00) = vram[s];
		*(void **)(memif[s] + 0xf8) = &memif_vt;
	}
	/* identical VRAM contents on both sides */
	for (i = 0; i < VRAM_FILL; i++) {
		int v = (int)rnd();

		vram[0][i] = v;
		vram[1][i] = v;
	}

	memset(txmbuf[0], 0x5a, TXM_TOTAL);
	memset(txmbuf[1], 0x5a, TXM_TOTAL);
	o_txm_ctor(txmbuf[0], memif[0]);
	n_txm_ctor(txmbuf[1], memif[1]);
	if (cmptxm("ctor", 0))
		return 1;

	calls = 0;

	/* ---- phase 1: the leaf clamps and filters ---- */
	for (n = 0; n < iter*8; n++) {
		int v = rndval();
		int r0, r1;

		r0 = o_clampq(txmbuf[0], v);
		r1 = n_clampq(txmbuf[1], v);
		if (r0 != r1) {
			printf("ClampQ(%d): %d vs %d\n", v, r0, r1);
			nfail++;
		}
		r0 = o_clampt(txmbuf[0], v);
		r1 = n_clampt(txmbuf[1], v);
		if (r0 != r1) {
			printf("ClampT(%d): %d vs %d\n", v, r0, r1);
			nfail++;
		}
		v = rndrange(-0x200, 0x200);
		r0 = o_clamplod(txmbuf[0], v);
		r1 = n_clamplod(txmbuf[1], v);
		if (r0 != r1) {
			printf("ClampLod(%d): %d vs %d\n", v, r0, r1);
			nfail++;
		}
		a = rndrange(0, 16);
		b = rndrange(0, 16);
		for (i = 0; i < 4; i++)
			p[i] = rndval();
		r0 = o_mfilter1(txmbuf[0], a, p[0], p[1]);
		r1 = n_mfilter1(txmbuf[1], a, p[0], p[1]);
		if (r0 != r1) {
			printf("MFilter1: %d vs %d\n", r0, r1);
			nfail++;
		}
		r0 = o_lfilter1(txmbuf[0], a, b, p[0], p[1], p[2], p[3]);
		r1 = n_lfilter1(txmbuf[1], a, b, p[0], p[1], p[2], p[3]);
		if (r0 != r1) {
			printf("LFilter1: %d vs %d\n", r0, r1);
			nfail++;
		}
		ncmp += 5;
		calls += 5;
	}
	printf("phase 1: %lld leaf calls, %lld failures\n", calls, nfail);

	/* ---- phase 2: GetOneTexel over every PSM ---- */
	for (n = 0; n < iter; n++) {
		int u, v, lod;

		if (n % 16 == 0)
			setupstate((int)(rnd() % 6));
		u = rndrange(-0x40, 0x800);
		v = rndrange(-0x40, 0x800);
		lod = rndrange(0, 6);
		memset(c[0], 0x33, 16);
		memset(c[1], 0x33, 16);
		side = 0;
		TRAP(o_getonetexel(txmbuf[0], u, v, lod, c[0]));
		savetrap(0);
		side = 1;
		TRAP(n_getonetexel(txmbuf[1], u, v, lod, c[1]));
		savetrap(1);
		if (trapinfo[0][0])
			ntrap++;
		cmptrap("getonetexel", n);
		if (memcmp(c[0], c[1], 16) != 0) {
			printf("GetOneTexel(%d,%d,%d): %08x%08x%08x%08x vs "
				"%08x%08x%08x%08x\n", u, v, lod,
				((unsigned *)c[0])[0], ((unsigned *)c[0])[1],
				((unsigned *)c[0])[2], ((unsigned *)c[0])[3],
				((unsigned *)c[1])[0], ((unsigned *)c[1])[1],
				((unsigned *)c[1])[2], ((unsigned *)c[1])[3]);
			nfail++;
		}
		cmptxm("getonetexel", n);
		ncmp++;
		calls++;
	}
	printf("phase 2: %lld calls, %lld failures\n", calls, nfail);

	/* ---- phase 3: register writes ---- */
	for (n = 0; n < iter; n++) {
		static const int regs[] = {
			0x00, 0x06, 0x07, 0x08, 0x09, 0x14, 0x15, 0x16, 0x17,
			0x1b, 0x1c, 0x22, 0x34, 0x35, 0x36, 0x37, 0x3b, 0x3d,
			0x40, 0x41, 0x4c, 0x4d,
			/* and a few TXM forwards untouched */
			0x0a, 0x42, 0x47, 0x50, 0x53
		};
		int r = regs[rnd() % (sizeof regs / sizeof regs[0])];
		long long d;

		switch (r) {
		case 0x06: case 0x07:
			d = mktex0(psms[rnd() % NPSM], rndrange(0, 10),
				rndrange(0, 10));
			break;
		case 0x14: case 0x15:
			d = mktex1((int)(rnd() & 7), rndrange(0, 6));
			break;
		case 0x08: case 0x09:
			d = mkclamp();
			break;
		default:
			d = rnd64();
			break;
		}
		reg(r, d, (int)(rnd() & 1));
		if (trapinfo[0][0])
			ntrap++;
		cmptrap("reg", n);
		cmptxm("reg", n);
		cmpstamps("reg", n);
		ncmp++;
		calls++;
	}
	printf("phase 3: %lld calls, %lld failures\n", calls, nfail);

	/* ---- phase 4: MTBA ---- */
	for (n = 0; n < iter/8; n++) {
		int tw = rndrange(5, 10);
		int ctx = (int)(rnd() & 1);

		reg(0x14 + ctx, mktex1(0, rndrange(0, 6)) | (1LL << 9), ctx);
		reg(0x06 + ctx, mktex0(psms[rnd() % 9], tw, tw), ctx);
		if (trapinfo[0][0])
			ntrap++;
		cmptrap("mtba", n);
		cmptxm("mtba", n);
		cmpstamps("mtba", n);
		ncmp++;
		calls += 2;
	}
	printf("phase 4: %lld calls, %lld failures\n", calls, nfail);

	/* ---- phase 4b: the clut[] overrun ---- */
	for (n = 0; n < iter/8; n++) {
		long long d;
		int ctx = (int)(rnd() & 1);

		d = mktex0(psms[rnd() % NPSM], rndrange(0, 10),
			rndrange(0, 10));
		csa = rndrange(16, 31);
		d = (d & ~(0x1fLL << 56)) | ((long long)csa << 56);
		reg(0x16 + ctx, d, ctx);
		cmptrap("overrun", n);
		cmptxm("overrun", n);
		cmpstamps("overrun", n);
		/* repair: the overrun left every ClutAttr full of texels */
		setupstate((int)(rnd() % 6));
		reg(0x00, 0, (int)(rnd() & 1));
		ncmp++;
		calls++;
	}
	printf("phase 4b: %lld calls, %lld failures\n", calls, nfail);

	/* ---- phase 5: stamps ---- */
	for (n = 0; n < iter; n++) {
		if (n % 8 == 0)
			setupstate((int)(rnd() % 6));

		memset(dda, 0, DDA_SIZE);
		D(0x158) = 0;			/* a stamp */
		D(0x16c) = rndrange(0, 640);	/* px */
		D(0x170) = rndrange(0, 240);	/* py */
		D(0x174) = (int)rnd();		/* mask */
		DL(0x178) = rndval64();		/* z0 */
		D(0x180) = rndval();		/* a0 */
		D(0x184) = rndval();		/* b0 */
		D(0x188) = rndval();		/* g0 */
		D(0x18c) = rndval();		/* r0 */
		D(0x190) = rndval();		/* f0 */
		DL(0x194) = rndval64();		/* z1 */
		D(0x19c) = rndval();		/* a1 */
		D(0x1a0) = rndval();		/* b1 */
		D(0x1a4) = rndval();		/* g1 */
		D(0x1a8) = rndval();		/* r1 */
		D(0x1ac) = rndval();		/* f1 */
		for (k = 0; k < 6; k++)
			D(0x1b0 + k*4) = rndval();	/* s0 t0 q0 s1 t1 q1 */
		for (k = 0; k < 3; k++)
			D(0x1c8 + k*4) = (int)rnd();	/* coverage */
		D(0x1d4) = (int)rnd();
		D(0x1d8) = rndval();
		D(0x1dc) = rndval();
		D(0x1e0) = rndval();
		D(0x1f8) = (int)rnd();		/* amask */
		DL(0x1fc) = rndval64();		/* dzdx */
		for (k = 0; k < 8; k++)
			D(0x204 + k*4) = rndval();	/* dadx..dqdx */
		D(0x224) = rndval();		/* zc */
		D(0x22c) = (int)(rnd() % 3 != 0);	/* TME */
		D(0x230) = (int)(rnd() & 1);	/* FGE */
		D(0x234) = (int)(rnd() & 1);	/* ABE */
		D(0x238) = (int)(rnd() & 1);	/* FST */
		D(0x23c) = (int)(rnd() % 3 == 0);	/* AA1 */
		D(0x244) = (int)(rnd() & 1);	/* CTXT */
		D(0x248) = rndrange(0, 0x1f);	/* maxexp */

		ncap[0] = ncap[1] = 0;
		side = 0;
		TRAP(o_put(txmbuf[0], dda));
		savetrap(0);
		side = 1;
		TRAP(n_put(txmbuf[1], dda));
		savetrap(1);
		if (trapinfo[0][0])
			ntrap++;
		cmptrap("stamp", n);
		cmptxm("stamp", n);
		cmpstamps("stamp", n);
		ncmp++;
		calls++;
		if (nfail > 20)
			break;
	}
	printf("phase 5: %lld calls, %lld failures\n", calls, nfail);

	/* ---- phase 6: the fatal arms, deterministically ---- */
	for (n = 0; n < 64; n++) {
		int what = n % 4;

		switch (what) {
		case 0:		/* GetOneTexel: illegal texture PSM */
			reg(0x06, mktex0(0x03 + (n % 5), 4, 4), 0);
			cmptrap("fatal", n);
			memset(c[0], 0x33, 16);
			memset(c[1], 0x33, 16);
			side = 0;
			TRAP(o_getonetexel(txmbuf[0], 3, 5, 0, c[0]));
			savetrap(0);
			side = 1;
			TRAP(n_getonetexel(txmbuf[1], 3, 5, 0, c[1]));
			savetrap(1);
			break;
		case 1:		/* MipTbpAuto: width != height */
			reg(0x14, mktex1(0, 3) | (1LL << 9), 0);
			reg(0x06, mktex0(0x00, 6, 5), 0);
			break;
		case 2:		/* MipTbpAuto: smaller than 32 */
			reg(0x14, mktex1(0, 3) | (1LL << 9), 0);
			reg(0x06, mktex0(0x00, 3, 3), 0);
			break;
		case 3:		/* MipTbpAuto: PSM without an MTBA rule */
			reg(0x14, mktex1(0, 3) | (1LL << 9), 0);
			reg(0x06, mktex0(0x0a, 6, 6), 0);
			break;
		}
		if (trapinfo[0][0] || trapmsg[0][0])
			ntrap++;
		cmptrap("fatal", n);
		cmptxm("fatal", n);
		ncmp++;
		calls++;
	}
	printf("phase 6: %lld traps compared\n", ntrap);

	printf("%lld calls, %lld comparisons, %lld traps, %lld failures\n",
		calls, ncmp, ntrap, nfail);
	return nfail != 0;
}
