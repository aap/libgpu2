/* diff_bitblt - differential test: Sony's bitblt.o vs src/bitblt.c.
 *
 * Both objects have their five mangled BitBLT entry points renamed apart
 * with objcopy (o_* / n_*, see run_bitblt.sh) and are linked into one i386
 * binary together with the *original* addrconv.o (byte-identical to ours).
 *
 * A BitBLT is 0x84 bytes and derives from AddrConv, so offsets 0x00..0x1c
 * are the address block and 0x20.. the transfer registers.  Each side gets
 * its own 16 MB fake local memory (four times the real 4 MB, so an
 * out-of-range BP still lands in mapped memory instead of faulting) and its
 * own object; after every call both the object and the whole of VRAM are
 * compared.
 *
 * fprintf() is defined here so the error paths can be exercised without a
 * real FILE (the era libc5 _IO_stderr_ object does not exist on a modern
 * host); it just counts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char _IO_stderr_[256];

static long nerr;
int
fprintf(FILE *f, const char *fmt, ...)
{
	nerr++;
	return 0;
}

#define OBJSZ	0x84
#define VRAMW	(4*1024*1024)		/* words */

/* BitBLT field offsets */
#define O_SBP	0x20
#define O_SBW	0x24
#define O_DBP	0x28
#define O_DBW	0x2c
#define O_SPSM	0x30
#define O_DPSM	0x34
#define O_SSAX	0x38
#define O_SSAY	0x3c
#define O_DSAX	0x40
#define O_DSAY	0x44
#define O_DIR	0x58
#define O_RRW	0x5c
#define O_RRH	0x60
#define O_TRXDIR 0x6c
#define O_COUNT	0x70
#define O_X	0x74
#define O_PHASE	0x78
#define O_SAVE	0x7c

typedef unsigned readfn(void *thisp, void *mem, int x, int y);
typedef void writefn(void *thisp, void *mem, unsigned data, int x, int y);
typedef void blitfn(void *thisp, void *mem);
typedef void wpfn(void *thisp, void *mem, long long data);
typedef long long rpfn(void *thisp, void *mem);

extern readfn o_read, n_read;
extern writefn o_write, n_write;
extern blitfn o_DoBitBLT, n_DoBitBLT;
extern wpfn o_WritePixel, n_WritePixel;
extern rpfn o_ReadPixel, n_ReadPixel;

static int *ovram, *nvram;
static unsigned char oo[OBJSZ], nn[OBJSZ];

static long calls, mismatch;

static unsigned s = 20260902;
static unsigned
rnd(void)
{
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	return s;
}

static const int psms[] = {0x00, 0x01, 0x02, 0x0a, 0x13, 0x14, 0x1b,
    0x24, 0x2c, 0x30, 0x31, 0x32, 0x3a};

#define F(o,f)	(*(int *)((o) + (f)))

static void
setup(int trxdir)
{
	int i, sp, dp;

	memset(oo, 0, OBJSZ);
	sp = psms[rnd() % 13];
	dp = psms[rnd() % 13];
	F(oo, O_SBP) = (rnd() % 0x4000) * 64;
	F(oo, O_SBW) = (rnd() % 64) * 64;
	F(oo, O_DBP) = (rnd() % 0x4000) * 64;
	F(oo, O_DBW) = (rnd() % 64) * 64;
	F(oo, O_SPSM) = sp;
	F(oo, O_DPSM) = dp;
	F(oo, O_SSAX) = rnd() % 2048;
	F(oo, O_SSAY) = rnd() % 2048;
	F(oo, O_DSAX) = rnd() % 2048;
	F(oo, O_DSAY) = rnd() % 2048;
	F(oo, O_DIR) = rnd() % 4;
	F(oo, O_RRW) = rnd() % 9;
	F(oo, O_RRH) = rnd() % 9;
	F(oo, O_TRXDIR) = trxdir;
	F(oo, O_COUNT) = rnd() % 5;
	F(oo, O_X) = rnd() % 2048;
	F(oo, O_PHASE) = rnd() % 6;
	F(oo, O_SAVE) = rnd() & 0xffff;
	memcpy(nn, oo, OBJSZ);
	for (i = 0; i < 4096; i++) {
		int a = rnd() % VRAMW;
		ovram[a] = nvram[a] = rnd();
	}
}

/* VRAM is 16 MB a side, so comparing all of it on every call would take
 * hours; the object is compared every call and VRAM every VCHK calls and at
 * the end of every phase.  A VRAM divergence is permanent, so it is still
 * caught - just a little later. */
#define VCHK	256

static int
check(const char *what)
{
	int i;

	calls++;
	if (memcmp(oo, nn, OBJSZ) == 0 &&
	    (calls % VCHK != 0 ||
	     memcmp(ovram, nvram, VRAMW * sizeof(int)) == 0))
		return 0;
	if (mismatch++ < 3) {
		printf("MISMATCH in %s\n", what);
		for (i = 0; i < OBJSZ; i += 4)
			if (F(oo, i) != F(nn, i))
				printf("  obj +%02x: %08x != %08x\n", i,
				    F(oo, i), F(nn, i));
		for (i = 0; i < VRAMW; i++)
			if (ovram[i] != nvram[i]) {
				printf("  vram[%x]: %08x != %08x\n", i,
				    ovram[i], nvram[i]);
				break;
			}
	}
	return 1;
}

int
main(void)
{
	int i, x, y;
	unsigned r1, r2, d;
	long long q1, q2;
	long n;

	ovram = (int *)malloc(VRAMW * sizeof(int));
	nvram = (int *)malloc(VRAMW * sizeof(int));
	if (ovram == 0 || nvram == 0) {
		printf("out of memory\n");
		return 1;
	}
	for (i = 0; i < VRAMW; i++)
		ovram[i] = nvram[i] = 0;

	/* 1. read - every source PSM, random positions */
	for (n = 0; n < 400000; n++) {
		setup(1);
		x = rnd() % 4096;
		y = rnd() % 4096;
		r1 = o_read(oo, ovram, x, y);
		r2 = n_read(nn, nvram, x, y);
		if (r1 != r2) {
			if (mismatch++ < 3)
				printf("MISMATCH read(%d,%d) SPSM=%02x: "
				    "%08x != %08x\n", x, y,
				    F(oo, O_SPSM), r1, r2);
			return 1;
		}
		if (check("read"))
			return 1;
	}
	if (memcmp(ovram, nvram, VRAMW * sizeof(int)) != 0) {
		printf("VRAM MISMATCH after read\n");
		return 1;
	}
	printf("read: %ld calls ok\n", calls);

	/* 2. write - every destination PSM, merged into live neighbours */
	for (n = 0; n < 400000; n++) {
		setup(0);
		x = rnd() % 4096;
		y = rnd() % 4096;
		d = rnd();
		o_write(oo, ovram, d, x, y);
		n_write(nn, nvram, d, x, y);
		if (check("write"))
			return 1;
	}
	if (memcmp(ovram, nvram, VRAMW * sizeof(int)) != 0) {
		printf("VRAM MISMATCH after write\n");
		return 1;
	}
	printf("write: %ld calls ok\n", calls);

	/* 3. DoBitBLT - all four directions */
	for (n = 0; n < 60000; n++) {
		setup(2);
		o_DoBitBLT(oo, ovram);
		n_DoBitBLT(nn, nvram);
		if (check("DoBitBLT"))
			return 1;
	}
	if (memcmp(ovram, nvram, VRAMW * sizeof(int)) != 0) {
		printf("VRAM MISMATCH after DoBitBLT\n");
		return 1;
	}
	printf("DoBitBLT: %ld calls ok\n", calls);

	/* 4. WritePixel - host to local, 64 bits at a time.  Runs of eight
	 *    calls on one object so the packing state carries over. */
	for (n = 0; n < 60000; n++) {
		setup(0);
		for (i = 0; i < 8; i++) {
			long long v = ((long long)rnd() << 32) | rnd();
			o_WritePixel(oo, ovram, v);
			n_WritePixel(nn, nvram, v);
			if (check("WritePixel"))
				return 1;
			F(oo, O_PHASE)++;
			F(nn, O_PHASE)++;
		}
	}
	if (memcmp(ovram, nvram, VRAMW * sizeof(int)) != 0) {
		printf("VRAM MISMATCH after WritePixel\n");
		return 1;
	}
	printf("WritePixel: %ld calls ok\n", calls);

	/* 5. ReadPixel - local to host; TRXDIR 1 works, 3 returns early */
	for (n = 0; n < 60000; n++) {
		setup((rnd() & 7) ? 1 : 3);
		for (i = 0; i < 8; i++) {
			q1 = o_ReadPixel(oo, ovram);
			q2 = n_ReadPixel(nn, nvram);
			if (q1 != q2) {
				if (mismatch++ < 3)
					printf("MISMATCH ReadPixel SPSM=%02x: "
					    "%08x%08x != %08x%08x\n",
					    F(oo, O_SPSM),
					    (unsigned)(q1>>32), (unsigned)q1,
					    (unsigned)(q2>>32), (unsigned)q2);
				return 1;
			}
			if (check("ReadPixel"))
				return 1;
		}
	}
	if (memcmp(ovram, nvram, VRAMW * sizeof(int)) != 0) {
		printf("VRAM MISMATCH after ReadPixel\n");
		return 1;
	}
	printf("%ld calls, %ld mismatches, %ld error messages\n",
	    calls, mismatch, nerr);
	return mismatch != 0;
}
