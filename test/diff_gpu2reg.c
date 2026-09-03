/* Differential test: reconstructed gpu2reg.o + drawprim.o against the
 * 1998 objects.
 *
 * Both sides get a fake GPU2Reg receiver (hand-built g++ 2.7 non-thunk
 * vtable: 8 zero bytes then {short delta; short pad; fn}) that RECORDS
 * every virtual Put(addr, data) into a stream and answers Get()/GetCRT()
 * from a deterministic PRNG.  The host-framework seam (tcl_ip,
 * RegisterCommands, OpenImageFile, SaveImageFile, FreeImage,
 * ConvImage24to8, ConvImage8to24, grfwSwitch*) is stubbed with
 * deterministic fakes that also append records to the stream, so the
 * image upload/save handlers are compared end to end: every register
 * write, every host call and its arguments, every byte of a saved
 * image, and the Tcl result string.
 *
 * The 82 command handlers are static in gpu2reg.o; they are reached
 * through each side's MyCBFuncs table (renamed o_/n_ by
 * test/run_gpu2reg.sh), which is itself compared entry by entry.
 * drawprim's Vertex0/1/2 / DrawLine / DrawTriangle are exercised over
 * all flag combinations with randomised vertices.  Quit calls exit()
 * and is checked in a child process.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct {
	int type;
	union {
		int i;
		float f;
		char *s;
	} d;
} jtcl_data_t;

typedef struct {
	char *name;
	int (*func)(int, char *, jtcl_data_t *);
	char *help;
	char *pad;
} jtcl_cmd_t;

typedef struct {
	char name[512];
	int type;
	int format;
	int width;
	int height;
	int ncolor;
	unsigned char *r, *g, *b;
	unsigned char *pixel;
} ImageData;

typedef struct {
	float x, y;
	double z;
	float r, g, b, a;
	float s, t, q, f;
} grfwVertex;

/* renamed 1998 / reconstructed symbols */
extern jtcl_cmd_t o_MyCBFuncs[], n_MyCBFuncs[];
extern void *o_pGPU2Reg, *n_pGPU2Reg;
extern void *o_ctor(void *self);
extern void *n_ctor(void *self);
extern void o_Vertex0(int, int, grfwVertex *);
extern void o_Vertex1(int, int, grfwVertex *);
extern void o_Vertex2(int, int, grfwVertex *);
extern void o_DrawLine(int, grfwVertex *, grfwVertex *);
extern void o_DrawTriangle(int, grfwVertex *, grfwVertex *, grfwVertex *);
extern void n_Vertex0(int, int, grfwVertex *);
extern void n_Vertex1(int, int, grfwVertex *);
extern void n_Vertex2(int, int, grfwVertex *);
extern void n_DrawLine(int, grfwVertex *, grfwVertex *);
extern void n_DrawTriangle(int, grfwVertex *, grfwVertex *, grfwVertex *);

/* C++ runtime the objects need.  new char[] gets 8 bytes of slack:
 * SaveRGB24Pixel rounds its Local->Host transfer up to whole quadwords
 * and overruns its w*h*3 buffer by up to 7 bytes, and SaveRGBA32Pixel
 * with odd w*h saves 4 never-written bytes (n/2 rounds down) - original
 * bugs (doc/notes/gpu2reg.md); zeroed allocations keep the comparison
 * deterministic. */
void *__builtin_vec_new(unsigned int n) { return calloc(n + 8, 1); }
void *__builtin_new(unsigned int n) { return calloc(n + 8, 1); }
void __builtin_delete(void *p) { free(p); }
void __builtin_vec_delete(void *p) { free(p); }
void __pure_virtual(void) { fprintf(stderr, "pure virtual called\n"); abort(); }

/* --- the recording stream ------------------------------------------- */

#define MAXREC 65536
struct rec {
	int tag;		/* 'P'ut, 'C'onv24to8, 'V'conv8to24, 'F'ree, 'S'ave, 'O'pen */
	int a;
	unsigned long long d;
};
static struct rec stream[2][MAXREC];
static int nrec[2];
static int side;

static void
put_rec(int tag, int a, unsigned long long d)
{
	if (nrec[side] < MAXREC) {
		stream[side][nrec[side]].tag = tag;
		stream[side][nrec[side]].a = a;
		stream[side][nrec[side]].d = d;
	}
	nrec[side]++;
}

/* --- deterministic PRNG (shared; reset per side) --------------------- */

static unsigned int seed;

static unsigned int
rnd(void)
{
	seed = seed*1103515245 + 12345;
	return seed >> 8;
}

/* --- the fake GPU2Reg ------------------------------------------------ */

struct vt_entry {
	short delta;
	short pad;
	void *fn;
};
struct vtable {
	int zero0, zero1;
	struct vt_entry e[3];
};

static void
fake_put(void *self, int addr, unsigned long long data)
{
	(void)self;
	put_rec('P', addr, data);
}

static unsigned long long
fake_get(void *self)
{
	unsigned long long v;

	(void)self;
	v = rnd();
	v = v << 32 | rnd();
	return v;
}

static int
fake_getcrt(void *self)
{
	(void)self;
	return (int)rnd();
}

static struct vtable fake_vt;
static struct { void *vptr; } fake_obj[2];

/* --- host framework stubs ------------------------------------------- */

static char tcl_result[2][4096];
struct interp { char *result; };
static struct interp interp_s;
struct interp *tcl_ip = &interp_s;

static jtcl_cmd_t *registered[2];
void
RegisterCommands(jtcl_cmd_t *tbl)
{
	registered[side] = tbl;
}

static void *sw_tri[2], *sw_v0[2], *sw_v1[2], *sw_v2[2];
void
grfwSwitchTriangleRasterlizer(void *fn)
{
	sw_tri[side] = fn;
}
void
grfwSwitchVertex(void *f0, void *f1, void *f2)
{
	sw_v0[side] = f0;
	sw_v1[side] = f1;
	sw_v2[side] = f2;
}

/* Image scenarios: 0 = open fails, 1..3 = format 1..3 */
static int img_scenario;
static int img_dim;		/* width/height base */

static void
fill_image(ImageData *img, int format)
{
	int i, n;

	img->type = 2;
	img->format = format;
	img->width = 1 + img_dim % 7;
	img->height = 1 + (img_dim/3) % 5;
	img->ncolor = 0;
	n = img->width * img->height * 4 + 16;
	img->pixel = malloc(n);
	for (i = 0; i < n; i++)
		img->pixel[i] = rnd();
	img->r = malloc(256);
	img->g = malloc(256);
	img->b = malloc(256);
	for (i = 0; i < 256; i++) {
		img->r[i] = rnd();
		img->g[i] = rnd();
		img->b[i] = rnd();
	}
}

int
OpenImageFile(ImageData *img)
{
	unsigned int h = 0;
	char *p;

	for (p = img->name; *p; p++)
		h = h*31 + (unsigned char)*p;
	put_rec('O', (int)h, 0);
	if (img_scenario == 0)
		return 0;
	fill_image(img, img_scenario);
	return 1;
}

int
SaveImageFile(ImageData *img)
{
	unsigned int h = 0;
	int i, n;
	char *p;

	for (p = img->name; *p; p++)
		h = h*31 + (unsigned char)*p;
	h = h*31 + img->type;
	h = h*31 + img->format;
	h = h*31 + img->width;
	h = h*31 + img->height;
	n = img->width * img->height *
	    (img->format == 1 ? 3 : 4);
	for (i = 0; i < n; i++)
		h = h*31 + img->pixel[i];
	put_rec('S', (int)h, (unsigned int)n);
	return 1;
}

void
FreeImage(ImageData *img)
{
	put_rec('F', img->format, (unsigned int)(img->width*img->height));
	/* leak the buffers: the handlers reuse the struct after this */
}

void
ConvImage24to8(ImageData *img, ImageData *idx, int ncolor, int mode)
{
	put_rec('C', ncolor*16 + mode, (unsigned int)(img->width*img->height));
	memcpy(idx->name, img->name, 512);
	fill_image(idx, 3);
	idx->ncolor = ncolor;
}

void
ConvImage8to24(ImageData *idx, ImageData *img)
{
	put_rec('V', idx->format, 0);
	memcpy(img, idx, sizeof(*img));
	fill_image(img, 1);
}

/* --- driving the handlers ------------------------------------------- */

static char *strargs[4];
static char scratchdir[256];

/* argument type spec per command: I int, F float, S string.
 * DISPLAY and SaveCRT get extra count variants below. */
static struct spec {
	const char *name;
	const char *args;
} specs[] = {
	{ "gpuprim",		"IIIIIIIII" },
	{ "gpuxyzf",		"IIII" },
	{ "gpuxyzf2",		"IIII" },
	{ "gpuxyzf3",		"IIII" },
	{ "gpuxyz2",		"III" },
	{ "gpuxyz3",		"III" },
	{ "gpurgbaq",		"IIIIF" },
	{ "gpurgbaq2",		"IIIIF" },
	{ "gpust",		"FF" },
	{ "gpust2",		"FF" },
	{ "gpuuv",		"II" },
	{ "gpuuv2",		"II" },
	{ "gpuxyoffset1",	"II" },
	{ "gpuxyoffset2",	"II" },
	{ "gpuprmodecont",	"I" },
	{ "gpuprmode",		"IIIIIIII" },
	{ "gpuscanmsk",		"I" },
	{ "gputex01",		"IIIIIIIIIIII" },
	{ "gputex02",		"IIIIIIIIIIII" },
	{ "gputexclut",		"III" },
	{ "gputex11",		"IIIIIII" },
	{ "gputex12",		"IIIIIII" },
	{ "gputex21",		"IIIIII" },
	{ "gputex22",		"IIIIII" },
	{ "gpumiptbp11",	"IIIIII" },
	{ "gpumiptbp12",	"IIIIII" },
	{ "gpumiptbp21",	"IIIIII" },
	{ "gpumiptbp22",	"IIIIII" },
	{ "gputexa",		"III" },
	{ "gpuclamp1",		"IIIIII" },
	{ "gpuclamp2",		"IIIIII" },
	{ "gpufogcol",		"III" },
	{ "gpucacheinvld",	"" },
	{ "gpuscissor1",	"IIII" },
	{ "gpuscissor2",	"IIII" },
	{ "gputest1",		"IIIIIIII" },
	{ "gputest2",		"IIIIIIII" },
	{ "gpualpha1",		"IIIII" },
	{ "gpualpha2",		"IIIII" },
	{ "gpupabe",		"I" },
	{ "gpudimx",		"IIIIIIIIIIIIIIII" },
	{ "gpudthe",		"I" },
	{ "gpucolclamp",	"I" },
	{ "gpufba1",		"I" },
	{ "gpufba2",		"I" },
	{ "gpuframe1",		"IIII" },
	{ "gpuframe2",		"IIII" },
	{ "gpuzbuf1",		"III" },
	{ "gpuzbuf2",		"III" },
	{ "gpubitbltbuf",	"IIIIII" },
	{ "gputrxpos",		"IIIII" },
	{ "gputrxreg",		"II" },
	{ "gputrxdir",		"I" },
	{ "gpuhwreg",		"II" },
	{ "gpupmode",		"IIIIIIIIIII" },
	{ "gpusmode1",		"IIIIIIIIIIIIIIIII" },
	{ "gpusmode2",		"III" },
	{ "gpusynch1",		"IIIII" },
	{ "gpusyncv",		"IIIIII" },
	{ "gpubgcolor",		"III" },
	{ "gpudispfb1",		"IIIII" },
	{ "gpudispfb2",		"IIIII" },
	{ "gpudisplay1",	"IIIIII" },
	{ "gpudisplay2",	"IIIIII" },
	{ "gpudisplay",		"III" },
	{ "gpuextbuf",		"IIIIIIII" },
	{ "gpuextdata",		"IIIIII" },
	{ "gpuextwrite",	"" },
	{ "gpupcrtc",		"I" },
	{ "savecrt",		"IISI" },
	{ "gpurgb24pixel",	"S" },
	{ "gpurgba32pixel",	"S" },
	{ "gpurgba16pixel",	"S" },
	{ "gpuclutrgba32pixel",	"SI" },
	{ "gpuclutrgba16pixel",	"SII" },
	{ "gpuidtex8pixel",	"S" },
	{ "gpuidtex4pixel",	"S" },
	{ "savergb24",		"IIIIS" },
	{ "savergba32",		"IIIIS" },
	{ "gpufile",		"S" },
	{ "gpureg",		"III" },
	{ 0, 0 }
};

static jtcl_cmd_t *
find_cmd(jtcl_cmd_t *tbl, const char *name)
{
	int i;

	for (i = 0; tbl[i].name; i++)
		if (strcmp(tbl[i].name, name) == 0)
			return &tbl[i];
	return 0;
}

static int failures;

static void
mismatch(const char *what, const char *cmd, int round)
{
	printf("MISMATCH %s: cmd %s round %d\n", what, cmd, round);
	if (++failures > 20) {
		printf("too many failures, giving up\n");
		exit(1);
	}
}

/* compare the two streams after a call pair */
static int
compare_streams(const char *cmd, int round, int reto, int retn)
{
	int i, bad = 0;

	if (reto != retn) {
		mismatch("return value", cmd, round);
		bad = 1;
	}
	if (nrec[0] != nrec[1]) {
		mismatch("record count", cmd, round);
		bad = 1;
	} else {
		for (i = 0; i < nrec[0] && i < MAXREC; i++)
			if (stream[0][i].tag != stream[1][i].tag ||
			    stream[0][i].a != stream[1][i].a ||
			    stream[0][i].d != stream[1][i].d) {
				mismatch("record", cmd, round);
				printf("  rec %d: o(%c %08x %016llx) "
				    "n(%c %08x %016llx)\n", i,
				    stream[0][i].tag, stream[0][i].a,
				    stream[0][i].d,
				    stream[1][i].tag, stream[1][i].a,
				    stream[1][i].d);
				bad = 1;
				break;
			}
	}
	if (strcmp(tcl_result[0], tcl_result[1]) != 0) {
		mismatch("tcl result", cmd, round);
		bad = 1;
	}
	return bad;
}

static long long put_count;

static int
run_side(int s, jtcl_cmd_t *cmd, int cnt, jtcl_data_t *data, unsigned int sd)
{
	int ret;

	side = s;
	nrec[s] = 0;
	tcl_result[s][0] = '\0';
	interp_s.result = tcl_result[s];
	seed = sd;
	img_dim = sd % 23;
	ret = cmd->func(cnt, (char *)"args", data);
	put_count += nrec[s];
	return ret;
}

static void
run_cmd(const char *name, int cnt, jtcl_data_t *data, int round)
{
	jtcl_cmd_t *oc = find_cmd(o_MyCBFuncs, name);
	jtcl_cmd_t *nc = find_cmd(n_MyCBFuncs, name);
	unsigned int sd = rnd();
	int reto, retn;

	if (!oc || !nc) {
		mismatch("command missing", name, round);
		return;
	}
	reto = run_side(0, oc, cnt, data, sd);
	retn = run_side(1, nc, cnt, data, sd);
	compare_streams(name, round, reto, retn);
}

static int
rndarg(int round)
{
	switch (rnd() % 4) {
	case 0: return rnd() & 0xf;
	case 1: return rnd() & 0xffff;
	case 2: return (int)rnd();
	default: return -(int)(rnd() & 0xffff);
	}
}

int
main(int argc, char **argv)
{
	int rounds = argc > 1 ? atoi(argv[1]) : 300;
	int r, i, j;
	jtcl_data_t data[20];
	char fname[512];
	FILE *fp;

	/* scratch files for gpufile */
	snprintf(scratchdir, sizeof(scratchdir), "%s/lg2test",
	    getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
	snprintf(fname, sizeof(fname), "%s/gpu2file.txt", scratchdir);

	fake_vt.zero0 = fake_vt.zero1 = 0;
	fake_vt.e[0].delta = 0; fake_vt.e[0].fn = (void *)fake_put;
	fake_vt.e[1].delta = 0; fake_vt.e[1].fn = (void *)fake_get;
	fake_vt.e[2].delta = 0; fake_vt.e[2].fn = (void *)fake_getcrt;
	fake_obj[0].vptr = &fake_vt;
	fake_obj[1].vptr = &fake_vt;

	/* --- the constructors: registration + pGPU2Reg ----------------- */
	{
		static char obj[2][16];
		void *ro, *rn;

		side = 0; ro = o_ctor(obj[0]);
		side = 1; rn = n_ctor(obj[1]);
		if (ro != (void *)obj[0] || rn != (void *)obj[1])
			mismatch("ctor return", "ctor", 0);
		if (registered[0] != o_MyCBFuncs || registered[1] != n_MyCBFuncs)
			mismatch("RegisterCommands", "ctor", 0);
		if (o_pGPU2Reg != (void *)obj[0] || n_pGPU2Reg != (void *)obj[1])
			mismatch("pGPU2Reg", "ctor", 0);
		if (!sw_tri[0] || !sw_tri[1] || !sw_v0[0] || !sw_v0[1] ||
		    !sw_v1[0] || !sw_v1[1] || !sw_v2[0] || !sw_v2[1])
			mismatch("grfwSwitch", "ctor", 0);
	}

	/* now aim both sides at the recording fake */
	o_pGPU2Reg = &fake_obj[0];
	n_pGPU2Reg = &fake_obj[1];

	/* --- table comparison ------------------------------------------ */
	for (i = 0; ; i++) {
		jtcl_cmd_t *o = &o_MyCBFuncs[i], *n = &n_MyCBFuncs[i];

		if (!o->name && !n->name) {
			if (o->func || n->func || o->help || n->help)
				mismatch("terminator", "table", i);
			break;
		}
		if (!o->name || !n->name ||
		    strcmp(o->name, n->name) != 0 ||
		    (o->help == 0) != (n->help == 0) ||
		    (o->help && strcmp(o->help, n->help) != 0) ||
		    (o->func == 0) != (n->func == 0)) {
			mismatch("table entry", o->name ? o->name : "?", i);
			break;
		}
	}

	/* --- quit: must exit(0), checked in a child -------------------- */
	{
		pid_t pid = fork();

		if (pid == 0) {
			jtcl_cmd_t *q = find_cmd(o_MyCBFuncs, "quit");
			jtcl_cmd_t *qn = find_cmd(n_MyCBFuncs, "quit");

			if (!q || !qn || q->func == 0 || qn->func == 0)
				_exit(2);
			memset(data, 0, sizeof(data));
			q->func(0, (char *)"", data);	/* exits */
			_exit(3);
		} else {
			int st;

			waitpid(pid, &st, 0);
			if (!WIFEXITED(st) || WEXITSTATUS(st) != 0)
				mismatch("quit exit code", "quit", 0);
		}
		pid = fork();
		if (pid == 0) {
			jtcl_cmd_t *qn = find_cmd(n_MyCBFuncs, "quit");

			memset(data, 0, sizeof(data));
			qn->func(0, (char *)"", data);
			_exit(3);
		} else {
			int st;

			waitpid(pid, &st, 0);
			if (!WIFEXITED(st) || WEXITSTATUS(st) != 0)
				mismatch("quit exit code", "quit-n", 0);
		}
	}

	/* --- gpufile scratch input ------------------------------------- */
	strargs[0] = fname;
	strargs[1] = (char *)"image_a.rgb";
	strargs[2] = (char *)"image_b.rgb";
	strargs[3] = (char *)"/nonexistent/lg2test-missing";

	seed = 20260903;

	for (r = 0; r < rounds; r++) {
		/* fresh gpufile content each round */
		fp = fopen(fname, "w");
		if (fp) {
			int lines = 1 + rnd() % 6;

			for (i = 0; i < lines; i++)
				fprintf(fp, "%x %x %x trailing junk\n",
				    rnd() & 0xff, rnd(), rnd());
			fclose(fp);
		}

		for (i = 0; specs[i].name; i++) {
			const char *a = specs[i].args;
			int cnt = (int)strlen(a);
			int si = 0;

			int is_save = strncmp(specs[i].name, "save", 4) == 0;

			memset(data, 0, sizeof(data));
			for (j = 0; a[j]; j++) {
				data[j].type = a[j];
				switch (a[j]) {
				case 'I':
					/* the save commands allocate w*h
					 * buffers and loop over them: keep
					 * their dimensions sane */
					if (is_save)
						data[j].d.i = 1 + (int)(rnd() % 16);
					else
						data[j].d.i = rndarg(r);
					break;
				case 'F':
					data[j].d.f = (float)((int)(rnd() % 65536) - 32768) / 16.0f;
					break;
				case 'S':
					/* gpufile round-robins the error path */
					if (strcmp(specs[i].name, "gpufile") == 0)
						data[j].d.s = strargs[r % 4 == 3 ? 3 : 0];
					else
						data[j].d.s = strargs[si++ & 1];
					break;
				}
			}
			/* image scenario: cycle fail/1/2/3 */
			img_scenario = (r + i) % 4;

			/* the CLUT handlers branch on ==3 / ==0 exactly,
			 * and need a successful non-ARGB open to reach the
			 * palette loops */
			if (strncmp(specs[i].name, "gpuclutrgba", 11) == 0) {
				data[1].d.i = (r & 1) ? 3 : data[1].d.i;
				data[2].d.i = (r >> 1) & 1;
				if (r % 4 != 3)
					img_scenario = 1 + (r >> 1) % 2 * 2;
			}

			run_cmd(specs[i].name, cnt, data, r);

			/* GPU2File never fclose()s: shed its leaked FILEs so
			 * long runs do not exhaust fds (an original bug,
			 * doc/notes/gpu2reg.md) */
			if (strcmp(specs[i].name, "gpufile") == 0) {
				int fd;

				for (fd = 3; fd < 1024; fd++)
					close(fd);
			}

			/* extra count variants */
			if (strcmp(specs[i].name, "gpudisplay") == 0)
				run_cmd(specs[i].name, 1, data, r);
			if (strcmp(specs[i].name, "savecrt") == 0)
				run_cmd(specs[i].name, 3, data, r);
		}
	}

	/* --- drawprim -------------------------------------------------- */
	{
		grfwVertex v[3];
		int type, flag, k;

		for (r = 0; r < rounds; r++) {
			for (k = 0; k < 3; k++) {
				v[k].x = (float)((int)(rnd() % 0x1000) - 0x800) / 4.0f;
				v[k].y = (float)((int)(rnd() % 0x1000) - 0x800) / 4.0f;
				v[k].z = (double)(rnd() % 0x1000000);
				v[k].r = (float)(rnd() % 256);
				v[k].g = (float)(rnd() % 256);
				v[k].b = (float)(rnd() % 256);
				v[k].a = (float)(rnd() % 256);
				v[k].s = (float)((int)(rnd() % 4096) - 2048) / 1024.0f;
				v[k].t = (float)((int)(rnd() % 4096) - 2048) / 1024.0f;
				v[k].q = (float)((int)(rnd() % 4096) - 2048) / 1024.0f;
				v[k].f = (float)(rnd() % 256);
			}
			type = rnd() % 3;
			flag = rnd() % 16;

			side = 0; nrec[0] = 0; o_Vertex0(type, flag, &v[0]);
			side = 1; nrec[1] = 0; n_Vertex0(type, flag, &v[0]);
			put_count += nrec[0] + nrec[1];
			compare_streams("Vertex0", r, 0, 0);

			side = 0; nrec[0] = 0; o_Vertex1(type, flag, &v[1]);
			side = 1; nrec[1] = 0; n_Vertex1(type, flag, &v[1]);
			put_count += nrec[0] + nrec[1];
			compare_streams("Vertex1", r, 0, 0);

			side = 0; nrec[0] = 0; o_Vertex2(type, flag, &v[2]);
			side = 1; nrec[1] = 0; n_Vertex2(type, flag, &v[2]);
			put_count += nrec[0] + nrec[1];
			compare_streams("Vertex2", r, 0, 0);

			side = 0; nrec[0] = 0;
			o_DrawTriangle(0, &v[0], &v[1], &v[2]);
			o_DrawLine(0, &v[0], &v[1]);
			side = 1; nrec[1] = 0;
			n_DrawTriangle(0, &v[0], &v[1], &v[2]);
			n_DrawLine(0, &v[0], &v[1]);
			compare_streams("DrawTriangle/Line", r, 0, 0);
		}
	}

	printf("diff_gpu2reg: %d rounds, %lld records compared, %d failures\n",
	    (int)(argc > 1 ? atoi(argv[1]) : 300), put_count, failures);
	return failures != 0;
}
