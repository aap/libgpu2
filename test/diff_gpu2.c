/* Differential test: reconstructed gpu2.o against the 1998 object.
 *
 * Both GPU2s are driven through the whole public surface:
 *
 *  - GPU2::GPU2 for disp_on 0, 1 and 2, with a recording allocator
 *    (__builtin_new and malloc share one bump pool per side), recording
 *    stubs for the out-of-line constructors (param, Reciproc, Pre1) and
 *    for OpenWindow, and fake g++ 2.7 vtables for the external
 *    _vt.* the constructor stores.  After each construction every
 *    allocation is byte-compared between the sides, with any word that
 *    points into the pool translated to its pool offset first.
 *  - the disp_on = 3 error arm (fprintf + exit, caught with longjmp)
 *    and the width <= 0 assert arm (__assert_fail, caught the same way,
 *    which also checks the "../gpu2u/xif.h" __FILE__ and line 139).
 *  - GPU2::Put for every addr in -0x80..0x10f: the drawing range goes
 *    to the Pre1::Put recording stub, the privileged range through the
 *    real PCRTCdmy vtable into each side's own weak
 *    PCRTC::SetRegister (EXTBUF/EXTDATA decode compared as object
 *    bytes), and on a disp_on = 1 object through the fake
 *    _vt.8PCRTCxif into a recorder.
 *  - GPU2::Get through a ReadPixel(BitBLT*, Memory*) recorder.
 *  - GPU2::ResizeWindow through both vtables.
 *  - the XWindowDump path end to end: SetBackground / ClearDisplay /
 *    DrawPixel / PrepareImgBuffer / Resize / DisplayPixel through the
 *    constructed object's own vtable, which lands in dumpCRT (the
 *    static callback, reached via the pointer the constructor stored),
 *    then GetCRT drains the captured frame from each side's own bss.
 *  - MemRead16/24/32::ReadPixel and PixelBlend1a/Alp::blend called
 *    directly against a patterned 4 MB Memory arena (address_convert
 *    comes from the real 1998 addrconv.o, shared by both sides).
 *
 * Built by test/run_gpu2.sh, which renames the 1998 symbols to o_* and
 * the reconstructed ones to n_* first.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#define GPU2_SIZE	0x18
#define MEMBLK_SIZE	0x4001c8
#define PCRTCDMY_SIZE	0x40
#define PCRTCXIF_SIZE	0x1d4
#define XWD_SIZE	0x24

/* GPU2 member offsets */
#define G_PP	0x00
#define G_DDA	0x04
#define G_TXM	0x08
#define G_MEMIF	0x0c
#define G_MEM	0x10
#define G_PCRTC	0x14

#define F(p, off)	(*(int *)((char *)(p) + (off)))
#define FP(p, off)	(*(void **)((char *)(p) + (off)))

/* --- the two implementations ---------------------------------------- */

extern void *o_ctor(void *g, char *title, int w, int h, int disp);
extern int o_put(void *g, int addr, long long data);
extern long long o_get(void *g);
extern unsigned int o_getcrt(void *g);
extern void o_resizewindow(void *g, int w, int h);
extern void o_rp16(void *mr, void *mem, int x, int y, void *c);
extern void o_rp24(void *mr, void *mem, int x, int y, void *c);
extern void o_rp32(void *mr, void *mem, int x, int y, void *c);
extern void o_blend1a(void *bl, void *c, const void *d);
extern void o_blendalp(void *bl, void *c, const void *d);
extern void o_ppout_put(void *po, void *pcalc);

extern void *n_ctor(void *g, char *title, int w, int h, int disp);
extern int n_put(void *g, int addr, long long data);
extern long long n_get(void *g);
extern unsigned int n_getcrt(void *g);
extern void n_resizewindow(void *g, int w, int h);
extern void n_rp16(void *mr, void *mem, int x, int y, void *c);
extern void n_rp24(void *mr, void *mem, int x, int y, void *c);
extern void n_rp32(void *mr, void *mem, int x, int y, void *c);
extern void n_blend1a(void *bl, void *c, const void *d);
extern void n_blendalp(void *bl, void *c, const void *d);
extern void n_ppout_put(void *po, void *pcalc);

/* --- test bookkeeping ------------------------------------------------ */

static int side;		/* 0 = old, 1 = new */
static long nchecks, nfail;
static jmp_buf escape;		/* exit()/__assert_fail() land here */
static int escaped;

static void
fail(const char *what, long a, long b)
{
	nfail++;
	if (nfail <= 20)
		printf("FAIL %s: old %lx new %lx\n", what, a, b);
}

static void
check(const char *what, long a, long b)
{
	nchecks++;
	if (a != b)
		fail(what, a, b);
}

/* --- event log: every stub call appends; the two sides' logs must
 * agree word for word -------------------------------------------------- */

#define LOGCAP	(1 << 20)
static unsigned int logbuf[2][LOGCAP];
static int logn[2];

static void
logv(unsigned int v)
{
	if (logn[side] < LOGCAP)
		logbuf[side][logn[side]] = v;
	logn[side]++;
}

static void
logcmp(void)
{
	int i, n;

	nchecks++;
	if (logn[0] != logn[1]) {
		nfail++;
		printf("FAIL log length: old %d new %d\n", logn[0], logn[1]);
	}
	n = logn[0] < logn[1] ? logn[0] : logn[1];
	if (n > LOGCAP)
		n = LOGCAP;
	for (i = 0; i < n; i++) {
		nchecks++;
		if (logbuf[0][i] != logbuf[1][i]) {
			nfail++;
			if (nfail <= 20)
				printf("FAIL log[%d]: old %08x new %08x\n",
					i, logbuf[0][i], logbuf[1][i]);
		}
	}
	logn[0] = logn[1] = 0;
}

/* --- the recording allocator ----------------------------------------- */

#define POOLCAP	(56u << 20)
static char *pool[2];
static unsigned int pooln[2];
#define MAXALLOC 64
static unsigned int alloco[2][MAXALLOC], allocs[2][MAXALLOC];
static int nalloc[2];

static unsigned int
poolrel(void *p)
{
	unsigned long d = (char *)p - pool[side];

	return d < POOLCAP ? (unsigned int)d : 0xdead0000u;
}

static void *
bump(unsigned int size)
{
	void *p = pool[side] + pooln[side];

	if (pooln[side] + size > POOLCAP) {
		printf("pool overflow\n");
		exit(1);
	}
	/* deterministic garbage so uninitialised fields compare equal */
	memset(p, 0xa5, size);
	if (nalloc[side] < MAXALLOC) {
		alloco[side][nalloc[side]] = pooln[side];
		allocs[side][nalloc[side]] = size;
	}
	nalloc[side]++;
	pooln[side] += (size + 7) & ~7u;
	return p;
}

void *
stub_new(unsigned int size)
{
	logv(1); logv(size);
	return bump(size);
}

void
stub_delete(void *p)
{
	logv(2); logv(poolrel(p));
}

void *
stub_malloc(unsigned int size)
{
	logv(3); logv(size);
	return bump(size);
}

void *
stub_realloc(void *p, unsigned int size)
{
	void *q;

	logv(4); logv(poolrel(p)); logv(size);
	q = bump(size);
	if (p)
		memcpy(q, p, size);	/* pool blocks are contiguous; fine */
	return q;
}

void
stub_free(void *p)
{
	logv(5); logv(poolrel(p));
}

/* --- out-of-line constructor / call stubs ---------------------------- */

void *
stub_pre1_ctor(void *p1, void *p3)
{
	logv(6); logv(poolrel(p1)); logv(poolrel(p3));
	memset(p1, 0x5a, 0xac);
	return p1;	/* g++ 2.7 ctors return this */
}

void
stub_pre1_put(void *p1, int addr, long long data)
{
	logv(7); logv(poolrel(p1)); logv(addr);
	logv((unsigned int)data); logv((unsigned int)(data >> 32));
}

void *
stub_param_ctor(void *p)
{
	logv(8); logv(poolrel(p));
	memset(p, 0x3c, 0x50);
	return p;
}

void *
stub_reciproc_ctor(void *p)
{
	logv(9); logv(poolrel(p));
	memset(p, 0x69, 0x8);
	return p;
}

void
stub_openwindow(void *xw, char *name, int w, int h)
{
	logv(10); logv(poolrel(xw));
	logv(name ? (unsigned int)(unsigned char)name[0] : 0xff);
	logv(w); logv(h);
}

long long
stub_bitblt_readpixel(void *blt, void *mem)
{
	logv(11); logv(poolrel(blt)); logv(poolrel(mem));
	return 0x0123456789abcdefLL;
}

void
stub_fbwritepixel(void *fb, void *mem, int x, int y,
	int c0, int c1, int c2, int c3, int f, int bp, int bw)
{
	logv(12);	/* not expected to be reached */
}

void
stub_purevirtual(void)
{
	printf("__pure_virtual reached\n");
	exit(1);
}

char fake_stderr[256];

void
stub_fprintf(void *f, const char *fmt, ...)
{
	int i;

	logv(13); logv(f == (void *)fake_stderr);
	for (i = 0; fmt[i]; i++)
		logv((unsigned char)fmt[i]);
	logv(*(unsigned int *)((char *)&fmt + 4));	/* first vararg */
}

void
stub_exit(int code)
{
	logv(14); logv(code);
	escaped = 1;
	longjmp(escape, 1);
}

void
stub_assert_fail(const char *e, const char *file, unsigned int line,
	const char *fn)
{
	int i;

	logv(15);
	for (i = 0; e[i]; i++) logv((unsigned char)e[i]);
	for (i = 0; file[i]; i++) logv((unsigned char)file[i]);
	logv(line);
	for (i = 0; fn[i]; i++) logv((unsigned char)fn[i]);
	escaped = 2;
	longjmp(escape, 1);
}

/* --- fake vtables (g++ 2.7 non-thunk format) ------------------------- */

struct vtent {
	short delta;
	short pad;
	void *fn;
};
struct vtable {
	int zero0, zero1;
	struct vtent e[10];
};

static void
rec_dda_put(void *dda, void *pcalc)
{
	logv(16); logv(poolrel(dda)); logv(poolrel(pcalc));
}

static void
rec_xif_setreg(void *pc, int addr, long long data)
{
	logv(17); logv(poolrel(pc)); logv(addr);
	logv((unsigned int)data); logv((unsigned int)(data >> 32));
}

static void
rec_xif_resize(void *pc, int w, int h)
{
	logv(18); logv(poolrel(pc)); logv(w); logv(h);
}

/* stored into objects by the constructor; only dda's and pcrtcxif's are
 * ever called through in this test */
struct vtable fake_vt_dda;
struct vtable fake_vt_txm;
struct vtable fake_vt_pre3;
struct vtable fake_vt_pcalc;
struct vtable fake_vt_memif;
struct vtable fake_vt_xwindow;
struct vtable fake_vt_pcrtcxif;

/* --- comparing the object graph -------------------------------------- */

/* A word that points into the executable image itself is a vptr (each
 * side's own local _vt.* copy) or a code pointer (dumpCRT): the
 * addresses legitimately differ between the sides, so only the fact
 * that both are image pointers is compared. */
static int
inimage(unsigned int v)
{
	return v - 0x08048000u < 0x00400000u;
}

/* Compare allocation k of both sides word by word; a word that points
 * into its own side's pool is translated to the pool offset first. */
static void
cmpalloc(int k)
{
	unsigned int sa = allocs[0][k], sb = allocs[1][k];
	unsigned char *a = (unsigned char *)pool[0] + alloco[0][k];
	unsigned char *b = (unsigned char *)pool[1] + alloco[1][k];
	unsigned int i, wa, wb, ta, tb;

	check("alloc offset", alloco[0][k], alloco[1][k]);
	check("alloc size", sa, sb);
	if (sa != sb)
		return;
	for (i = 0; i + 4 <= sa; i += 4) {
		wa = *(unsigned int *)(a + i);
		wb = *(unsigned int *)(b + i);
		ta = wa - (unsigned int)(unsigned long)pool[0];
		tb = wb - (unsigned int)(unsigned long)pool[1];
		if (ta < POOLCAP && tb < POOLCAP) {
			wa = ta;
			wb = tb;
		} else if (inimage(wa) && inimage(wb))
			wa = wb = 0;
		nchecks++;
		if (wa != wb) {
			nfail++;
			if (nfail <= 20)
				printf("FAIL alloc %d +0x%x: old %08x new %08x\n",
					k, i, wa, wb);
		}
	}
}

static void
cmpgraph(void)
{
	int k, n;

	check("allocation count", nalloc[0], nalloc[1]);
	n = nalloc[0] < nalloc[1] ? nalloc[0] : nalloc[1];
	if (n > MAXALLOC)
		n = MAXALLOC;
	for (k = 0; k < n; k++)
		cmpalloc(k);
}

static void
resetpool(void)
{
	nalloc[0] = nalloc[1] = 0;
	pooln[0] = pooln[1] = 0;
}

/* --- constructing --------------------------------------------------- */

static char title[] = "diffgpu2";

/* Construct one GPU2 on the current side; returns 0 if the ctor escaped
 * through exit()/assert.  *esc gets the escape reason. */
static void *
construct(void *(*ctor)(void *, char *, int, int, int),
	int w, int h, int disp, int *esc)
{
	void *g = bump(GPU2_SIZE);
	void *r;

	escaped = 0;
	if (setjmp(escape)) {
		*esc = escaped;
		return 0;
	}
	r = (*ctor)(g, title, w, h, disp);
	check("ctor returns this", (long)(r == g), 1);
	*esc = 0;
	return g;
}

int
main(void)
{
	void *g[2], *xd[2], *pcx[2];
	static unsigned char c0[2][16], c1[2][16];
	static char frame[2][0x40][0x40][4];
	void *arena[2];
	int esc[2];
	int i, j, x, y, s, k;
	long long v[2];
	unsigned int u[2];

	pool[0] = malloc(POOLCAP);
	pool[1] = malloc(POOLCAP);
	if (!pool[0] || !pool[1]) {
		printf("no pool\n");
		return 1;
	}

	/* fake vtable entries */
	fake_vt_dda.e[0].fn = (void *)rec_dda_put;
	fake_vt_pcrtcxif.e[0].fn = (void *)rec_xif_setreg;
	fake_vt_pcrtcxif.e[1].fn = (void *)rec_xif_resize;

	/* ---- disp_on = 0: construct, compare graph ---- */
	side = 0; g[0] = construct(o_ctor, 640, 480, 0, &esc[0]);
	side = 1; g[1] = construct(n_ctor, 640, 480, 0, &esc[1]);
	check("disp0 escape", esc[0], esc[1]);
	logcmp();
	cmpgraph();

	/* ---- GPU2::Put over the whole address space ---- */
	for (i = -0x80; i < 0x110; i++) {
		long long d = ((long long)(0xabcd0000 + i) << 32)
			| (0x1234u + i);

		side = 0; s = o_put(g[0], i, d);
		side = 1; k = n_put(g[1], i, d);
		check("put retval", s, k);
		logcmp();
	}
	/* the PCRTCdmy decode state (EXTBUF/EXTDATA land in the object) */
	side = 0; o_put(g[0], 0x8b, 0x123456789abcdefLL);
	side = 1; n_put(g[1], 0x8b, 0x123456789abcdefLL);
	side = 0; o_put(g[0], 0x8c, 0x0fedcba987654321LL);
	side = 1; n_put(g[1], 0x8c, 0x0fedcba987654321LL);
	side = 0; o_put(g[0], 0x8d, 0x1LL);	/* ext==0: early out */
	side = 1; n_put(g[1], 0x8d, 0x1LL);
	logcmp();
	for (i = 0; i < PCRTCDMY_SIZE; i += 4) {
		unsigned int wa = F(FP(g[0], G_PCRTC), i);
		unsigned int wb = F(FP(g[1], G_PCRTC), i);
		unsigned int ta = wa - (unsigned int)(unsigned long)pool[0];
		unsigned int tb = wb - (unsigned int)(unsigned long)pool[1];

		if (ta < POOLCAP && tb < POOLCAP) {	/* mem */
			wa = ta;
			wb = tb;
		} else if (inimage(wa) && inimage(wb))	/* the vptr */
			wa = wb = 0;
		check("pcrtcdmy word", wa, wb);
	}

	/* ---- GPU2::Get ---- */
	side = 0; v[0] = o_get(g[0]);
	side = 1; v[1] = n_get(g[1]);
	check("get lo", (long)(unsigned int)v[0], (long)(unsigned int)v[1]);
	check("get hi", (long)(unsigned int)(v[0] >> 32),
		(long)(unsigned int)(v[1] >> 32));
	logcmp();

	/* ---- ResizeWindow through the real PCRTCdmy vtable ---- */
	side = 0; o_resizewindow(g[0], 512, 384);
	side = 1; n_resizewindow(g[1], 512, 384);
	logcmp();

	/* ---- PPOut::Put forwards through the fake DDA vtable ---- */
	{
		void *po[2];

		side = 0; po[0] = FP(FP(g[0], G_PP), 0x0c);
		side = 1; po[1] = FP(FP(g[1], G_PP), 0x0c);
		side = 0; o_ppout_put(po[0], (char *)pool[0] + 0x100);
		side = 1; n_ppout_put(po[1], (char *)pool[1] + 0x100);
		logcmp();
	}

	/* ---- disp_on = 1: OpenWindow stub; Put/Resize through the fake
	 * _vt.8PCRTCxif ---- */
	resetpool();
	side = 0; g[0] = construct(o_ctor, 640, 224, 1, &esc[0]);
	side = 1; g[1] = construct(n_ctor, 640, 224, 1, &esc[1]);
	check("disp1 escape", esc[0], esc[1]);
	logcmp();
	cmpgraph();
	pcx[0] = FP(g[0], G_PCRTC);
	pcx[1] = FP(g[1], G_PCRTC);
	/* the same address sweep, now through the fake _vt.8PCRTCxif: the
	 * recorder sees every privileged addr, so a routing change on
	 * either side of the 0x00..0xff boundary cannot hide */
	for (i = -0x80; i < 0x110; i++) {
		long long d = ((long long)(0x55aa0000 + i) << 32)
			| (0x9876u + i);

		side = 0; o_put(g[0], i, d);
		side = 1; n_put(g[1], i, d);
		logcmp();
	}
	side = 0; o_put(g[0], 0x100, 0x10001LL);
	side = 1; n_put(g[1], 0x100, 0x10001LL);
	side = 0; o_put(g[0], -5, 0x77LL);
	side = 1; n_put(g[1], -5, 0x77LL);
	side = 0; o_resizewindow(g[0], 320, 240);
	side = 1; n_resizewindow(g[1], 320, 240);
	logcmp();

	/* ---- MemRead16/24/32::ReadPixel against a patterned arena ----
	 * (the three readers the constructor built inside DispCirc; the
	 * real address_convert from 1998 addrconv.o does the swizzle) */
	arena[0] = FP(g[0], G_MEM);
	arena[1] = FP(g[1], G_MEM);
	for (s = 0; s < 2; s++)
		for (i = 0; i < 0x40000; i++)
			((unsigned int *)arena[s])[i] =
				i * 0x9e3779b9u + 0x12345;
	for (y = 0; y < 48; y++)
		for (x = 0; x < 64; x += 3) {
			side = 0; o_rp32((char *)pcx[0] + 0xc4, arena[0],
				x, y, c0[0]);
			side = 1; n_rp32((char *)pcx[1] + 0xc4, arena[1],
				x, y, c0[1]);
			for (i = 0; i < 16; i++)
				check("rp32", c0[0][i], c0[1][i]);
			side = 0; o_rp24((char *)pcx[0] + 0xf4, arena[0],
				x, y, c0[0]);
			side = 1; n_rp24((char *)pcx[1] + 0xf4, arena[1],
				x, y, c0[1]);
			for (i = 0; i < 16; i++)
				check("rp24", c0[0][i], c0[1][i]);
			side = 0; o_rp16((char *)pcx[0] + 0x124, arena[0],
				x, y, c0[0]);
			side = 1; n_rp16((char *)pcx[1] + 0x124, arena[1],
				x, y, c0[1]);
			for (i = 0; i < 16; i++)
				check("rp16", c0[0][i], c0[1][i]);
		}
	logcmp();

	/* ---- PixelBlend1a / PixelBlendAlp ---- */
	for (i = 0; i < 256; i += 5)
		for (j = 0; j < 256; j += 7) {
			struct { void *vt; int alp; } bl;

			bl.vt = 0;
			bl.alp = j;
			for (s = 0; s < 2; s++) {
				F(c0[s], 0) = i;	/* R */
				F(c0[s], 4) = 255 - i;	/* G */
				F(c0[s], 8) = i ^ 0x55;	/* B */
				F(c0[s], 12) = j;	/* A */
				F(c1[s], 0) = j;
				F(c1[s], 4) = i;
				F(c1[s], 8) = (i + j) & 0xff;
				F(c1[s], 12) = i;
			}
			side = 0; o_blend1a(&bl, c0[0], c1[0]);
			side = 1; n_blend1a(&bl, c0[1], c1[1]);
			for (k = 0; k < 16; k++)
				check("blend1a", c0[0][k], c0[1][k]);
			bl.alp = j;
			side = 0; o_blendalp(&bl, c0[0], c1[0]);
			side = 1; n_blendalp(&bl, c0[1], c1[1]);
			for (k = 0; k < 16; k++)
				check("blendalp", c0[0][k], c0[1][k]);
		}

	/* ---- disp_on = 2: the XWindowDump + dumpCRT + GetCRT loop ---- */
	resetpool();
	side = 0; g[0] = construct(o_ctor, 0x40, 0x30, 2, &esc[0]);
	side = 1; g[1] = construct(n_ctor, 0x40, 0x30, 2, &esc[1]);
	check("disp2 escape", esc[0], esc[1]);
	logcmp();
	cmpgraph();
	for (s = 0; s < 2; s++)
		xd[s] = FP((char *)FP(g[s], G_PCRTC), 0x1d0);

	/* draw two frames through the object's own vtable; entry order:
	 * 0 dtor, 1 PrepareImgBuffer, 2 DrawPixel5, 3 DrawPixel6,
	 * 4 DisplayPixel, 5 Resize, 6 ClearDisplay, 7 SetBackground,
	 * 8 Flush */
	for (j = 0; j < 2; j++) {
		for (s = 0; s < 2; s++) {
			struct vtable *vt = *(struct vtable **)xd[s];
			void (*prep)(void *, int, int) =
				(void (*)(void *, int, int))vt->e[1].fn;
			void (*draw5)(void *, int, int, int, int, int) =
				(void (*)(void *, int, int, int, int, int))
				vt->e[2].fn;
			void (*draw6)(void *, int, int, int, int, int, int) =
				(void (*)(void *, int, int, int, int, int, int))
				vt->e[3].fn;
			void (*disp)(void *, int, int, int, int) =
				(void (*)(void *, int, int, int, int))
				vt->e[4].fn;
			void (*clear)(void *) = (void (*)(void *))vt->e[6].fn;
			void (*setbg)(void *, int, int, int) =
				(void (*)(void *, int, int, int))vt->e[7].fn;

			side = s;
			(*setbg)(xd[s], 0x20 + j, 0x40, 0x87);
			(*clear)(xd[s]);
			(*prep)(xd[s], 0x40, 0x30);
			for (y = 0; y < 0x30; y += 2)
				for (x = 0; x < 0x40; x += 3) {
					(*draw5)(xd[s], x, y,
						x ^ y, x + j, y);
					if (x + 1 < 0x40)
						(*draw6)(xd[s], x + 1, y,
							y, x, j, x ^ 0x33);
				}
			(*disp)(xd[s], 0, 0, 0x40, 0x30);
		}
		logcmp();
		/* drain each side's capture through GetCRT (0x40*0x30
		 * pixels, then the 0-on-empty tail) */
		for (i = 0; i < 0x40 * 0x30 + 16; i++) {
			side = 0; u[0] = o_getcrt(g[0]);
			side = 1; u[1] = n_getcrt(g[1]);
			check("getcrt", u[0], u[1]);
		}
		logcmp();
	}

	/* ---- disp_on = 3: the fprintf/exit arm ---- */
	resetpool();
	side = 0; g[0] = construct(o_ctor, 640, 480, 3, &esc[0]);
	side = 1; g[1] = construct(n_ctor, 640, 480, 3, &esc[1]);
	check("disp3 escaped via exit", esc[0], 1);
	check("disp3 escape", esc[0], esc[1]);
	logcmp();

	/* ---- the width<=0 assert arm (disp_on = 2, w = 0) ---- */
	resetpool();
	side = 0; g[0] = construct(o_ctor, 0, 16, 2, &esc[0]);
	side = 1; g[1] = construct(n_ctor, 0, 16, 2, &esc[1]);
	check("assert escaped", esc[0], 2);
	check("assert escape", esc[0], esc[1]);
	logcmp();

	printf("%ld checks, %ld failures\n", nchecks, nfail);
	return nfail != 0;
}
