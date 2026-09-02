/* diff_memory - differential test: Sony's memory.o vs src/memory.c.
 *
 * Both objects have their nine mangled entry points renamed apart with
 * objcopy (o_* / n_*, see run_memory.sh) and are linked into one i386
 * binary together with the *original* addrconv.o and bitblt.o, which both
 * halves share (addrconv is byte-identical to ours; bitblt is the engine
 * Memory::SetRegister drives, not the code under test).
 *
 * Each side gets its own 16 MB fake local memory - four times the real
 * 4 MB, so a legal-but-extreme FBP still lands in mapped memory instead of
 * faulting.  A Memory is 0x4001c8 bytes: `vram[0x100000]' at offset 0,
 * then three FBConfigs, three ZBConfigs and the BitBLT, so the arena's
 * first 0x4001c8 bytes ARE the object.  After every call the 0x1c8-byte
 * config block is compared, VRAM every VCHK calls, and the whole 16 MB at
 * the end of each phase.
 *
 * fprintf() and exit() are defined here so the error paths can be
 * exercised without a real FILE (the era libc5 _IO_stderr_ does not exist
 * on a modern host) and without dying: exit() longjmps back to the caller,
 * which then checks that both sides took it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>

char _IO_stderr_[256];

static long nerr;
static jmp_buf jb;
static int exited;

int
fprintf(FILE *f, const char *fmt, ...)
{
	nerr++;
	return 0;
}

void
exit(int code)
{
	exited = 1;
	longjmp(jb, 1);
}

/* memory.o's only other import */
void
DbgWatch__Fiii(int type, int x, int y)
{
}

#define ARENAW	(4*1024*1024)		/* words: 16 MB a side */
#define VRAMW	0x100000		/* the real 4 MB of local memory */
#define MEMSZ	0x4001c8
#define CFG	0x400000		/* first config byte */
#define CFGSZ	0x1c8

/* Memory */
#define M_FB	0x400000
#define M_FB1	0x400034
#define M_FB2	0x400068
#define M_ZB	0x40009c
#define M_ZB1	0x4000d4
#define M_ZB2	0x40010c
#define M_BLT	0x400144
#define M_CTXT	0x4001c4

/* FBConfig / ZBConfig */
#define C_FBP	0x20
#define C_FBW	0x24
#define C_PSM	0x28
#define C_FBMSK	0x2c
#define C_FBA	0x30
#define C_ZMSK	0x2c
#define C_ZMASK	0x30
#define C_ZTE	0x34

/* PixelStamp */
#define S_SIZE	0x3cc
#define S_X	0x14
#define S_Y	0x18
#define S_MASK	0x1c
#define S_AAMSK	0x28
#define S_PIX	0x4c
#define S_STRIDE 0x38
#define P_R	0x00
#define P_Z	0x18
#define P_PASS	0x30
#define P_AFAIL	0x34

typedef void rpfn(void *ret, void *cfg, void *mem, int x, int y);
typedef void wpfn(void *cfg, void *mem, int x, int y,
	int r, int g, int b, int a, int aw, int fbp, int fbw);
typedef void stampfn(void *cfg, void *mem, void *st);
typedef unsigned rzfn(void *cfg, void *mem, int x, int y);
typedef void wzfn(void *cfg, void *mem, int x, int y, unsigned z);
typedef void regfn(void *mem, int addr, long long data);

extern rpfn o_FBReadPixel, n_FBReadPixel;
extern wpfn o_FBWritePixel, n_FBWritePixel;
extern stampfn o_FBReadStamp, n_FBReadStamp;
extern stampfn o_FBWriteStamp, n_FBWriteStamp;
extern rzfn o_ZBReadZ, n_ZBReadZ;
extern wzfn o_ZBWriteZ, n_ZBWriteZ;
extern stampfn o_ZBReadStamp, n_ZBReadStamp;
extern stampfn o_ZBWriteStamp, n_ZBWriteStamp;
extern regfn o_SetRegister, n_SetRegister;

static char *om, *nm;			/* the two arenas */
static unsigned char ost[S_SIZE], nst[S_SIZE];

static long calls, mismatch, traps;
static const char *phase = "startup";

static unsigned sd = 20260902;
static unsigned
rnd(void)
{
	sd ^= sd << 13; sd ^= sd >> 17; sd ^= sd << 5;
	return sd;
}

static const int fbpsms[] = {0x00, 0x01, 0x02, 0x0a, 0x13, 0x14, 0x1b,
    0x24, 0x2c, 0x30, 0x31, 0x32, 0x3a};
static const int zbpsms[] = {0x30, 0x31, 0x32, 0x3a};

#define F(o,f)	(*(int *)((char *)(o) + (f)))

/* Fill both arenas' config blocks with the same random-but-legal state. */
static void
setcfg(void)
{
	int i;

	for (i = 0; i < CFGSZ; i += 4)
		F(om, CFG + i) = 0;
	F(om, M_FB + C_FBP) = (rnd() % 0x200) * 2048;
	F(om, M_FB + C_FBW) = (rnd() % 64) * 64;
	F(om, M_FB + C_PSM) = fbpsms[rnd() % 13];
	F(om, M_FB + C_FBMSK) = rnd();
	F(om, M_FB + C_FBA) = rnd() & 1;
	F(om, M_ZB + C_FBP) = (rnd() % 0x200) * 2048;
	F(om, M_ZB + C_FBW) = (rnd() % 64) * 64;
	F(om, M_ZB + C_PSM) = zbpsms[rnd() % 4];
	F(om, M_ZB + C_ZMSK) = rnd() & 1;
	F(om, M_ZB + C_ZMASK) = rnd();
	F(om, M_ZB + C_ZTE) = rnd() & 1;
	memcpy(nm + CFG, om + CFG, CFGSZ);
	for (i = 0; i < 512; i++) {
		int a = rnd() % VRAMW;
		F(om, a*4) = F(nm, a*4) = rnd();
	}
}

static void
setstamp(void)
{
	int i;

	for (i = 0; i < S_SIZE; i += 4)
		F(ost, i) = rnd();
	F(ost, S_X) = rnd() % 2048;
	F(ost, S_Y) = rnd() % 1024;
	F(ost, S_MASK) = rnd() & 0xffff;
	F(ost, S_AAMSK) = rnd() & 0xffff;
	for (i = 0; i < 16; i++) {
		int p = S_PIX + i*S_STRIDE;
		F(ost, p + P_R + 0) = rnd() & 0x1ff;
		F(ost, p + P_R + 4) = rnd() & 0x1ff;
		F(ost, p + P_R + 8) = rnd() & 0x1ff;
		F(ost, p + P_R + 12) = rnd() & 0xff;
		F(ost, p + P_Z) = rnd();
		F(ost, p + P_PASS) = rnd() & 1;
		F(ost, p + P_AFAIL) = rnd() & 3;
	}
	memcpy(nst, ost, S_SIZE);
}

/* Comparing 16 MB on every call would take hours; the config block is
 * checked every call, VRAM every VCHK, and everything at phase end. */
#define VCHK	512

static int
check(const char *what)
{
	int i;

	calls++;
	if (memcmp(om + CFG, nm + CFG, CFGSZ) == 0 &&
	    memcmp(ost, nst, S_SIZE) == 0 &&
	    (calls % VCHK != 0 ||
	     memcmp(om, nm, ARENAW * 4) == 0))
		return 0;
	if (mismatch++ < 3) {
		printf("MISMATCH in %s (call %ld)\n", what, calls);
		for (i = 0; i < CFGSZ; i += 4)
			if (F(om, CFG + i) != F(nm, CFG + i))
				printf("  cfg +%06x: %08x != %08x\n",
				    CFG + i, F(om, CFG + i), F(nm, CFG + i));
		for (i = 0; i < S_SIZE; i += 4)
			if (F(ost, i) != F(nst, i))
				printf("  stamp +%03x: %08x != %08x\n", i,
				    F(ost, i), F(nst, i));
		for (i = 0; i < ARENAW; i++)
			if (F(om, i*4) != F(nm, i*4)) {
				printf("  vram[%x]: %08x != %08x\n", i,
				    F(om, i*4), F(nm, i*4));
				break;
			}
	}
	return 1;
}

static int
endphase(const char *what)
{
	if (memcmp(om, nm, ARENAW * 4) != 0) {
		printf("VRAM MISMATCH after %s\n", what);
		return 1;
	}
	printf("%-12s %9ld calls ok\n", what, calls);
	return 0;
}

/* Registers Memory::SetRegister handles; anything else is the fatal
 * default arm, exercised separately below. */
static const int regs[] = {0x00, 0x1b, 0x3f, 0x47, 0x48, 0x4a, 0x4b,
    0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x7f};

int
main(void)
{
	int i, x, y, n, aw;
	unsigned z1, z2;
	int oc[4], nc[4];
	long long d;
	int oe, ne;

	om = (char *)malloc(ARENAW * 4);
	nm = (char *)malloc(ARENAW * 4);
	if (om == 0 || nm == 0) {
		printf("out of memory\n");
		return 1;
	}
	memset(om, 0, ARENAW * 4);
	memset(nm, 0, ARENAW * 4);

	/* Any exit() outside phase 7 is a bug in the test setup, not a
	   difference; catch it rather than longjmping into hyperspace. */
	if (setjmp(jb) != 0) {
		printf("UNEXPECTED exit() in %s (call %ld)\n", phase, calls);
		fflush(stdout);
		_exit(1);
	}

	/* 1. FBConfig::ReadPixel - every frame buffer PSM */
	phase = "ReadPixel";
	for (n = 0; n < 400000; n++) {
		setcfg();
		x = rnd() % 2048;
		y = rnd() % 2048;
		o_FBReadPixel(oc, om + M_FB, om, x, y);
		n_FBReadPixel(nc, nm + M_FB, nm, x, y);
		if (memcmp(oc, nc, sizeof oc) != 0) {
			if (mismatch++ < 3)
				printf("MISMATCH ReadPixel(%d,%d) PSM=%02x: "
				    "%d,%d,%d,%d != %d,%d,%d,%d\n", x, y,
				    F(om, M_FB + C_PSM),
				    oc[0], oc[1], oc[2], oc[3],
				    nc[0], nc[1], nc[2], nc[3]);
			return 1;
		}
		if (check("ReadPixel"))
			return 1;
	}
	if (endphase("ReadPixel"))
		return 1;

	/* 2. FBConfig::WritePixel - both alpha-write arms, all PSMs */
	phase = "WritePixel";
	for (n = 0; n < 400000; n++) {
		setcfg();
		x = rnd() % 2048;
		y = rnd() % 2048;
		for (i = 0; i < 4; i++)
			oc[i] = rnd() & 0xff;
		aw = rnd() & 1;
		o_FBWritePixel(om + M_FB, om, x, y, oc[0], oc[1], oc[2],
		    oc[3], aw, F(om, M_FB + C_FBP), F(om, M_FB + C_FBW));
		n_FBWritePixel(nm + M_FB, nm, x, y, oc[0], oc[1], oc[2],
		    oc[3], aw, F(nm, M_FB + C_FBP), F(nm, M_FB + C_FBW));
		if (check("WritePixel"))
			return 1;
	}
	if (endphase("WritePixel"))
		return 1;

	/* 3. FBConfig::ReadStamp / WriteStamp */
	phase = "FBStamp";
	for (n = 0; n < 200000; n++) {
		setcfg();
		setstamp();
		o_FBReadStamp(om + M_FB, om, ost);
		n_FBReadStamp(nm + M_FB, nm, nst);
		if (check("FBReadStamp"))
			return 1;
		o_FBWriteStamp(om + M_FB, om, ost);
		n_FBWriteStamp(nm + M_FB, nm, nst);
		if (check("FBWriteStamp"))
			return 1;
	}
	if (endphase("FBStamp"))
		return 1;

	/* 4. ZBConfig::ReadZ / WriteZ */
	phase = "ReadZ/WriteZ";
	for (n = 0; n < 400000; n++) {
		setcfg();
		x = rnd() % 2048;
		y = rnd() % 2048;
		z1 = o_ZBReadZ(om + M_ZB, om, x, y);
		z2 = n_ZBReadZ(nm + M_ZB, nm, x, y);
		if (z1 != z2) {
			if (mismatch++ < 3)
				printf("MISMATCH ReadZ(%d,%d) PSM=%02x: "
				    "%08x != %08x\n", x, y,
				    F(om, M_ZB + C_PSM), z1, z2);
			return 1;
		}
		if (check("ReadZ"))
			return 1;
		z1 = rnd();
		o_ZBWriteZ(om + M_ZB, om, x, y, z1);
		n_ZBWriteZ(nm + M_ZB, nm, x, y, z1);
		if (check("WriteZ"))
			return 1;
	}
	if (endphase("ReadZ/WriteZ"))
		return 1;

	/* 5. ZBConfig::ReadStamp / WriteStamp */
	phase = "ZBStamp";
	for (n = 0; n < 200000; n++) {
		setcfg();
		setstamp();
		o_ZBReadStamp(om + M_ZB, om, ost);
		n_ZBReadStamp(nm + M_ZB, nm, nst);
		if (check("ZBReadStamp"))
			return 1;
		o_ZBWriteStamp(om + M_ZB, om, ost);
		n_ZBWriteStamp(nm + M_ZB, nm, nst);
		if (check("ZBWriteStamp"))
			return 1;
	}
	if (endphase("ZBStamp"))
		return 1;

	/* 6. Memory::SetRegister - every handled register, in runs so the
	 *    transfer state machine (BITBLTBUF/TRXPOS/TRXREG/TRXDIR/HWREG)
	 *    actually runs transfers.  TRXREG is kept small so the
	 *    local-to-local blit does not take all day. */
	phase = "SetRegister";
	for (n = 0; n < 200000; n++) {
		setcfg();
		for (i = 0; i < 12; i++) {
			int r = regs[rnd() % 17];
			d = ((long long)rnd() << 32) | rnd();
			if (r == 0x52)		/* TRXREG: keep it small */
				d = (long long)(rnd() % 8) |
				    ((long long)(rnd() % 8) << 32);
			/* a local-to-local blit with a nonsense PSM makes
			   address_convert print and exit(1); both sides must
			   do it at the same point. */
			exited = 0;
			if (setjmp(jb) == 0)
				o_SetRegister(om, r, d);
			oe = exited;
			exited = 0;
			if (setjmp(jb) == 0)
				n_SetRegister(nm, r, d);
			ne = exited;
			if (oe != ne) {
				printf("MISMATCH exit reg %02x: %d != %d\n",
				    r, oe, ne);
				return 1;
			}
			traps += oe;
			if (check("SetRegister"))
				return 1;
		}
	}
	if (endphase("SetRegister"))
		return 1;

	/* 7. the fatal default arm: fprintf + exit(0) */
	phase = "default";
	for (n = 0; n < 2000; n++) {
		int r = 0x55 + rnd() % 0x2a;

		setcfg();
		d = ((long long)rnd() << 32) | rnd();
		exited = 0;
		if (setjmp(jb) == 0)
			o_SetRegister(om, r, d);
		oe = exited;
		exited = 0;
		if (setjmp(jb) == 0)
			n_SetRegister(nm, r, d);
		ne = exited;
		if (oe != ne || oe == 0) {
			printf("MISMATCH default arm reg %02x: %d != %d\n",
			    r, oe, ne);
			return 1;
		}
		traps++;
		if (check("default"))
			return 1;
	}
	if (endphase("default"))
		return 1;

	printf("\n%ld calls, %ld error prints, %ld fatal traps, "
	    "%ld mismatches\n", calls, nerr, traps, mismatch);
	fflush(stdout);
	_exit(mismatch != 0);
	return 0;
}
