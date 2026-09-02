/* diff_memif - differential test: Sony's memif.o vs src/memif.c.
 *
 * memif.o cannot be tested on its own: MemIF::Stamp reaches VRAM through
 * FBConfig/ZBConfig::ReadStamp/WriteStamp and Memory::SetRegister, all in
 * memory.o.  So *both* objects are renamed apart per side (o_* / n_*, see
 * run_memif.sh) and linked together; each side's memif then resolves to
 * its own memory, and the two halves share only the original addrconv.o
 * and bitblt.o.
 *
 * Each side gets a 16 MB arena whose first 0x4001c8 bytes are a Memory
 * (vram[0x100000], then the FB/ZB configs and the BitBLT), its own 0xfc
 * MemIF and its own 0x3cc PixelStamp.  After every call the MemIF (minus
 * the vptr, which legitimately differs), the stamp and the Memory config
 * block are compared; VRAM every VCHK calls and at the end of each phase.
 *
 * fprintf() and exit() are defined here: the era libc5 _IO_stderr_ does
 * not exist on a modern host, and exit() longjmps back so the fatal arms
 * (illegal ATST, illegal PSM, unknown register) can be compared instead of
 * killing the run.
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

void
DbgWatch__Fiii(int type, int x, int y)
{
}

#define ARENAW	(4*1024*1024)		/* words: 16 MB a side */
#define VRAMW	0x100000		/* the real 4 MB */
#define CFG	0x400000
#define CFGSZ	0x1c8

#define M_FB	0x400000
#define M_ZB	0x40009c
#define C_FBP	0x20
#define C_FBW	0x24
#define C_PSM	0x28
#define C_FBMSK	0x2c
#define C_FBA	0x30
#define C_ZMSK	0x2c
#define C_ZMASK	0x30
#define C_ZTE	0x34

/* MemIF */
#define I_SIZE	0xfc
#define I_CMP	0xf8			/* everything but the vptr */
#define I_MEM	0x00
#define I_CTXT	0x04
#define I_ATEST	0x08
#define I_ATESTC 0x18
#define I_DATEST 0x38
#define I_DATESTC 0x40
#define I_ZTEST	0x50
#define I_ZTESTC 0x58
#define I_BLEND	0x68
#define I_BLENDC 0x80
#define I_DITHER 0xb0
#define I_CLAMP	0xf4

/* PixelStamp */
#define S_SIZE	0x3cc
#define S_TYPE	0x00
#define S_REG	0x04
#define S_DATA	0x08
#define S_X	0x14
#define S_Y	0x18
#define S_MASK	0x1c
#define S_AAMSK	0x28
#define S_ABE	0x38
#define S_CTXT	0x48
#define S_PIX	0x4c
#define S_STRIDE 0x38
#define P_Z	0x18
#define P_PASS	0x30
#define P_AFAIL	0x34

typedef int passfn(void *at, int a);
typedef void st1fn(void *obj, void *st);
typedef void st2fn(void *obj, void *mem, void *st);
typedef int rwfn(void *mif, int i);
typedef void *ctorfn(void *mif, void *mem);
typedef void ctxfn(void *mif, int c);
typedef void set1fn(void *mif, long long d);
typedef void set2fn(void *mif, int ctx, long long d);
typedef void voidfn(void *mif);

extern passfn o_Pass, n_Pass;
extern st1fn o_ATest, n_ATest;
extern st2fn o_DATest, n_DATest;
extern st2fn o_ZTest, n_ZTest;
extern st2fn o_Blend, n_Blend;
extern st1fn o_Dithering, n_Dithering;
extern st1fn o_Clamp, n_Clamp;
extern st1fn o_Stamp, n_Stamp;
extern rwfn o_ReadWord, n_ReadWord;
extern ctorfn o_MemIFctor, n_MemIFctor;
extern ctxfn o_SetContext, n_SetContext;
extern set1fn o_SetPABE, n_SetPABE;
extern set1fn o_SetCOLCLAMP, n_SetCOLCLAMP;
extern set1fn o_SetDTHE, n_SetDTHE;
extern set1fn o_SetDIMX, n_SetDIMX;
extern set2fn o_SetALPHA, n_SetALPHA;
extern set2fn o_SetTEST, n_SetTEST;
extern voidfn o_Context, n_Context;

static char *om, *nm;
static unsigned char omi[I_SIZE], nmi[I_SIZE];
static unsigned char ost[S_SIZE], nst[S_SIZE];

static long calls, mismatch, traps;
static const char *phase = "startup";

static unsigned sd = 20260903;
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
	for (i = 0; i < 256; i++) {
		int a = rnd() % VRAMW;
		F(om, a*4) = F(nm, a*4) = rnd();
	}
}

/* Randomise the whole MemIF but keep ATST in range: an ATST above 7 is
 * the fatal arm, exercised on its own below. */
static void
setmif(void)
{
	int i;

	for (i = 0; i < I_CMP; i += 4)
		F(omi, i) = rnd();
	F(omi, I_MEM) = 0;		/* filled in by the caller */
	F(omi, I_CTXT) = rnd() & 1;
	for (i = 0; i < 3; i++) {
		int a = i == 0 ? I_ATEST : I_ATESTC + (i-1)*0x10;
		F(omi, a + 0x00) = rnd() & 1;		/* ATE */
		F(omi, a + 0x04) = rnd() & 7;		/* ATST */
		F(omi, a + 0x08) = rnd() & 3;		/* AFAIL */
		F(omi, a + 0x0c) = rnd() & 0xff;	/* AREF */
		a = i == 0 ? I_DATEST : I_DATESTC + (i-1)*8;
		F(omi, a + 0x00) = rnd() & 1;		/* DATE */
		F(omi, a + 0x04) = rnd() & 1;		/* DATM */
		a = i == 0 ? I_ZTEST : I_ZTESTC + (i-1)*8;
		F(omi, a + 0x00) = rnd() & 1;		/* ZTE */
		F(omi, a + 0x04) = rnd() & 3;		/* ZTST */
		a = i == 0 ? I_BLEND : I_BLENDC + (i-1)*0x18;
		F(omi, a + 0x00) = rnd() & 1;		/* PABE */
		F(omi, a + 0x04) = rnd() & 3;		/* A */
		F(omi, a + 0x08) = rnd() & 3;		/* B */
		F(omi, a + 0x0c) = rnd() & 3;		/* D */
		F(omi, a + 0x10) = rnd() & 3;		/* C */
		F(omi, a + 0x14) = rnd() & 0xff;	/* FIX */
	}
	F(omi, I_DITHER) = rnd() & 1;
	for (i = 0; i < 16; i++)
		F(omi, I_DITHER + 4 + i*4) = (int)(rnd() & 7) - 4;
	F(omi, I_CLAMP) = rnd() & 1;
	memcpy(nmi, omi, I_CMP);
	F(omi, I_MEM) = (int)(long)om;
	F(nmi, I_MEM) = (int)(long)nm;
}

static void
setstamp(int type)
{
	int i;

	for (i = 0; i < S_SIZE; i += 4)
		F(ost, i) = rnd();
	F(ost, S_TYPE) = type;
	F(ost, S_X) = rnd() % 2048;
	F(ost, S_Y) = rnd() % 1024;
	F(ost, S_MASK) = rnd() & 0xffff;
	F(ost, S_AAMSK) = rnd() & 0xffff;
	F(ost, S_ABE) = rnd() & 1;
	F(ost, S_CTXT) = rnd() & 1;
	for (i = 0; i < 16; i++) {
		int p = S_PIX + i*S_STRIDE;
		F(ost, p + 0) = (int)(rnd() & 0x3ff) - 256;
		F(ost, p + 4) = (int)(rnd() & 0x3ff) - 256;
		F(ost, p + 8) = (int)(rnd() & 0x3ff) - 256;
		F(ost, p + 12) = rnd() & 0xff;
		F(ost, p + P_Z) = rnd();
		F(ost, p + P_PASS) = rnd() & 1;
		F(ost, p + P_AFAIL) = rnd() & 3;
	}
	memcpy(nst, ost, S_SIZE);
}

#define VCHK	512

static int
check(const char *what)
{
	int i;

	calls++;
	if (memcmp(omi + 4, nmi + 4, I_CMP - 4) == 0 &&
	    memcmp(ost, nst, S_SIZE) == 0 &&
	    memcmp(om + CFG, nm + CFG, CFGSZ) == 0 &&
	    (calls % VCHK != 0 || memcmp(om, nm, ARENAW * 4) == 0))
		return 0;
	if (mismatch++ < 3) {
		printf("MISMATCH in %s (call %ld)\n", what, calls);
		for (i = 4; i < I_CMP; i += 4)
			if (F(omi, i) != F(nmi, i))
				printf("  memif +%03x: %08x != %08x\n", i,
				    F(omi, i), F(nmi, i));
		for (i = 0; i < S_SIZE; i += 4)
			if (F(ost, i) != F(nst, i))
				printf("  stamp +%03x: %08x != %08x\n", i,
				    F(ost, i), F(nst, i));
		for (i = 0; i < CFGSZ; i += 4)
			if (F(om, CFG + i) != F(nm, CFG + i))
				printf("  cfg +%06x: %08x != %08x\n",
				    CFG + i, F(om, CFG + i), F(nm, CFG + i));
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

/* MemIF::Stamp's own registers plus a few it only forwards. */
static const int regs[] = {0x00, 0x1b, 0x42, 0x43, 0x44, 0x45, 0x46,
    0x47, 0x48, 0x49, 0x3f, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
    0x51, 0x52, 0x53, 0x54, 0x7f};

/* Run both sides of a call that may exit(); returns 1 on divergence. */
#define BOTH(ocall, ncall, what)					\
	do {								\
		int oe, ne;						\
		exited = 0;						\
		if (setjmp(jb) == 0) { ocall; }				\
		oe = exited;						\
		exited = 0;						\
		if (setjmp(jb) == 0) { ncall; }				\
		ne = exited;						\
		if (oe != ne) {						\
			printf("MISMATCH exit in %s\n", what);		\
			return 1;					\
		}							\
		traps += oe;						\
	} while (0)

int
main(void)
{
	int i, n, r1, r2;
	long long d;

	om = (char *)malloc(ARENAW * 4);
	nm = (char *)malloc(ARENAW * 4);
	if (om == 0 || nm == 0) {
		printf("out of memory\n");
		return 1;
	}
	memset(om, 0, ARENAW * 4);
	memset(nm, 0, ARENAW * 4);

	if (setjmp(jb) != 0) {
		printf("UNEXPECTED exit() in %s (call %ld)\n", phase, calls);
		fflush(stdout);
		_exit(1);
	}

	/* 1. the constructor and ReadWord */
	phase = "ctor";
	for (n = 0; n < 100000; n++) {
		setcfg();
		memset(omi, 0, I_SIZE);
		memset(nmi, 0, I_SIZE);
		o_MemIFctor(omi, om);
		n_MemIFctor(nmi, nm);
		if (F(omi, I_MEM) != (int)(long)om ||
		    F(nmi, I_MEM) != (int)(long)nm) {
			printf("MISMATCH ctor did not store mem\n");
			return 1;
		}
		if (check("ctor"))
			return 1;
		i = rnd() % VRAMW;
		r1 = o_ReadWord(omi, i);
		r2 = n_ReadWord(nmi, i);
		if (r1 != r2) {
			printf("MISMATCH ReadWord(%d): %08x != %08x\n",
			    i, r1, r2);
			return 1;
		}
		if (check("ReadWord"))
			return 1;
	}
	if (endphase("ctor"))
		return 1;

	/* 2. AlphaTest::Pass, all eight functions plus the fatal arm */
	phase = "Pass";
	for (n = 0; n < 400000; n++) {
		setmif();
		if ((rnd() & 63) == 0)
			F(omi, I_ATEST + 4) = F(nmi, I_ATEST + 4) =
			    8 + rnd() % 24;
		i = rnd() & 0xff;
		r1 = r2 = 0;
		BOTH(r1 = o_Pass(omi + I_ATEST, i),
		     r2 = n_Pass(nmi + I_ATEST, i), "Pass");
		if (r1 != r2) {
			printf("MISMATCH Pass(%d) ATST=%d: %d != %d\n", i,
			    F(omi, I_ATEST + 4), r1, r2);
			return 1;
		}
		if (check("Pass"))
			return 1;
	}
	if (endphase("Pass"))
		return 1;

	/* 3. the six per-pixel units on their own */
	phase = "units";
	for (n = 0; n < 200000; n++) {
		setcfg();
		setmif();
		setstamp(0);
		o_ATest(omi + I_ATEST, ost);
		n_ATest(nmi + I_ATEST, nst);
		if (check("ATest"))
			return 1;
		o_DATest(omi + I_DATEST, om, ost);
		n_DATest(nmi + I_DATEST, nm, nst);
		if (check("DATest"))
			return 1;
		o_ZTest(omi + I_ZTEST, om, ost);
		n_ZTest(nmi + I_ZTEST, nm, nst);
		if (check("ZTest"))
			return 1;
		o_Blend(omi + I_BLEND, om, ost);
		n_Blend(nmi + I_BLEND, nm, nst);
		if (check("Blend"))
			return 1;
		o_Dithering(omi + I_DITHER, ost);
		n_Dithering(nmi + I_DITHER, nst);
		if (check("Dithering"))
			return 1;
		o_Clamp(omi + I_CLAMP, ost);
		n_Clamp(nmi + I_CLAMP, nst);
		if (check("Clamp"))
			return 1;
	}
	if (endphase("units"))
		return 1;

	/* 4. the register setters and Context */
	phase = "setters";
	for (n = 0; n < 200000; n++) {
		setmif();
		d = ((long long)rnd() << 32) | rnd();
		o_SetContext(omi, rnd() & 1);
		n_SetContext(nmi, sd & 1);
		if (check("SetContext"))
			return 1;
		o_SetPABE(omi, d);
		n_SetPABE(nmi, d);
		if (check("SetPABE"))
			return 1;
		o_SetCOLCLAMP(omi, d);
		n_SetCOLCLAMP(nmi, d);
		if (check("SetCOLCLAMP"))
			return 1;
		o_SetDTHE(omi, d);
		n_SetDTHE(nmi, d);
		if (check("SetDTHE"))
			return 1;
		o_SetDIMX(omi, d);
		n_SetDIMX(nmi, d);
		if (check("SetDIMX"))
			return 1;
		i = rnd() & 1;
		o_SetALPHA(omi, i, d);
		n_SetALPHA(nmi, i, d);
		if (check("SetALPHA"))
			return 1;
		o_SetTEST(omi, i, d);
		n_SetTEST(nmi, i, d);
		if (check("SetTEST"))
			return 1;
		o_Context(omi);
		n_Context(nmi);
		if (check("Context"))
			return 1;
	}
	if (endphase("setters"))
		return 1;

	/* 5. MemIF::Stamp: the pixel path */
	phase = "Stamp(pixels)";
	for (n = 0; n < 300000; n++) {
		setcfg();
		setmif();
		setstamp(0);
		BOTH(o_Stamp(omi, ost), n_Stamp(nmi, nst), "Stamp");
		if (check("Stamp"))
			return 1;
	}
	if (endphase("Stamp(pixels)"))
		return 1;

	/* 6. MemIF::Stamp: register writes, in runs on one object */
	phase = "Stamp(regs)";
	for (n = 0; n < 200000; n++) {
		setcfg();
		setmif();
		for (i = 0; i < 8; i++) {
			setstamp(1 + (rnd() & 3));
			F(ost, S_REG) = F(nst, S_REG) = regs[rnd() % 23];
			if (F(ost, S_REG) == 0x52) {
				F(ost, S_DATA) = F(nst, S_DATA) = rnd() % 8;
				F(ost, S_DATA+4) = F(nst, S_DATA+4) =
				    rnd() % 8;
			}
			BOTH(o_Stamp(omi, ost), n_Stamp(nmi, nst), "Stamp");
			if (check("Stamp(reg)"))
				return 1;
		}
	}
	if (endphase("Stamp(regs)"))
		return 1;

	/* 7. unknown registers: Memory::SetRegister's fatal arm, reached
	 *    through MemIF::Stamp's default case */
	phase = "Stamp(unknown)";
	for (n = 0; n < 2000; n++) {
		setcfg();
		setmif();
		setstamp(1);
		F(ost, S_REG) = F(nst, S_REG) = 0x55 + rnd() % 0x2a;
		BOTH(o_Stamp(omi, ost), n_Stamp(nmi, nst), "Stamp");
		if (check("Stamp(unknown)"))
			return 1;
	}
	if (endphase("Stamp(unknown)"))
		return 1;

	printf("\n%ld calls, %ld error prints, %ld fatal traps, "
	    "%ld mismatches\n", calls, nerr, traps, mismatch);
	fflush(stdout);
	_exit(mismatch != 0);
	return 0;
}
