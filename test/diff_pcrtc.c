/* Differential test: reconstructed pcrtc.o against the 1998 object.
 *
 * Both PCRTCxif's are driven with the same randomised privileged-register
 * stream through PCRTCxif::SetRegister - PMODE, SMODE1/2, SYNCH1, SYNCV,
 * DISPFB1/2, DISPLAY1/2, BGCOLOR, EXTBUF, EXTDATA, EXTWRITE and the two
 * pseudo-registers 0x100 (Display) and 0x101 (DisplayPcrtc) - over a
 * private 16 MB local memory each, seeded identically.
 *
 * The X11 side is a fake Xifbase: a hand-built g++ 2.7 vtable (8 zero bytes
 * then nine {short delta; short pad; fn} entries) whose nine entries hash
 * every call and its arguments.  Every pixel the real XWindow would have
 * been handed therefore reaches the comparison, which is the only way to
 * test the display merge at all - the VRAM md5 of a replay never sees it.
 *
 * After every register write the whole 0x1d4-byte PCRTCxif is compared
 * (the ten pointer/vtable slots are normalised to object-relative form
 * first), and after every display the call hash, the call count and both
 * sides' local memory.
 *
 * Not covered here: the real XWindow path (needs an X server; byte-match
 * rigour covers it) and PCRTCxif's second constructor, which calls
 * XWindow::OpenWindow.
 *
 * Built by test/run_pcrtc.sh, which renames the 1998 symbols to o_* and the
 * reconstructed ones to n_* first.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef long long ll;
typedef unsigned long long u64;
typedef unsigned int u32;

#define PCRTC_SIZE	0x1d4
#define VMWORDS		(4*1024*1024)	/* 16 MB of slack per side */

/* Offsets inside PCRTCxif that hold a pointer or a vtable address and so
 * cannot be compared byte for byte between the two builds. */
static const int ptroff[] = {
	0x00,				/* PCRTC::mem */
	0x38,				/* PCRTC::ext */
	0x3c,				/* PCRTCxif vptr */
	0xbc, 0xc0,			/* di.dc.rd[0..1] */
	0xf0, 0x120, 0x150,		/* MemRead32/24/16 vptrs */
	0x1c0, 0x1c8,			/* PixelBlendAlp/1a vptrs */
	0x1cc,				/* di.bl */
	0x1d0				/* xif */
};
#define NPTR (int)(sizeof ptroff/sizeof ptroff[0])

/* --- the fake Xifbase ------------------------------------------------ */

struct vtent { short delta; short pad; void (*fn)(); };
struct vt9 { int z0, z1; struct vtent e[9]; };

struct fakexif { struct vt9 *vptr; };

static u32 hash[2];
static long ncall[2];
static int side;

static void
h(u32 v)
{
	hash[side] = (hash[side] ^ v) * 16777619u;
}

static void fx_dtor(void *t) { (void)t; ncall[side]++; h(0); }
static void fx_prep(void *t, int w, int hh)
	{ (void)t; ncall[side]++; h(1); h(w); h(hh); }
static void fx_draw5(void *t, int x, int y, int r, int g, int b)
	{ (void)t; ncall[side]++; h(2); h(x); h(y); h(r); h(g); h(b); }
static void fx_draw6(void *t, int x, int y, int r, int g, int b, int a)
	{ (void)t; ncall[side]++; h(3); h(x); h(y); h(r); h(g); h(b); h(a); }
static void fx_disp(void *t, int x, int y, int w, int hh)
	{ (void)t; ncall[side]++; h(4); h(x); h(y); h(w); h(hh); }
static void fx_resize(void *t, int w, int hh)
	{ (void)t; ncall[side]++; h(5); h(w); h(hh); }
static void fx_clear(void *t) { (void)t; ncall[side]++; h(6); }
static void fx_bg(void *t, int r, int g, int b)
	{ (void)t; ncall[side]++; h(7); h(r); h(g); h(b); }
static void fx_flush(void *t) { (void)t; ncall[side]++; h(8); }

static struct vt9 fakevt;
static struct fakexif fake[2];

static void
fakeinit(void)
{
	void (*fns[9])() = {
		(void (*)())fx_dtor, (void (*)())fx_prep,
		(void (*)())fx_draw5, (void (*)())fx_draw6,
		(void (*)())fx_disp, (void (*)())fx_resize,
		(void (*)())fx_clear, (void (*)())fx_bg,
		(void (*)())fx_flush
	};
	int i;

	for (i = 0; i < 9; i++) {
		fakevt.e[i].delta = 0;
		fakevt.e[i].pad = 0;
		fakevt.e[i].fn = fns[i];
	}
	fake[0].vptr = &fakevt;
	fake[1].vptr = &fakevt;
}

/* --- the two objects under test -------------------------------------- */

extern void *o_ctor(void *p, void *mem, char *name, int w, int h,
	void (*f)(int, int, const unsigned int *));
extern void o_setreg(void *p, int addr, ll data);
extern void o_resize(void *p, int w, int h);
extern void *n_ctor(void *p, void *mem, char *name, int w, int h,
	void (*f)(int, int, const unsigned int *));
extern void n_setreg(void *p, int addr, ll data);
extern void n_resize(void *p, int w, int h);

/* memory.o's WritePixel and addrconv.o's converter are shared; the two
 * PCRTCs get one Memory each so EXTWRITE can be compared. */
extern void address_convert__8AddrConviiiiiRiN56(void *self, int x, int y,
	int psm, int bw, int tbp, int *, int *, int *, int *, int *, int *);

void DbgWatch__Fiii(int a, int b, int c) { (void)a; (void)b; (void)c; }
void DoBitBLT__6BitBLTP6Memory(void *a, void *b) { (void)a; (void)b; }
void WritePixel__6BitBLTP6Memoryx(void *a, void *b, ll c)
	{ (void)a; (void)b; (void)c; }
long long ReadPixel__6BitBLTP6Memory(void *a, void *b)
	{ (void)a; (void)b; return 0; }
void OpenWindow__7XWindowPcii(void *a, char *b, int c, int d)
	{ (void)a; (void)b; (void)c; (void)d; }
void *_vt__7XWindow;
__asm__(".globl _vt.7XWindow\n	.set _vt.7XWindow, _vt__7XWindow");

/* --- Memory ----------------------------------------------------------- */

#define MEMSIZE		(VMWORDS*4 + 0x1c8)
#define FB_OFF		0x400000	/* Memory::fb */

static unsigned char *mem[2];

static void
memsetup(void)
{
	int s;

	for (s = 0; s < 2; s++) {
		mem[s] = malloc(MEMSIZE);
		memset(mem[s], 0, MEMSIZE);
	}
}

/* the active FBConfig, which EXTWRITE writes through */
static void
fbsetup(int psm, int fbmsk, int fba)
{
	int s;

	for (s = 0; s < 2; s++) {
		int *fb = (int *)(mem[s] + FB_OFF);

		fb[0x20/4] = 0;		/* FBP  - WritePixel takes it as an arg */
		fb[0x24/4] = 640;	/* FBW  - likewise */
		fb[0x28/4] = psm;
		fb[0x2c/4] = fbmsk;
		fb[0x30/4] = fba;
	}
}

/* --- Frame2d for EXTWRITE --------------------------------------------- */

struct frame2d { u32 *b; int w, h; };
static struct frame2d ext[2];

/* --- random ----------------------------------------------------------- */

static u32 seed = 20260903;

static u32
rnd(void)
{
	seed = seed*1103515245 + 12345;
	return seed >> 8;
}

static int
rr(int lo, int hi)
{
	return lo + (int)(rnd() % (u32)(hi - lo + 1));
}

static u64
bits(u64 v, int lo, int n)
{
	return (v & (((u64)1 << n) - 1)) << lo;
}

/* --- comparison -------------------------------------------------------- */

static long nchk, nfail;

static int
isptr(int off)
{
	int i;

	for (i = 0; i < NPTR; i++)
		if (ptroff[i] == off)
			return 1;
	return 0;
}

static void
fail(const char *what, long iter)
{
	printf("FAIL %s at iteration %ld\n", what, iter);
	if (++nfail > 8)
		exit(1);
}

static void
cmpobj(unsigned char *a, unsigned char *b, long iter)
{
	int i;

	nchk++;
	for (i = 0; i < PCRTC_SIZE; i += 4) {
		if (isptr(i)) {
			/* normalise: pointers into the object itself */
			long pa = *(long *)(a + i) - (long)a;
			long pb = *(long *)(b + i) - (long)b;

			if (i == 0x00 || i == 0x38 || i == 0x3c ||
			    i == 0xf0 || i == 0x120 || i == 0x150 ||
			    i == 0x1c0 || i == 0x1c8 || i == 0x1d0)
				continue;	/* per-side object address */
			if (pa != pb) {
				printf("  ptr slot 0x%x: %ld vs %ld\n",
					i, pa, pb);
				fail("object pointer", iter);
				return;
			}
			continue;
		}
		if (memcmp(a + i, b + i, 4) != 0) {
			printf("  offset 0x%x: %08x vs %08x\n", i,
				*(u32 *)(a + i), *(u32 *)(b + i));
			fail("object", iter);
			return;
		}
	}
}

/* --- register generation ----------------------------------------------- */

/* Keep the display rectangles tiny: the merge paths walk every pixel. */
static u64
genreg(int *addrp)
{
	static const int regs[] = {
		0x80, 0x80, 0x81, 0x82, 0x84, 0x85, 0x86,
		0x87, 0x88, 0x89, 0x8a, 0x8e, 0x8b, 0x8c
	};
	int a = regs[rnd() % (sizeof regs/sizeof regs[0])];
	u64 d = 0;

	*addrp = a;
	switch (a) {
	case 0x80:		/* PMODE */
		d = bits(rr(0, 1), 0, 1)	/* EN1 */
		  | bits(rr(0, 1), 1, 1)	/* EN2 */
		  | bits(rr(0, 7), 2, 3)	/* CRTMD */
		  | bits(rr(0, 1), 5, 1)	/* MMOD */
		  | bits(rr(0, 1), 6, 1)	/* AMOD */
		  | bits(rr(0, 1), 7, 1)	/* SLBG */
		  | bits(rnd() & 0xff, 8, 8);	/* ALP */
		if (rnd() % 4 == 0)
			d |= bits(0xff, 8, 8);	/* the opaque special case */
		break;
	case 0x81:		/* SMODE1 */
		d = bits(rr(0, 3), 13, 2) | bits(rnd(), 0, 13);
		break;
	case 0x82:		/* SMODE2 */
		d = bits(rr(0, 1), 0, 1) | bits(rnd(), 1, 20);
		break;
	case 0x84:		/* SYNCH1 */
	case 0x85:		/* SYNCH2 */
	case 0x86:		/* SYNCV */
		d = (u64)rnd() << 32 | rnd();
		break;
	case 0x87:		/* DISPFB1 */
	case 0x89:		/* DISPFB2 */
		{
			/* Only the four DISPFB.PSM values the model
			 * accepts: the invalid path stores the raw PSM
			 * into the MemRead anyway (an original bug, see
			 * doc/notes/pcrtc.md) and address_convert then
			 * refuses it. */
			static const int psms[] = { 0, 1, 2, 10 };
			int psm = psms[rnd() % 4];

			d = bits(rr(0, 8), 0, 9)	/* FBP */
			  | bits(rr(1, 10), 9, 6)	/* FBW */
			  | bits(psm, 15, 5)
			  | bits(rr(0, 31), 32, 11)	/* DBX */
			  | bits(rr(0, 31), 43, 11);	/* DBY */
		}
		break;
	case 0x88:		/* DISPLAY1 */
	case 0x8a:		/* DISPLAY2 */
		d = bits(rr(0, 40), 0, 12)		/* DX */
		  | bits(rr(0, 40), 12, 11)		/* DY */
		  | bits(rr(0, 3), 23, 4)		/* MAGH */
		  | bits(rr(0, 3), 27, 2)		/* MAGV */
		  | bits(rr(0, 31), 32, 12)		/* DW */
		  | bits(rr(0, 31), 44, 11);		/* DH */
		break;
	case 0x8e:		/* BGCOLOR */
		d = rnd() & 0xffffff;
		break;
	case 0x8b:		/* EXTBUF */
		d = bits(rr(0, 16), 0, 14)		/* EXBP */
		  | bits(rr(1, 10), 14, 6)		/* EXBW */
		  | bits(rr(0, 3), 20, 2)		/* FBIN */
		  | bits(rr(0, 1), 22, 1)		/* WFFMD */
		  | bits(rr(0, 3), 23, 2)		/* EMODA */
		  | bits(rr(0, 3), 25, 2)		/* EMODC */
		  | bits(rr(0, 15), 32, 11)		/* WDX */
		  | bits(rr(0, 15), 43, 11);		/* WDY */
		break;
	case 0x8c:		/* EXTDATA */
		d = bits(rr(0, 15), 0, 12)		/* SX */
		  | bits(rr(0, 15), 12, 11)		/* SY */
		  | bits(rr(0, 3), 23, 4)		/* SMPH */
		  | bits(rr(0, 3), 27, 2)		/* SMPV */
		  | bits(rr(0, 7), 32, 12)		/* WW */
		  | bits(rr(0, 7), 44, 11);		/* WH */
		break;
	}
	return d;
}

int
main(int argc, char **argv)
{
	long n = argc > 1 ? atol(argv[1]) : 200000;
	long i, ndisp = 0, npix = 0;
	unsigned char *p[2];
	int s, j;

	fakeinit();
	memsetup();

	/* identical pseudo-random VRAM on both sides; only the real 4 MB,
	 * so the slack behind it stays zero and the FB/ZB config block that
	 * follows it is not clobbered */
	for (j = 0; j < 0x100000; j++) {
		u32 v = rnd();

		((u32 *)mem[0])[j] = v;
		((u32 *)mem[1])[j] = v;
	}

	fbsetup(0, 0, 0);

	/* the external video source EXTWRITE reads */
	for (s = 0; s < 2; s++) {
		ext[s].w = 64;
		ext[s].h = 64;
		ext[s].b = malloc(64*64*4);
	}
	for (j = 0; j < 64*64; j++) {
		u32 v = rnd();

		ext[0].b[j] = v;
		ext[1].b[j] = v;
	}

	p[0] = malloc(PCRTC_SIZE);
	p[1] = malloc(PCRTC_SIZE);
	memset(p[0], 0xa5, PCRTC_SIZE);
	memset(p[1], 0xa5, PCRTC_SIZE);
	side = 0; o_ctor(p[0], mem[0], "o", 64, 64, 0);
	side = 1; n_ctor(p[1], mem[1], "n", 64, 64, 0);
	for (s = 0; s < 2; s++) {
		*(void **)(p[s] + 0x1d0) = &fake[s];
		*(void **)(p[s] + 0x38) = &ext[s];	/* PCRTC::ext */
	}
	cmpobj(p[0], p[1], -1);

	for (i = 0; i < n; i++) {
		int addr;
		u64 d = genreg(&addr);

		side = 0; o_setreg(p[0], addr, (ll)d);
		side = 1; n_setreg(p[1], addr, (ll)d);
		cmpobj(p[0], p[1], i);
		if (hash[0] != hash[1] || ncall[0] != ncall[1])
			fail("callback stream", i);

		if (i % 512 == 511) {
			/* vary the destination frame buffer EXTWRITE
			 * writes through: 32, 24 and 16 bit, with and
			 * without a write mask and the FBA bit */
			static const int psms[] = {
				0, 1, 2, 10, 0x30, 0x31, 0x32, 0x3a
			};

			fbsetup(psms[rnd() % 8],
				(rnd() % 4) ? 0 : (int)rnd(), rr(0, 1));
		}
		if (i % 64 == 63) {		/* EXTWRITE */
			side = 0; o_setreg(p[0], 0x8d, 0);
			side = 1; n_setreg(p[1], 0x8d, 0);
			if (memcmp(mem[0], mem[1], MEMSIZE) != 0)
				fail("EXTWRITE local memory", i);
			nchk++;
		}
		if (i % 16 == 15) {		/* DisplayPcrtc: the merge */
			long c0 = ncall[0];
			ll dn = (ll)(rnd() & 1);

			side = 0; o_setreg(p[0], 0x101, dn);
			side = 1; n_setreg(p[1], 0x101, dn);
			if (hash[0] != hash[1] || ncall[0] != ncall[1])
				fail("DisplayPcrtc", i);
			ndisp++;
			npix += ncall[0] - c0;
			nchk++;
		}
		if (i % 128 == 127) {		/* Display: the old path */
			u64 dd = bits(rr(0, 1), 0, 2)
			       | bits(rr(0, 3), 32, 16)
			       | bits(rr(0, 3), 48, 16);
			long c0 = ncall[0];

			side = 0; o_setreg(p[0], 0x100, (ll)dd);
			side = 1; n_setreg(p[1], 0x100, (ll)dd);
			if (hash[0] != hash[1] || ncall[0] != ncall[1])
				fail("Display", i);
			ndisp++;
			npix += ncall[0] - c0;
			nchk++;
		}
		if (i % 256 == 255) {		/* Resize */
			int w = rr(1, 640), hh = rr(1, 480);

			side = 0; o_resize(p[0], w, hh);
			side = 1; n_resize(p[1], w, hh);
			if (hash[0] != hash[1] || ncall[0] != ncall[1])
				fail("Resize", i);
			nchk++;
		}
	}

	printf("%ld register writes, %ld displays, %ld callbacks "
		"(%ld display callbacks), %ld checks, %ld failures\n",
		n, ndisp, ncall[0], npix, nchk, nfail);
	return nfail != 0;
}
