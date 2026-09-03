/* Differential test: reconstructed gpu2vec.o against the 1998 object.
 *
 * gpu2vec.o is Sony's pipeline-tap / RTL-test-vector layer.  The two
 * copies are renamed apart (o_* / n_*) and linked into one binary; every
 * symbol they import from the rest of the archive - the allocator, the
 * out-of-line constructors, the four downstream stage entries
 * (Pre1::Put, DDA::Put, TXM::Put, MemIF::Stamp), OpenWindow, the
 * external vtables, stderr and exit - is a recording stub here, so the
 * comparison is about gpu2vec.o alone.  <stdio.h> itself is NOT stubbed:
 * fputc / sprintf / sscanf / strcat run for real, because the whole
 * point of this object is the bytes it writes.
 *
 * What is exercised:
 *
 *  - GPU2VEC::GPU2VEC for disp_on 0, 1 and 2, with a recording bump
 *    allocator per side.  Every allocation (MyMemory 0x4001d0, MyMemIF
 *    0x100, MyTXM 0x730, MyDDA 0x258, MyPP 0x14, PPOut 8, PCalc 0xc00,
 *    Pre3, Pre1, and the PCRTC arm) is byte-compared with pool
 *    pointers and image pointers normalised, so the wiring - which
 *    object is newed, in what order, at what size, and which pointer
 *    lands in which slot - is compared exhaustively.
 *  - the disp_on = 3 error arm (fprintf(stderr) + exit, caught with
 *    longjmp) and the width <= 0 assert arm from xif.h.
 *  - GPU2VEC::Put for every addr in -0x80..0x10f: the drawing range
 *    goes through MyPP::Put to the Pre1::Put recorder, the privileged
 *    range through the PCRTC vtable, and addr 0x7f additionally fires
 *    MyMemory::Dump.
 *  - GPU2VEC::Get, GPU2VEC::ResizeWindow, GPU2VEC::GetCRT.
 *  - GPU2VEC::SetVector(sel, f) for sel 0..7: which object's fp slot
 *    receives f, and that `vec' is set for every sel.
 *  - THE VECTOR FILE BYTES.  Every tap is driven into a tmpfile on each
 *    side and the two files are compared byte for byte:
 *      sel 6  GPU2VEC::Put's raw register trace
 *      sel 1  MyPP::Put's Pre1-input trace
 *      sel 2  MyDDA::Put -> RegisterVec / TriangleVec  (4096 PCalcs)
 *      sel 3  MyTXM::Put -> RegisterVec / PrimitiveVec (4096 DDAs)
 *      sel 4  MyMemIF::Stamp -> RegisterVec / PrimitiveVec (4096 stamps)
 *      sel 5  MyMemory::Dump (the whole 4 MB of vram, 1M lines)
 *    The stage objects are filled from a xorshift PRNG so every field
 *    of every record is a different value each iteration, and the
 *    selector fields (send_type, AA1, type, TME, FGE, m_bf4, isreg,
 *    first, PixelStamp::type/reg/mask/aamask/m_30/m_34/m_40) are cycled
 *    so every conditional column - every x-run, every forced-zero
 *    column, every PutX arm - is taken in both directions.
 *  - the MyDDA / MyTXM / MyMemIF destructors (fclose on a live tap).
 *
 * Built by test/run_gpu2vec.sh, which does the renaming.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <unistd.h>

/* ---- object sizes and offsets (read off the 1998 constructor) ------- */

#define GPU2VEC_SIZE	0x20
#define MYMEMORY_SIZE	0x4001d0
#define MYMEMIF_SIZE	0x100
#define MYTXM_SIZE	0x730
#define MYDDA_SIZE	0x258
#define MYPP_SIZE	0x14
#define PCALC_SIZE	0xc00
#define PCRTCDMY_SIZE	0x40
#define PCRTCXIF_SIZE	0x1d4
#define PIXELSTAMP_SIZE	0x3cc
#define DDA_SIZE	0x250

/* GPU2VEC */
#define V_PP	0x00
#define V_DDA	0x04
#define V_TXM	0x08
#define V_MEMIF	0x0c
#define V_MEM	0x10
#define V_PCRTC	0x14
#define V_VEC	0x18
#define V_FP	0x1c

/* the per-stage tap slots */
#define PP_FP		0x10
#define DDA_FP		0x254
#define TXM_FP		0x72c
#define MEMIF_FP	0xfc
#define MEMORY_FP	0x4001c8

/* selector fields */
#define PC_SCANMSK	0xbb0
#define PC_SENDTYPE	0xbb4
#define PC_TME		0xbc4
#define PC_FGE		0xbc8
#define PC_ABE		0xbcc
#define PC_AA1		0xbd0
#define PC_CTXT		0xbd8
#define PC_FST		0xbdc
#define PC_BF4		0xbf4
#define PC_TYPE		0xbf8

#define D_ISREG		0x158
#define D_FIRST		0x15c
#define D_YDIR		0x228
#define D_TME		0x22c
#define D_FGE		0x230
#define D_ABE		0x234
#define D_FST		0x238
#define D_AA1		0x23c
#define D_CTXT		0x244

#define S_TYPE		0x000
#define S_REG		0x004
#define S_MASK		0x01c
#define S_AAMASK	0x028
#define S_M30		0x030
#define S_M34		0x034
#define S_ABE		0x038
#define S_M40		0x040
#define S_CTXT		0x048

#define F(p, off)	(*(int *)((char *)(p) + (off)))
#define FP(p, off)	(*(void **)((char *)(p) + (off)))

/* ---- the two implementations ---------------------------------------- */

extern void *o_ctor(void *g, char *title, int w, int h, int disp);
extern int o_put(void *g, int addr, long long data);
extern long long o_get(void *g);
extern unsigned int o_getcrt(void *g);
extern void o_resizewindow(void *g, int w, int h);
extern void o_setvector(void *g, int sel, void *f);
extern void o_mypp_put(void *pp, int addr, long long data);
extern void o_dda_put(void *d, void *pcalc);
extern void o_txm_put(void *t, void *dda);
extern void o_mif_stamp(void *m, void *stamp);
extern void o_mem_dump(void *m);
extern void o_dda_setvec(void *d, void *f);
extern void o_txm_setvec(void *t, void *f);
extern void o_mif_setvec(void *m, void *f);
extern void o_dda_dtor(void *d, int inchrg);
extern void o_txm_dtor(void *t, int inchrg);
extern void o_mif_dtor(void *m, int inchrg);
extern void *o_dda_ctor(void *d, void *txm);
extern void *o_txm_ctor(void *t, void *memif);
extern void *o_mif_ctor(void *m, void *mem);

extern void *n_ctor(void *g, char *title, int w, int h, int disp);
extern int n_put(void *g, int addr, long long data);
extern long long n_get(void *g);
extern unsigned int n_getcrt(void *g);
extern void n_resizewindow(void *g, int w, int h);
extern void n_setvector(void *g, int sel, void *f);
extern void n_mypp_put(void *pp, int addr, long long data);
extern void n_dda_put(void *d, void *pcalc);
extern void n_txm_put(void *t, void *dda);
extern void n_mif_stamp(void *m, void *stamp);
extern void n_mem_dump(void *m);
extern void n_dda_setvec(void *d, void *f);
extern void n_txm_setvec(void *t, void *f);
extern void n_mif_setvec(void *m, void *f);
extern void n_dda_dtor(void *d, int inchrg);
extern void n_txm_dtor(void *t, int inchrg);
extern void n_mif_dtor(void *m, int inchrg);
extern void *n_dda_ctor(void *d, void *txm);
extern void *n_txm_ctor(void *t, void *memif);
extern void *n_mif_ctor(void *m, void *mem);

extern void o_dda_regvec(void *, void *);
extern void o_dda_trivec(void *, void *);
extern void o_txm_regvec(void *, void *);
extern void o_txm_primvec(void *, void *);
extern void o_mif_regvec(void *, void *);
extern void o_mif_primvec(void *, void *);
extern void n_dda_regvec(void *, void *);
extern void n_dda_trivec(void *, void *);
extern void n_txm_regvec(void *, void *);
extern void n_txm_primvec(void *, void *);
extern void n_mif_regvec(void *, void *);
extern void n_mif_primvec(void *, void *);

/* ---- bookkeeping ---------------------------------------------------- */

static int side;			/* 0 = old, 1 = new */
static long nchecks, nfail;
static jmp_buf escape;
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

/* ---- event log ------------------------------------------------------ */

#define LOGCAP	(1 << 21)
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

/* ---- the recording allocator ---------------------------------------- */

#define POOLCAP	(72u << 20)
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
	memset(p, 0xa5, size);		/* deterministic garbage */
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
		memcpy(q, p, size);
	return q;
}

void
stub_free(void *p)
{
	logv(5); logv(poolrel(p));
}

/* ---- downstream-stage and constructor stubs ------------------------- */

void *
stub_pre1_ctor(void *p1, void *p3)
{
	logv(6); logv(poolrel(p1)); logv(poolrel(p3));
	memset(p1, 0x5a, 0xac);
	return p1;			/* g++ 2.7 ctors return this */
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
	logv(12);			/* not expected to be reached */
}

/* the three stage entries the My* overrides forward to */
static int taplog;			/* log the forwarding calls? */

void
stub_dda_put(void *d, void *p)
{
	if (taplog) { logv(20); logv(F(p, PC_SENDTYPE)); }
}

void
stub_txm_put(void *t, void *d)
{
	if (taplog) { logv(21); logv(F(d, D_ISREG)); }
}

void
stub_memif_stamp(void *m, void *s)
{
	if (taplog) { logv(22); logv(F(s, S_TYPE)); }
}

void
stub_purevirtual(void)
{
	printf("__pure_virtual reached\n");
	exit(1);
}

/* ---- stderr / exit / assert ----------------------------------------- */

char fake_stderr[256];			/* the era _IO_stderr_ object */

/* fprintf is shared between the vector files (real output, the point of
 * the whole object) and the constructor's stderr error message. */
int
my_fprintf(void *f, const char *fmt, ...)
{
	va_list ap;
	int r, i;

	if (f == (void *)fake_stderr) {
		unsigned int a0;

		va_start(ap, fmt);
		a0 = va_arg(ap, unsigned int);
		va_end(ap);
		logv(13);
		for (i = 0; fmt[i]; i++)
			logv((unsigned char)fmt[i]);
		logv(a0);
		return 0;
	}
	va_start(ap, fmt);
	r = vfprintf((FILE *)f, fmt, ap);
	va_end(ap);
	return r;
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

/* ---- fake vtables (g++ 2.7 non-thunk format) ------------------------ */

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

struct vtable fake_vt_dda;
struct vtable fake_vt_txm;
struct vtable fake_vt_pre3;
struct vtable fake_vt_pcalc;
struct vtable fake_vt_memif;
struct vtable fake_vt_xwindow;
struct vtable fake_vt_pcrtcxif;

/* ---- comparing the object graph -------------------------------------- */

static int
inimage(unsigned int v)
{
	return v - 0x08048000u < 0x00800000u;
}

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

/* ---- comparing two tap files ----------------------------------------- */

static FILE *tap[2];

static void
tapopen(void)
{
	int s;

	for (s = 0; s < 2; s++) {
		tap[s] = tmpfile();
		if (tap[s] == 0) {
			printf("tmpfile failed\n");
			exit(1);
		}
	}
}

/* byte-compare the two tap files, then close them */
static void
tapcmp(const char *what)
{
	static char ba[65536], bb[65536];
	long off = 0;
	int s;

	for (s = 0; s < 2; s++)
		if (fflush(tap[s]) != 0 || fseek(tap[s], 0, SEEK_SET) != 0) {
			printf("FAIL %s: tap rewind\n", what);
			nfail++;
			return;
		}
	for (;;) {
		size_t na = fread(ba, 1, sizeof ba, tap[0]);
		size_t nb = fread(bb, 1, sizeof bb, tap[1]);
		size_t i;

		nchecks++;
		if (na != nb) {
			nfail++;
			printf("FAIL %s: length differs at %ld (%ld vs %ld)\n",
				what, off, (long)na, (long)nb);
			break;
		}
		if (na == 0)
			break;
		if (memcmp(ba, bb, na) != 0) {
			for (i = 0; i < na && ba[i] == bb[i]; i++)
				;
			nfail++;
			printf("FAIL %s: byte %ld: old %02x new %02x\n",
				what, off + (long)i,
				(unsigned char)ba[i], (unsigned char)bb[i]);
			break;
		}
		nchecks += (long)na;
		off += (long)na;
	}
	if (off > 0)
		printf("    %-28s %9ld bytes identical\n", what, off);
	for (s = 0; s < 2; s++) {
		fclose(tap[s]);
		tap[s] = 0;
	}
}

/* ---- xorshift PRNG (identical stream for both sides) ----------------- */

static unsigned int rs;

static unsigned int
rnd(void)
{
	rs ^= rs << 13;
	rs ^= rs >> 17;
	rs ^= rs << 5;
	return rs;
}

static void
fillrnd(void *p, unsigned int n)
{
	unsigned int i;

	for (i = 0; i + 4 <= n; i += 4)
		*(unsigned int *)((char *)p + i) = rnd();
}

/* ---- constructing ---------------------------------------------------- */

static char title[] = "diffgpu2vec";

static void *
construct(void *(*ctor)(void *, char *, int, int, int),
	int w, int h, int disp, int *esc)
{
	void *g = bump(GPU2VEC_SIZE);
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

/* ---- the three stage taps, driven directly --------------------------- */

/* one MyDDA / MyTXM / MyMemIF receiver per side, plus the source object */
static void *rdda[2], *rtxm[2], *rmif[2];
static void *sPCalc[2], *sDDA[2], *sStamp[2];

static void
ddatap(int iters)
{
	int i, s;

	for (i = 0; i < iters; i++) {
		unsigned int seed = 0x1234567u + i * 2654435761u;

		for (s = 0; s < 2; s++) {
			void *p = sPCalc[s];

			rs = seed | 1;
			fillrnd(p, PCALC_SIZE);
			F(p, PC_SENDTYPE) = (i >> 0) & 1;
			F(p, PC_AA1)      = (i >> 1) & 1;
			F(p, PC_TYPE)     = (i >> 2) & 3;
			F(p, PC_FGE)      = (i >> 4) & 1;
			F(p, PC_TME)      = (i >> 5) & 3;
			F(p, PC_BF4)      = (i >> 7) & 3;
			F(p, PC_ABE)      = (i >> 9) & 1;
			F(p, PC_FST)      = (i >> 10) & 1;
			F(p, PC_CTXT)     = (i >> 11) & 1;
			F(p, PC_SCANMSK)  = (i >> 3) & 3;
			side = s;
			if (s == 0)
				o_dda_put(rdda[0], p);
			else
				n_dda_put(rdda[1], p);
		}
	}
}

static void
txmtap(int iters)
{
	int i, s;

	for (i = 0; i < iters; i++) {
		unsigned int seed = 0x89abcdefu + i * 2246822519u;

		for (s = 0; s < 2; s++) {
			void *d = sDDA[s];

			rs = seed | 1;
			fillrnd(d, DDA_SIZE);
			F(d, D_ISREG) = (i >> 0) & 1;
			F(d, D_TME)   = (i >> 1) & 1;
			F(d, D_FGE)   = (i >> 2) & 1;
			F(d, D_AA1)   = (i >> 3) & 1;
			F(d, D_FIRST) = (i >> 4) & 1;
			F(d, D_YDIR)  = (i >> 5) & 1;
			F(d, D_ABE)   = (i >> 6) & 1;
			F(d, D_FST)   = (i >> 7) & 1;
			F(d, D_CTXT)  = (i >> 8) & 1;
			side = s;
			if (s == 0)
				o_txm_put(rtxm[0], d);
			else
				n_txm_put(rtxm[1], d);
		}
	}
}

static void
miftap(int iters)
{
	int i, s;

	for (i = 0; i < iters; i++) {
		unsigned int seed = 0x0f0f1234u + i * 3266489917u;

		for (s = 0; s < 2; s++) {
			void *p = sStamp[s];

			rs = seed | 1;
			fillrnd(p, PIXELSTAMP_SIZE);
			F(p, S_TYPE) = (i >> 0) & 1;
			/* reg 0 and 0x1b are the two early-out arms */
			F(p, S_REG) = (i & 4) ? ((i & 8) ? 0 : 0x1b)
					      : (i & 0x7ff);
			F(p, S_M30) = (i >> 1) & 1;
			F(p, S_M34) = (i >> 2) & 1;
			F(p, S_M40) = (i >> 3) & 1;
			F(p, S_MASK) = (i * 7) & 0xffff;
			F(p, S_AAMASK) = (i * 13) & 0xfff;
			F(p, S_ABE) = (i >> 5) & 1;
			F(p, S_CTXT) = (i >> 6) & 1;
			side = s;
			if (s == 0)
				o_mif_stamp(rmif[0], p);
			else
				n_mif_stamp(rmif[1], p);
		}
	}
}

int
main(void)
{
	void *g[2];
	int esc[2];
	int i, s, k;
	long long v[2];
	unsigned int u[2];

	setvbuf(stdout, 0, _IOLBF, 0);
	pool[0] = malloc(POOLCAP);
	pool[1] = malloc(POOLCAP);
	if (!pool[0] || !pool[1]) {
		printf("no pool\n");
		return 1;
	}
	fake_vt_pcrtcxif.e[0].fn = (void *)rec_xif_setreg;
	fake_vt_pcrtcxif.e[1].fn = (void *)rec_xif_resize;

	/* ================= constructor, disp_on = 0 ================= */
	taplog = 1;
	side = 0; g[0] = construct(o_ctor, 640, 480, 0, &esc[0]);
	side = 1; g[1] = construct(n_ctor, 640, 480, 0, &esc[1]);
	check("disp0 escape", esc[0], esc[1]);
	logcmp();
	cmpgraph();

	/* the wiring, read out of the constructed graph */
	for (s = 0; s < 2; s++) {
		void *pp = FP(g[s], V_PP);

		check("mem  nonnull", (long)(FP(g[s], V_MEM) != 0), 1);
		check("memif->mem", (long)(FP(FP(g[s], V_MEMIF), 0)
			== FP(g[s], V_MEM)), 1);
		check("txm->memif", (long)(FP(FP(g[s], V_TXM), 0x24)
			== FP(g[s], V_MEMIF)), 1);
		check("dda->txm", (long)(FP(FP(g[s], V_DDA), 0x04)
			== FP(g[s], V_TXM)), 1);
		check("ppout->dda", (long)(FP(FP(pp, 0x0c), 0x04)
			== FP(g[s], V_DDA)), 1);
		check("pcalc->ppout", (long)(FP(FP(pp, 0x08), 0)
			== FP(pp, 0x0c)), 1);
		check("pre3->pcalc", (long)(FP(FP(pp, 0x04), 0)
			== FP(pp, 0x08)), 1);
		check("taps clear", F(pp, PP_FP)
			| F(FP(g[s], V_DDA), DDA_FP)
			| F(FP(g[s], V_TXM), TXM_FP)
			| F(FP(g[s], V_MEMIF), MEMIF_FP)
			| F(FP(g[s], V_MEM), MEMORY_FP), 0);
	}

	/* ---- Put over the whole address space (taps off) ---- */
	for (i = -0x80; i < 0x110; i++) {
		long long d = ((long long)(0xabcd0000 + i) << 32)
			| (0x1234u + i);

		side = 0; s = o_put(g[0], i, d);
		side = 1; k = n_put(g[1], i, d);
		check("put retval", s, k);
		logcmp();
	}
	/* the PCRTCdmy decode state */
	side = 0; o_put(g[0], 0x8b, 0x123456789abcdefLL);
	side = 1; n_put(g[1], 0x8b, 0x123456789abcdefLL);
	side = 0; o_put(g[0], 0x8c, 0x0fedcba987654321LL);
	side = 1; n_put(g[1], 0x8c, 0x0fedcba987654321LL);
	logcmp();
	for (i = 0; i < PCRTCDMY_SIZE; i += 4) {
		unsigned int wa = F(FP(g[0], V_PCRTC), i);
		unsigned int wb = F(FP(g[1], V_PCRTC), i);
		unsigned int ta = wa - (unsigned int)(unsigned long)pool[0];
		unsigned int tb = wb - (unsigned int)(unsigned long)pool[1];

		if (ta < POOLCAP && tb < POOLCAP) {
			wa = ta;
			wb = tb;
		} else if (inimage(wa) && inimage(wb))
			wa = wb = 0;
		check("pcrtcdmy word", wa, wb);
	}

	/* ---- Get / ResizeWindow ---- */
	side = 0; v[0] = o_get(g[0]);
	side = 1; v[1] = n_get(g[1]);
	check("get lo", (long)(unsigned int)v[0], (long)(unsigned int)v[1]);
	check("get hi", (long)(unsigned int)(v[0] >> 32),
		(long)(unsigned int)(v[1] >> 32));
	logcmp();
	side = 0; o_resizewindow(g[0], 512, 384);
	side = 1; n_resizewindow(g[1], 512, 384);
	logcmp();

	/* ---- SetVector: which slot receives f, for every sel ---- */
	for (k = -1; k < 8; k++) {
		void *f = (void *)(long)(0xb0000000u + k * 0x100);

		side = 0; o_setvector(g[0], k, f);
		side = 1; n_setvector(g[1], k, f);
		for (s = 0; s < 2; s++) {
			void *pp = FP(g[s], V_PP);

			side = s;
			logv((unsigned int)F(g[s], V_VEC));
			logv((unsigned int)F(g[s], V_FP));
			logv((unsigned int)F(pp, PP_FP));
			logv((unsigned int)F(FP(g[s], V_DDA), DDA_FP));
			logv((unsigned int)F(FP(g[s], V_TXM), TXM_FP));
			logv((unsigned int)F(FP(g[s], V_MEMIF), MEMIF_FP));
			logv((unsigned int)F(FP(g[s], V_MEM), MEMORY_FP));
		}
		logcmp();
		/* put the taps back */
		side = 0; o_setvector(g[0], 0, 0);
		side = 1; n_setvector(g[1], 0, 0);
		for (s = 0; s < 2; s++) {
			void *gg = g[s];
			void *pp = FP(gg, V_PP);

			F(pp, PP_FP) = 0;
			F(FP(gg, V_DDA), DDA_FP) = 0;
			F(FP(gg, V_TXM), TXM_FP) = 0;
			F(FP(gg, V_MEMIF), MEMIF_FP) = 0;
			F(FP(gg, V_MEM), MEMORY_FP) = 0;
			F(gg, V_FP) = 0;
		}
	}

	/* =========== the vector files: sel 6, the raw register tap ====== */
	tapopen();
	side = 0; o_setvector(g[0], 6, tap[0]);
	side = 1; n_setvector(g[1], 6, tap[1]);
	for (i = -0x80; i < 0x110; i++) {
		long long d = ((long long)(0x5a5a0000u + i * 7) << 32)
			| (0xc3c30000u + i * 13);

		side = 0; o_put(g[0], i, d);
		side = 1; n_put(g[1], i, d);
	}
	logcmp();
	tapcmp("sel6 GPU2VEC::Put");
	side = 0; o_setvector(g[0], 6, 0);
	side = 1; n_setvector(g[1], 6, 0);
	for (s = 0; s < 2; s++)
		F(g[s], V_FP) = 0;

	/* =========== sel 1: MyPP::Put ================================== */
	tapopen();
	side = 0; o_setvector(g[0], 1, tap[0]);
	side = 1; n_setvector(g[1], 1, tap[1]);
	for (i = -0x100; i < 0x180; i++) {
		long long d = ((long long)(0x11223300u + i * 3) << 32)
			| (0x44556600u + i * 11);

		side = 0; o_mypp_put(FP(g[0], V_PP), i, d);
		side = 1; n_mypp_put(FP(g[1], V_PP), i, d);
	}
	logcmp();
	tapcmp("sel1 MyPP::Put");
	for (s = 0; s < 2; s++)
		F(FP(g[s], V_PP), PP_FP) = 0;

	/* =========== sel 5: MyMemory::Dump ============================= */
	tapopen();
	for (s = 0; s < 2; s++) {
		unsigned int *vram = (unsigned int *)FP(g[s], V_MEM);

		rs = 0xfeedbeefu;
		for (i = 0; i < 0x100000; i++)
			vram[i] = rnd();
	}
	side = 0; o_setvector(g[0], 5, tap[0]);
	side = 1; n_setvector(g[1], 5, tap[1]);
	side = 0; o_put(g[0], 0x7f, 0x1122334455667788LL);
	side = 1; n_put(g[1], 0x7f, 0x1122334455667788LL);
	logcmp();
	tapcmp("sel5 MyMemory::Dump");
	for (s = 0; s < 2; s++)
		F(FP(g[s], V_MEM), MEMORY_FP) = 0;
	/* Dump with the tap off must write nothing */
	side = 0; o_mem_dump(FP(g[0], V_MEM));
	side = 1; n_mem_dump(FP(g[1], V_MEM));
	logcmp();

	/* =========== sel 2/3/4: the stage taps ========================== */
	taplog = 1;
	for (s = 0; s < 2; s++) {
		rdda[s] = calloc(1, MYDDA_SIZE);
		rtxm[s] = calloc(1, MYTXM_SIZE);
		rmif[s] = calloc(1, MYMEMIF_SIZE);
		sPCalc[s] = calloc(1, PCALC_SIZE);
		sDDA[s] = calloc(1, DDA_SIZE);
		sStamp[s] = calloc(1, PIXELSTAMP_SIZE);
	}

	/* taps off: nothing may be written, the forward must still happen */
	tapopen();
	ddatap(8); txmtap(8); miftap(8);
	logcmp();
	tapcmp("taps off (empty)");

	tapopen();
	side = 0; o_dda_setvec(rdda[0], tap[0]);
	side = 1; n_dda_setvec(rdda[1], tap[1]);
	check("dda setvec", F(rdda[0], DDA_FP) != 0, F(rdda[1], DDA_FP) != 0);
	ddatap(4096);
	logcmp();
	tapcmp("sel2 MyDDA::Put");

	tapopen();
	side = 0; o_txm_setvec(rtxm[0], tap[0]);
	side = 1; n_txm_setvec(rtxm[1], tap[1]);
	txmtap(4096);
	logcmp();
	tapcmp("sel3 MyTXM::Put");

	tapopen();
	side = 0; o_mif_setvec(rmif[0], tap[0]);
	side = 1; n_mif_setvec(rmif[1], tap[1]);
	miftap(4096);
	logcmp();
	tapcmp("sel4 MyMemIF::Stamp");

	/* the RegisterVec / *Vec entries called directly, both arms, so a
	 * routing change in Put/Stamp cannot mask a formatting change */
	tapopen();
	side = 0; o_dda_setvec(rdda[0], tap[0]);
	side = 1; n_dda_setvec(rdda[1], tap[1]);
	for (i = 0; i < 512; i++)
		for (s = 0; s < 2; s++) {
			void *p = sPCalc[s];

			rs = 0x77777777u + i * 40503u;
			fillrnd(p, PCALC_SIZE);
			F(p, PC_AA1)  = (i >> 0) & 1;
			F(p, PC_TYPE) = (i >> 1) & 3;
			F(p, PC_FGE)  = (i >> 3) & 1;
			F(p, PC_TME)  = (i >> 4) & 3;
			F(p, PC_BF4)  = (i >> 6) & 3;
			side = s;
			if (s == 0) {
				o_dda_regvec(rdda[0], p);
				o_dda_trivec(rdda[0], p);
			} else {
				n_dda_regvec(rdda[1], p);
				n_dda_trivec(rdda[1], p);
			}
		}
	tapcmp("MyDDA::*Vec direct");

	tapopen();
	side = 0; o_txm_setvec(rtxm[0], tap[0]);
	side = 1; n_txm_setvec(rtxm[1], tap[1]);
	for (i = 0; i < 512; i++)
		for (s = 0; s < 2; s++) {
			void *d = sDDA[s];

			rs = 0x2468ace0u + i * 69621u;
			fillrnd(d, DDA_SIZE);
			F(d, D_TME) = (i >> 0) & 1;
			F(d, D_FGE) = (i >> 1) & 1;
			F(d, D_AA1) = (i >> 2) & 1;
			side = s;
			if (s == 0) {
				o_txm_regvec(rtxm[0], d);
				o_txm_primvec(rtxm[0], d);
			} else {
				n_txm_regvec(rtxm[1], d);
				n_txm_primvec(rtxm[1], d);
			}
		}
	tapcmp("MyTXM::*Vec direct");

	tapopen();
	side = 0; o_mif_setvec(rmif[0], tap[0]);
	side = 1; n_mif_setvec(rmif[1], tap[1]);
	for (i = 0; i < 512; i++)
		for (s = 0; s < 2; s++) {
			void *p = sStamp[s];

			rs = 0x13579bdfu + i * 26237u;
			fillrnd(p, PIXELSTAMP_SIZE);
			F(p, S_REG) = (i & 3) == 0 ? 0
				    : (i & 3) == 1 ? 0x1b : (i & 0x7ff);
			F(p, S_M30) = (i >> 2) & 1;
			F(p, S_M34) = (i >> 3) & 1;
			F(p, S_M40) = (i >> 4) & 1;
			F(p, S_MASK) = (i * 29) & 0xffff;
			F(p, S_AAMASK) = (i * 37) & 0xfff;
			side = s;
			if (s == 0) {
				o_mif_regvec(rmif[0], p);
				o_mif_primvec(rmif[0], p);
			} else {
				n_mif_regvec(rmif[1], p);
				n_mif_primvec(rmif[1], p);
			}
		}
	tapcmp("MyMemIF::*Vec direct");

	/* ---- the destructors: fclose on a live tap ---- */
	{
		FILE *f[2];

		for (s = 0; s < 2; s++)
			f[s] = tmpfile();
		side = 0; o_dda_setvec(rdda[0], f[0]);
		side = 1; n_dda_setvec(rdda[1], f[1]);
		side = 0; o_dda_dtor(rdda[0], 0);
		side = 1; n_dda_dtor(rdda[1], 0);
		logcmp();
		for (s = 0; s < 2; s++)
			f[s] = tmpfile();
		side = 0; o_txm_setvec(rtxm[0], f[0]);
		side = 1; n_txm_setvec(rtxm[1], f[1]);
		side = 0; o_txm_dtor(rtxm[0], 0);
		side = 1; n_txm_dtor(rtxm[1], 0);
		logcmp();
		for (s = 0; s < 2; s++)
			f[s] = tmpfile();
		side = 0; o_mif_setvec(rmif[0], f[0]);
		side = 1; n_mif_setvec(rmif[1], f[1]);
		side = 0; o_mif_dtor(rmif[0], 0);
		side = 1; n_mif_dtor(rmif[1], 0);
		logcmp();
		/* and with no tap installed: no fclose */
		side = 0; o_dda_setvec(rdda[0], 0);
		side = 1; n_dda_setvec(rdda[1], 0);
		side = 0; o_dda_dtor(rdda[0], 0);
		side = 1; n_dda_dtor(rdda[1], 0);
		logcmp();
	}

	/* ---- the stage constructors ---- */
	for (s = 0; s < 2; s++) {
		memset(rdda[s], 0x5c, MYDDA_SIZE);
		memset(rtxm[s], 0x5c, MYTXM_SIZE);
		memset(rmif[s], 0x5c, MYMEMIF_SIZE);
	}
	side = 0;
	o_dda_ctor(rdda[0], (void *)0x1000);
	o_txm_ctor(rtxm[0], (void *)0x2000);
	o_mif_ctor(rmif[0], (void *)0x3000);
	side = 1;
	n_dda_ctor(rdda[1], (void *)0x1000);
	n_txm_ctor(rtxm[1], (void *)0x2000);
	n_mif_ctor(rmif[1], (void *)0x3000);
	logcmp();
	for (i = 0; i < MYDDA_SIZE; i += 4) {
		unsigned int a = F(rdda[0], i), b = F(rdda[1], i);

		if (inimage(a) && inimage(b))
			a = b = 0;
		check("MyDDA ctor", a, b);
	}
	for (i = 0; i < MYTXM_SIZE; i += 4) {
		unsigned int a = F(rtxm[0], i), b = F(rtxm[1], i);

		if (inimage(a) && inimage(b))
			a = b = 0;
		check("MyTXM ctor", a, b);
	}
	for (i = 0; i < MYMEMIF_SIZE; i += 4) {
		unsigned int a = F(rmif[0], i), b = F(rmif[1], i);

		if (inimage(a) && inimage(b))
			a = b = 0;
		check("MyMemIF ctor", a, b);
	}

	/* =========== disp_on = 1: OpenWindow + the PCRTCxif vtable ====== */
	resetpool();
	side = 0; g[0] = construct(o_ctor, 640, 224, 1, &esc[0]);
	side = 1; g[1] = construct(n_ctor, 640, 224, 1, &esc[1]);
	check("disp1 escape", esc[0], esc[1]);
	logcmp();
	cmpgraph();
	for (i = -0x80; i < 0x110; i++) {
		long long d = ((long long)(0x55aa0000 + i) << 32)
			| (0x9876u + i);

		side = 0; o_put(g[0], i, d);
		side = 1; n_put(g[1], i, d);
		logcmp();
	}
	side = 0; o_resizewindow(g[0], 320, 240);
	side = 1; n_resizewindow(g[1], 320, 240);
	logcmp();

	/* =========== disp_on = 2: XWindowDump + dumpCRT + GetCRT ======== */
	resetpool();
	side = 0; g[0] = construct(o_ctor, 0x40, 0x30, 2, &esc[0]);
	side = 1; g[1] = construct(n_ctor, 0x40, 0x30, 2, &esc[1]);
	check("disp2 escape", esc[0], esc[1]);
	logcmp();
	cmpgraph();
	{
		void *xd[2];

		for (s = 0; s < 2; s++)
			xd[s] = FP((char *)FP(g[s], V_PCRTC), 0x1d0);
		for (k = 0; k < 2; k++) {
			for (s = 0; s < 2; s++) {
				struct vtable *vt = *(struct vtable **)xd[s];
				void (*prep)(void *, int, int) =
					(void (*)(void *, int, int))vt->e[1].fn;
				void (*draw5)(void *, int, int, int, int, int) =
					(void (*)(void *, int, int, int, int,
						int))vt->e[2].fn;
				void (*disp)(void *, int, int, int, int) =
					(void (*)(void *, int, int, int, int))
					vt->e[4].fn;
				void (*clear)(void *) =
					(void (*)(void *))vt->e[6].fn;
				void (*setbg)(void *, int, int, int) =
					(void (*)(void *, int, int, int))
					vt->e[7].fn;
				int x, y;

				side = s;
				(*setbg)(xd[s], 0x20 + k, 0x40, 0x87);
				(*clear)(xd[s]);
				(*prep)(xd[s], 0x40, 0x30);
				for (y = 0; y < 0x30; y += 2)
					for (x = 0; x < 0x40; x += 3)
						(*draw5)(xd[s], x, y,
							x ^ y, x + k, y);
				(*disp)(xd[s], 0, 0, 0x40, 0x30);
			}
			logcmp();
			for (i = 0; i < 0x40 * 0x30 + 16; i++) {
				side = 0; u[0] = o_getcrt(g[0]);
				side = 1; u[1] = n_getcrt(g[1]);
				check("getcrt", u[0], u[1]);
			}
			logcmp();
		}
	}

	/* =========== disp_on = 3: the fprintf/exit arm ================== */
	resetpool();
	side = 0; g[0] = construct(o_ctor, 640, 480, 3, &esc[0]);
	side = 1; g[1] = construct(n_ctor, 640, 480, 3, &esc[1]);
	check("disp3 escaped via exit", esc[0], 1);
	check("disp3 escape", esc[0], esc[1]);
	logcmp();

	/* =========== the width<=0 assert arm ============================ */
	resetpool();
	side = 0; g[0] = construct(o_ctor, 0, 16, 2, &esc[0]);
	side = 1; g[1] = construct(n_ctor, 0, 16, 2, &esc[1]);
	check("assert escaped", esc[0], 2);
	check("assert escape", esc[0], esc[1]);
	logcmp();

	printf("%ld checks, %ld failures\n", nchecks, nfail);
	return nfail != 0;
}
