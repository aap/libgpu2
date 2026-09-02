/* diff_libgpu2 - differential test: Sony's 1998 libgpu2.o vs src/libgpu2.c.
 *
 * Both objects are linked into one i386 binary with their eight public
 * entry points renamed (old_* / new_*, see run_libgpu2_diff.sh).  Their
 * three bss globals (gpu2, fb, Field) are *local* symbols, so each object
 * keeps its own private copy and the two implementations cannot see each
 * other's state -- which is exactly what a differential test needs.
 *
 * Everything below GS_* is stubbed here, so both objects call the same
 * fakes: `new'/`delete', the GPU2 constructor, GPU2::Put (recorded into a
 * trace) and GPU2::Get (a deterministic pseudo-random pixel stream).  Each
 * scenario is run twice -- old, then new -- and the two traces, return
 * values, save-area state and GS_SaveImage output files are compared.
 *
 * Not covered: the fopen-failure path, which calls fprintf(_IO_stderr_).
 * `_IO_stderr_' is the glibc 2.0 libio object both 1998-era objects
 * reference; it does not exist in a modern glibc, so it is faked here as
 * storage that is never dereferenced (the tests always open a real file).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int fbp;
	int fbw;
	int psm;
	int posx, posy;
	int width, height;
} FRAME_BUFFER;

/* the era libio object; never dereferenced (see header comment) */
char _IO_stderr_[512];

/* ---- the two implementations under test -------------------------------- */
#define API(m)								\
	m(void, InitSim, (void), ())					\
	m(void, OpenSim, (char *t, int w, int h, int d, int f), (t,w,h,d,f)) \
	m(void, CloseSim, (void), ())					\
	m(void, PutPort, (int a, long long d), (a,d))			\
	m(int,  PutCtlPort, (int a, long long d), (a,d))		\
	m(int,  SaveImage, (char *f), (f))				\
	m(void, SetSaveImageArea, (FRAME_BUFFER *f), (f))		\
	m(void, GetSaveImageArea, (FRAME_BUFFER *f), (f))

#define DECL(ret, name, args, call) \
	extern ret old_##name args; extern ret new_##name args;
API(DECL)

/* ---- recorded side effects --------------------------------------------- */
struct put { int addr; long long data; };

static struct put trace[200000];
static int ntrace;
static char ctor_title[256];
static int ctor_arg[3], nctor, ndelete;

static void
reset(void)
{
	ntrace = nctor = ndelete = 0;
	ctor_title[0] = 0;
	memset(ctor_arg, 0, sizeof ctor_arg);
}

/* ---- stubs the two objects link against -------------------------------- */
void *
__builtin_new(unsigned int n)
{
	return malloc(n ? n : 1);
}

void
__builtin_delete(void *p)
{
	ndelete++;
	free(p);
}

void *
__4GPU2Pciii(void *thisp, char *title, int width, int height, int disp_on)
{
	nctor++;
	snprintf(ctor_title, sizeof ctor_title, "%s", title ? title : "(null)");
	ctor_arg[0] = width;
	ctor_arg[1] = height;
	ctor_arg[2] = disp_on;
	return thisp;			/* ctors return `this' in %eax */
}

void
Put__4GPU2ix(void *thisp, int addr, long long data)
{
	(void)thisp;
	if (ntrace < (int)(sizeof trace / sizeof trace[0])) {
		trace[ntrace].addr = addr;
		trace[ntrace].data = data;
	}
	ntrace++;
}

static unsigned pix_state;

long long
Get__4GPU2(void *thisp)
{
	unsigned lo, hi;

	(void)thisp;
	pix_state ^= pix_state << 13;
	pix_state ^= pix_state >> 17;
	pix_state ^= pix_state << 5;
	lo = pix_state;
	pix_state ^= pix_state << 13;
	pix_state ^= pix_state >> 17;
	pix_state ^= pix_state << 5;
	hi = pix_state;
	return ((long long)hi << 32) | lo;
}

/* ---- comparison bookkeeping -------------------------------------------- */
static int fails, checks;

static struct put saved[200000];
static int nsaved;
static char sctor[256];
static int sarg[3], sctorn, sdeln;

static void
snapshot(void)
{
	int n = ntrace;
	if (n > (int)(sizeof saved / sizeof saved[0]))
		n = sizeof saved / sizeof saved[0];
	memcpy(saved, trace, n * sizeof trace[0]);
	nsaved = ntrace;
	memcpy(sctor, ctor_title, sizeof sctor);
	memcpy(sarg, ctor_arg, sizeof sarg);
	sctorn = nctor;
	sdeln = ndelete;
}

static void
cmp_side_effects(const char *what)
{
	int i, n;

	checks++;
	if (nsaved != ntrace) {
		printf("FAIL %s: %d Put calls old, %d new\n",
		    what, nsaved, ntrace);
		fails++;
		return;
	}
	n = ntrace;
	if (n > (int)(sizeof saved / sizeof saved[0]))
		n = sizeof saved / sizeof saved[0];
	for (i = 0; i < n; i++)
		if (saved[i].addr != trace[i].addr ||
		    saved[i].data != trace[i].data) {
			printf("FAIL %s: Put[%d] old(%#x,%#llx) "
			    "new(%#x,%#llx)\n", what, i, saved[i].addr,
			    saved[i].data, trace[i].addr, trace[i].data);
			fails++;
			return;
		}
	if (sctorn != nctor || sdeln != ndelete ||
	    strcmp(sctor, ctor_title) != 0 ||
	    memcmp(sarg, ctor_arg, sizeof sarg) != 0) {
		printf("FAIL %s: ctor/dtor mismatch (old %d/%d \"%s\" %d,%d,%d "
		    "| new %d/%d \"%s\" %d,%d,%d)\n", what, sctorn, sdeln,
		    sctor, sarg[0], sarg[1], sarg[2], nctor, ndelete,
		    ctor_title, ctor_arg[0], ctor_arg[1], ctor_arg[2]);
		fails++;
	}
}

static void
cmp_fb(const char *what, FRAME_BUFFER *a, FRAME_BUFFER *b)
{
	checks++;
	if (memcmp(a, b, sizeof *a) != 0) {
		printf("FAIL %s: save area old(%d,%d,%d,%d,%d,%d,%d) "
		    "new(%d,%d,%d,%d,%d,%d,%d)\n", what,
		    a->fbp, a->fbw, a->psm, a->posx, a->posy, a->width,
		    a->height, b->fbp, b->fbw, b->psm, b->posx, b->posy,
		    b->width, b->height);
		fails++;
	}
}

static void
cmp_int(const char *what, int a, int b)
{
	checks++;
	if (a != b) {
		printf("FAIL %s: returned %d old, %d new\n", what, a, b);
		fails++;
	}
}

static long
slurp(const char *path, char **out)
{
	FILE *f = fopen(path, "rb");
	long n;

	if (f == NULL)
		return -1;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	*out = malloc(n ? n : 1);
	if (fread(*out, 1, n, f) != (size_t)n)
		n = -1;
	fclose(f);
	return n;
}

static void
cmp_file(const char *what)
{
	char *a = NULL, *b = NULL;
	long na, nb;

	checks++;
	na = slurp("/tmp/lg2diff.old", &a);
	nb = slurp("/tmp/lg2diff.new", &b);
	if (na != nb || na < 0 || memcmp(a, b, na) != 0) {
		printf("FAIL %s: image %ld bytes old, %ld new%s\n", what,
		    na, nb, (na == nb && na > 0) ? " (content differs)" : "");
		fails++;
	}
	free(a);
	free(b);
}

/* ---- scenarios --------------------------------------------------------- */
static unsigned s = 987654321;
static unsigned
rnd(void)
{
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	return s;
}

static long long
rnd64(void)
{
	unsigned lo = rnd(), hi = rnd();
	return ((long long)hi << 32) | lo;
}

int
main(void)
{
	FRAME_BUFFER fa, fb2;
	static const int psms[] = {0, 1, 2, 3, 0x13, 0x30};
	static const int ctl[] = {
		0x12000000, 0x12000010, 0x12000060, 0x12000070, 0x12000080,
		0x12000090, 0x120000a0, 0x120000b0, 0x120000c0, 0x120000d0,
		0x120000c4, 0x11ffffff, 0x12001000, 0x12001010, 0x12001020,
		0x12001040, 0x12001080, 0x12001090, 0x120010f0, 0x120010e0,
		0x12002000, 0x00000000, -1,
	};
	int i, j, k, ra, rb;

	/* 1. InitSim defaults */
	reset(); old_InitSim(); old_GetSaveImageArea(&fa); snapshot();
	reset(); new_InitSim(); new_GetSaveImageArea(&fb2); cmp_side_effects("InitSim");
	cmp_fb("InitSim defaults", &fa, &fb2);
	printf("InitSim defaults: fbp=%d fbw=%d psm=%d pos=%d,%d %dx%d\n",
	    fa.fbp, fa.fbw, fa.psm, fa.posx, fa.posy, fa.width, fa.height);

	/* 2. Set/GetSaveImageArea round trip */
	for (i = 0; i < 2000; i++) {
		FRAME_BUFFER in;
		in.fbp = rnd() % 0x4000;
		in.fbw = rnd() % 64;
		in.psm = psms[rnd() % 6];
		in.posx = rnd() % 2048;
		in.posy = rnd() % 2048;
		in.width = 1 + rnd() % 2048;
		in.height = 1 + rnd() % 2048;
		old_SetSaveImageArea(&in); old_GetSaveImageArea(&fa);
		new_SetSaveImageArea(&in); new_GetSaveImageArea(&fb2);
		cmp_fb("Set/GetSaveImageArea", &fa, &fb2);
	}

	/* 3. OpenSim power-on register pour, per field mode */
	for (i = 0; i < 4; i++) {
		char t[32];
		sprintf(t, "sim%d", i);
		reset(); old_InitSim(); old_OpenSim(t, 640, 480, i & 1, i);
		snapshot();
		reset(); new_InitSim(); new_OpenSim(t, 640, 480, i & 1, i);
		cmp_side_effects("OpenSim");
		reset(); old_CloseSim(); snapshot();
		reset(); new_CloseSim(); cmp_side_effects("CloseSim");
	}
	/* and with a save area that changes initPCRTC's arithmetic */
	for (i = 0; i < 200; i++) {
		FRAME_BUFFER in;
		in.fbp = rnd() % 0x4000;
		in.fbw = rnd() % 64;
		in.psm = psms[rnd() % 6];
		in.posx = rnd() % 2048;
		in.posy = rnd() % 2048;
		in.width = 1 + rnd() % 2048;
		in.height = 1 + rnd() % 2048;
		k = rnd() % 3;
		reset(); old_InitSim(); old_SetSaveImageArea(&in);
		old_OpenSim("t", 320, 240, 0, k); snapshot();
		reset(); new_InitSim(); new_SetSaveImageArea(&in);
		new_OpenSim("t", 320, 240, 0, k);
		cmp_side_effects("OpenSim+initPCRTC");
		old_CloseSim(); new_CloseSim();
	}

	/* 4. PutPort, including the 0x7f -> 0x100 remap */
	for (k = 0; k < 3; k++) {
		reset(); old_InitSim(); old_OpenSim("t", 64, 64, 0, k);
		reset(); new_InitSim(); new_OpenSim("t", 64, 64, 0, k);
		for (i = 0; i < 3000; i++) {
			int addr = (i < 8) ? 0x7f : (int)(rnd() % 0x200);
			long long d = rnd64();
			reset(); old_PutPort(addr, d); snapshot();
			reset(); new_PutPort(addr, d); cmp_side_effects("PutPort");
		}
		old_CloseSim(); new_CloseSim();
	}

	/* 5. PutCtlPort: return value, Put trace and resulting save area */
	reset(); old_InitSim(); old_OpenSim("t", 64, 64, 0, 0);
	reset(); new_InitSim(); new_OpenSim("t", 64, 64, 0, 0);
	for (i = 0; i < (int)(sizeof ctl / sizeof ctl[0]); i++)
		for (j = 0; j < 400; j++) {
			long long d = rnd64();
			reset(); ra = old_PutCtlPort(ctl[i], d); snapshot();
			reset(); rb = new_PutCtlPort(ctl[i], d);
			cmp_side_effects("PutCtlPort");
			cmp_int("PutCtlPort", ra, rb);
			old_GetSaveImageArea(&fa);
			new_GetSaveImageArea(&fb2);
			cmp_fb("PutCtlPort save area", &fa, &fb2);
		}
	old_CloseSim(); new_CloseSim();

	/* 6. SaveImage: BitBLT programming, pixel conversion and file bytes */
	for (i = 0; i < 300; i++) {
		FRAME_BUFFER in;
		in.fbp = rnd() % 0x4000;
		in.fbw = rnd() % 64;
		in.psm = psms[rnd() % 6];
		in.posx = rnd() % 512;
		in.posy = rnd() % 512;
		in.width = 1 + rnd() % 64;
		in.height = 1 + rnd() % 64;
		k = rnd() % 3;

		reset(); old_InitSim(); old_SetSaveImageArea(&in);
		old_OpenSim("t", 64, 64, 0, k);
		reset(); pix_state = 0x1234567 + i;
		ra = old_SaveImage("/tmp/lg2diff.old");
		snapshot();
		old_CloseSim();

		reset(); new_InitSim(); new_SetSaveImageArea(&in);
		new_OpenSim("t", 64, 64, 0, k);
		reset(); pix_state = 0x1234567 + i;
		rb = new_SaveImage("/tmp/lg2diff.new");
		cmp_side_effects("SaveImage");
		cmp_int("SaveImage", ra, rb);
		cmp_file("SaveImage");
		new_CloseSim();
	}

	printf("%d checks, %d failures\n", checks, fails);
	return fails != 0;
}
