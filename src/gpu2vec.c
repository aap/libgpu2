#include "gpu2vec.h"

/* gpu2vec.c - Sony's pipeline-tap / RTL-test-vector layer.
 *
 * Each My* stage override writes one text line per event into the FILE*
 * GPU2VEC::SetVector() installed, then forwards to the real stage code.
 * A line is a sequence of '_'-terminated columns; a column is either a
 * field value in zero-padded hex, a run of Verilog-style 'x' don't-care
 * digits, or (for disabled texture/fog columns at the TXM stage) forced
 * zeros.  Field widths are in *bits*; a w-bit column is (w+3)/4 digits.
 * See doc/notes/gpu2vec.md for every record layout.
 *
 * The six writers below are the whole file-format machine.  Note that
 * PutX counts DIGITS while everything else counts BITS:
 *
 *   Field    one value column: data & mask, "%0<digits>x_"
 *   FieldLL  one long long column: the high word is masked and printed
 *            in (w+3)/4 - 8 digits, the low word in 8
 *   ZField   a value column wired to constant 0
 *   PutX     n 'x' digits and the separator
 *   XField   a w-bit don't-care column
 *   XLine    a whole run of don't-care columns from a width list like
 *            "23 23 30 17 ..." (the same strings a testbench-side reader
 *            would use to split the record)
 *
 * The constant column values (the `en'/`isreg' enables) are written as
 * plain literals: the 1998 object materialises them into a register
 * first, but so does every other constant argument of these inlines -
 * that is the compiler's inline-argument copy, not a source shape.
 */

static inline void
Field(FILE *fp, int data, int mask, int width)
{
	char fmt[128];
	int n = (width + 3)/4;

	strcpy(fmt, "%0");
	sprintf(fmt + 2, "%dx_", n);
	fprintf(fp, fmt, data & mask);
}

static inline void
FieldLL(FILE *fp, long long data, int mask, int width)
{
	char fmt[128];
	int n = (width + 3)/4;

	strcpy(fmt, "%0");
	sprintf(fmt + 2, "%dx", n - 8);
	strcat(fmt, "%08x_");
	fprintf(fp, fmt, (int)(data >> 32) & mask, (int)data);
}

static inline void
ZField(FILE *fp, int width)
{
	char fmt[128];
	int n = (width + 3)/4;

	strcpy(fmt, "%0");
	sprintf(fmt + 2, "%dx_", n);
	fprintf(fp, fmt, 0);
}

static inline void
PutX(FILE *fp, int n)
{
	for (; n != 0; n--)
		fputc('x', fp);
	fputc('_', fp);
}

static inline void
XField(FILE *fp, int width)
{
	int n;

	for (n = (width + 3) >> 2; n != 0; n--)
		fputc('x', fp);
	fputc('_', fp);
}

static inline void
XLine(FILE *fp, char *s)
{
	int n;

	for (;;) {
		if (sscanf(s, "%d", &n) == 0)
			return;
		for (n = (n + 3)/4; n != 0; n--)
			fputc('x', fp);
		fputc('_', fp);
		while (*s != ' ') {
			if (*s == '\0')
				return;
			s++;
		}
		if (*s == '\0')
			return;
		s++;
	}
}

/* The CRT capture: XWindowDump hands every displayed frame to this
 * callback, and GetCRT() plays the pixels back one word per call. */

static unsigned int r_count;
static unsigned int r_size;
static unsigned int *r_buf;

static void
dumpCRT(int width, int height, const unsigned int *buf)
{
	if (r_buf)
		r_buf = (unsigned int *)realloc(r_buf, width*4*height);
	else
		r_buf = (unsigned int *)malloc(width*4*height);
	r_size = width*height;
	r_count = 0;
	memcpy(r_buf, buf, r_size*4);
}

unsigned int
GPU2VEC::GetCRT()
{
	if (r_count < r_size)
		return r_buf[r_count++];
	return 0;
}

GPU2VEC::GPU2VEC(char *title, int width, int height, int disp_on)
{
	mem = new MyMemory;
	memif = new MyMemIF(mem);
	txm = new MyTXM(memif);
	dda = new MyDDA(txm);
	pp = new MyPP(dda);
	switch (disp_on) {
	case 0:
		pcrtc = new PCRTCdmy(mem);
		break;
	case 1:
		pcrtc = new PCRTCxif(mem, title, width, height);
		break;
	case 2:
		pcrtc = new PCRTCxif(mem, title, width, height, dumpCRT);
		break;
	default:
		fprintf(stderr, "invalid argument xdisp -- [%d]\n", disp_on);
		exit(1);
	}
}

long long
GPU2VEC::Get()
{
	Memory *m = mem;

	return m->bitblt.ReadPixel(m);
}

int
GPU2VEC::Put(int addr, long long data)
{
	if (vec == 6)
		fprintf(fp, "%02x %08x %08x\n", addr, (int)(data >> 32),
			(int)data);
	if ((signed char)addr < 0 || addr > 0xff)
		pcrtc->SetRegister(addr, data);
	else {
		if (addr == 0x7f)
			mem->Dump();
		pp->Put(addr, data);
	}
	return 1;
}

void
GPU2VEC::ResizeWindow(int w, int h)
{
	pcrtc->Resize(w, h);
}

void
GPU2VEC::SetVector(int sel, FILE *f)
{
	switch (sel) {
	case 1:
		pp->SetVector(f);
		break;
	case 2:
		dda->SetVector(f);
		break;
	case 3:
		txm->SetVector(f);
		break;
	case 4:
		memif->SetVector(f);
		break;
	case 5:
		mem->SetVector(f);
		break;
	case 6:
		fp = f;
		break;
	}
	vec = sel;
}

void
MyPP::Put(int addr, long long data)
{
	if (fp)
		fprintf(fp, "%02x %08x %08x\n", addr & 0x7f,
			(int)(data >> 32), (int)data);
	pre1->Put(addr, data);
}

/* ---- the DDA-input vector (what PCalc hands the rasterizer) -------- */

void
MyDDA::RegisterVec(PCalc *p)
{
	XLine(fp, "23 23 30 17 17 17 17 17 17 13 13 13 13 13");
	Field(fp, p->ddax, 0x7ff, 11);
	XLine(fp, "11");
	XLine(fp, "19 19 19 19 19 19 19 19 19");
	FieldLL(fp, p->ozv, 0xfff, 44);
	Field(fp, p->ofv, 0xfffff, 20);
	Field(fp, p->oav, 0xfffff, 20);
	Field(fp, p->orv, 0xfffff, 20);
	Field(fp, p->ogv, 0xfffff, 20);
	Field(fp, p->obv, 0xfffff, 20);
	XLine(fp, "28 28 28");
	XLine(fp, "44 20 20 20 20 20 28 28 28");
	XLine(fp, "44 20 20 20 20 20 28 28 28");
	Field(fp, p->TME, 0x1, 1);
	Field(fp, p->FGE, 0x1, 1);
	Field(fp, p->ABE, 0x1, 1);
	XLine(fp, "1");
	Field(fp, p->FST, 0x1, 1);
	Field(fp, p->CTXT, 0x1, 1);
	Field(fp, p->send_type, 0x1, 1);
	XLine(fp, "1 1 1 1 1 1 1");
	Field(fp, p->send_addr, 0x7f, 7);
	FieldLL(fp, p->send_reg, 0xffffffff, 64);
	XLine(fp, "8");
	fprintf(fp, "\n");
}

void
MyDDA::TriangleVec(PCalc *p)
{
	Field(fp, p->m_abc, 0x7fffff, 23);
	Field(fp, p->m_ac0, 0x7fffff, 23);
	Field(fp, p->m_ac4, 0x7fffffff, 31);
	Field(fp, p->ddx[0], 0xffff, 16);
	Field(fp, p->ddx[1], 0x1ffff, 17);
	Field(fp, p->ddx[2], 0x1ffff, 17);
	Field(fp, p->ddy[0], 0x1ffff, 17);
	Field(fp, p->ddy[1], 0xffff, 16);
	Field(fp, p->ddy[2], 0x1ffff, 17);
	Field(fp, p->bbl, 0x3fff, 14);
	Field(fp, p->bbt, 0x3fff, 14);
	Field(fp, p->bbr, 0x3fff, 14);
	Field(fp, p->bbb, 0x3fff, 14);
	Field(fp, p->m_af0, 0x3fff, 14);
	Field(fp, p->ddax, 0x3ff, 10);
	Field(fp, p->dday, 0x3ff, 10);
	if (p->AA1 == 0)
		XLine(fp, "17 17 17 18 18 18 18 18 18");
	else {
		Field(fp, p->covs[0], 0x1ffff, 17);
		Field(fp, p->covs[1], 0x1ffff, 17);
		if (p->type == 1)
			PutX(fp, 5);
		else
			Field(fp, p->covs[2], 0x1ffff, 17);
		Field(fp, p->covdx[0], 0x3ffff, 18);
		Field(fp, p->covdx[1], 0x3ffff, 18);
		if (p->type == 1)
			PutX(fp, 5);
		else
			Field(fp, p->covdx[2], 0x3ffff, 18);
		Field(fp, p->covdy[0], 0x3ffff, 18);
		Field(fp, p->covdy[1], 0x3ffff, 18);
		if (p->type == 1)
			PutX(fp, 5);
		else
			Field(fp, p->covdy[2], 0x3ffff, 18);
	}
	FieldLL(fp, p->ozv, 0x7ff, 43);
	if (p->FGE)
		Field(fp, p->ofv, 0x7ffff, 19);
	else
		PutX(fp, 5);
	Field(fp, p->oav, 0x7ffff, 19);
	Field(fp, p->orv, 0x7ffff, 19);
	Field(fp, p->ogv, 0x7ffff, 19);
	Field(fp, p->obv, 0x7ffff, 19);
	if (p->m_bf4 == 0 && p->TME & 1) {
		Field(fp, p->osv, 0xfffffff, 28);
		Field(fp, p->otv, 0xfffffff, 28);
		Field(fp, p->oqv, 0xfffffff, 28);
	} else
		XLine(fp, "28 28 28");
	FieldLL(fp, p->ozdx, 0xfff, 44);
	if (p->FGE)
		Field(fp, p->ofdx, 0xfffff, 20);
	else
		PutX(fp, 5);
	Field(fp, p->oadx, 0xfffff, 20);
	Field(fp, p->ordx, 0xfffff, 20);
	Field(fp, p->ogdx, 0xfffff, 20);
	Field(fp, p->obdx, 0xfffff, 20);
	if (p->TME && (p->m_bf4 ^ 1) & 1) {
		Field(fp, p->osdx, 0xfffffff, 28);
		Field(fp, p->otdx, 0xfffffff, 28);
		Field(fp, p->oqdx, 0xfffffff, 28);
	} else
		XLine(fp, "28 28 28");
	FieldLL(fp, p->ozdy, 0xfff, 44);
	if (p->FGE)
		Field(fp, p->ofdy, 0xfffff, 20);
	else
		PutX(fp, 5);
	Field(fp, p->oady, 0xfffff, 20);
	Field(fp, p->ordy, 0xfffff, 20);
	Field(fp, p->ogdy, 0xfffff, 20);
	Field(fp, p->obdy, 0xfffff, 20);
	if (p->TME && !(p->m_bf4 & 2)) {
		Field(fp, p->osdy, 0xfffffff, 28);
		Field(fp, p->otdy, 0xfffffff, 28);
		Field(fp, p->oqdy, 0xfffffff, 28);
	} else
		XLine(fp, "28 28 28");
	Field(fp, p->TME, 0x1, 1);
	Field(fp, p->FGE, 0x1, 1);
	Field(fp, p->ABE, 0x1, 1);
	Field(fp, p->AA1, 0x1, 1);
	Field(fp, p->FST, 0x1, 1);
	Field(fp, p->CTXT, 0x1, 1);
	Field(fp, p->send_type, 0x1, 1);
	Field(fp, p->xdir, 0x1, 1);
	Field(fp, p->ydir, 0x1, 1);
	Field(fp, p->SCANMSK, 0x3, 2);
	if (p->AA1 == 0)
		XLine(fp, "2 2 2");
	else {
		Field(fp, p->steep[0], 0x1, 1);
		Field(fp, p->steep[1], 0x1, 1);
		if (p->type == 1)
			PutX(fp, 1);
		else
			Field(fp, p->steep[2], 0x1, 1);
	}
	Field(fp, p->flat, 0x1, 1);
	XLine(fp, "7 64");
	if (p->type == 0 || p->TME == 0)
		PutX(fp, 2);
	else
		Field(fp, p->maxexp, 0xff, 8);
	fprintf(fp, "\n");
}

void
MyDDA::Put(PCalc *p)
{
	if (fp) {
		if (p->send_type)
			RegisterVec(p);
		else
			TriangleVec(p);
	}
	DDA::Put(p);
}

/* ---- the TXM-input vector (what the DDA hands the texture unit) ---- */

void
MyTXM::RegisterVec(DDA *d)
{
	Field(fp, 1, 0x1, 1);
	Field(fp, d->isreg, 0x1, 1);
	PutX(fp, 1);
	Field(fp, d->px, 0x7ff, 11);
	PutX(fp, 3);
	PutX(fp, 1);
	Field(fp, d->mask, 0xffff, 16);
	Field(fp, d->TME, 0x1, 1);
	Field(fp, d->FGE, 0x1, 1);
	Field(fp, d->ABE, 0x1, 1);
	Field(fp, d->FST, 0x1, 1);
	Field(fp, d->AA1, 0x1, 1);
	Field(fp, d->CTXT, 0x1, 1);
	Field(fp, d->a0, 0x3fff, 14);
	Field(fp, d->b0, 0x3fff, 14);
	Field(fp, d->g0, 0x3fff, 14);
	Field(fp, d->r0, 0x3fff, 14);
	PutX(fp, 4);
	PutX(fp, 4);
	PutX(fp, 4);
	PutX(fp, 4);
	PutX(fp, 4);
	PutX(fp, 4);
	PutX(fp, 4);
	PutX(fp, 4);
	FieldLL(fp, d->z0, 0x3f, 38);
	PutX(fp, 10);
	PutX(fp, 10);
	PutX(fp, 1);
	Field(fp, d->s0, 0xfffffff, 28);
	Field(fp, d->t0, 0xfffffff, 28);
	PutX(fp, 7);
	Field(fp, d->s1, 0xfffffff, 28);
	Field(fp, d->t1, 0xfffffff, 28);
	PutX(fp, 7);
	PutX(fp, 7);
	PutX(fp, 7);
	PutX(fp, 7);
	Field(fp, d->f0, 0x3fff, 14);
	PutX(fp, 4);
	PutX(fp, 4);
	PutX(fp, 3);
	PutX(fp, 3);
	PutX(fp, 3);
	PutX(fp, 3);
	PutX(fp, 3);
	PutX(fp, 3);
	PutX(fp, 3);
	Field(fp, d->amask, 0xff, 8);
	PutX(fp, 2);
	fprintf(fp, "\n");
}

void
MyTXM::PrimitiveVec(DDA *d)
{
	Field(fp, 1, 0x1, 1);
	Field(fp, d->isreg, 0x1, 1);
	Field(fp, d->first, 0x1, 1);
	Field(fp, d->px, 0x7ff, 11);
	Field(fp, d->py, 0x7ff, 11);
	Field(fp, d->ydir, 0x1, 1);
	Field(fp, d->mask, 0xffff, 16);
	Field(fp, d->TME, 0x1, 1);
	Field(fp, d->FGE, 0x1, 1);
	Field(fp, d->ABE, 0x1, 1);
	Field(fp, d->FST, 0x1, 1);
	Field(fp, d->AA1, 0x1, 1);
	Field(fp, d->CTXT, 0x1, 1);
	Field(fp, d->a0, 0x3fff, 14);
	Field(fp, d->b0, 0x3fff, 14);
	Field(fp, d->g0, 0x3fff, 14);
	Field(fp, d->r0, 0x3fff, 14);
	Field(fp, d->a1, 0x3fff, 14);
	Field(fp, d->b1, 0x3fff, 14);
	Field(fp, d->g1, 0x3fff, 14);
	Field(fp, d->r1, 0x3fff, 14);
	Field(fp, d->dadx, 0x3fff, 14);
	Field(fp, d->dbdx, 0x3fff, 14);
	Field(fp, d->dgdx, 0x3fff, 14);
	Field(fp, d->drdx, 0x3fff, 14);
	FieldLL(fp, d->z0, 0x3f, 38);
	FieldLL(fp, d->z1, 0x3f, 38);
	FieldLL(fp, d->dzdx, 0xff, 40);
	Field(fp, d->zc, 0x3, 2);
	if (d->TME) {
		Field(fp, d->s0, 0xfffffff, 28);
		Field(fp, d->t0, 0xfffffff, 28);
		Field(fp, d->q0, 0xfffffff, 28);
		Field(fp, d->s1, 0xfffffff, 28);
		Field(fp, d->t1, 0xfffffff, 28);
		Field(fp, d->q1, 0xfffffff, 28);
		Field(fp, d->dsdx, 0xfffffff, 28);
		Field(fp, d->dtdx, 0xfffffff, 28);
		Field(fp, d->dqdx, 0xfffffff, 28);
	} else {
		ZField(fp, 28);
		ZField(fp, 28);
		ZField(fp, 28);
		ZField(fp, 28);
		ZField(fp, 28);
		ZField(fp, 28);
		ZField(fp, 28);
		ZField(fp, 28);
		ZField(fp, 28);
	}
	if (d->FGE) {
		Field(fp, d->f0, 0x3fff, 14);
		Field(fp, d->f1, 0x3fff, 14);
		Field(fp, d->dfdx, 0x3fff, 14);
	} else {
		ZField(fp, 14);
		ZField(fp, 14);
		ZField(fp, 14);
	}
	if (d->AA1) {
		Field(fp, d->cova0, 0xfff, 12);
		Field(fp, d->covb0, 0xfff, 12);
		Field(fp, d->cova1, 0xfff, 12);
		Field(fp, d->covb1, 0xfff, 12);
		Field(fp, d->covdxa, 0xfff, 12);
		Field(fp, d->covdxb0, 0xfff, 12);
		Field(fp, d->covdxb1, 0xfff, 12);
	} else {
		PutX(fp, 3);
		PutX(fp, 3);
		PutX(fp, 3);
		PutX(fp, 3);
		PutX(fp, 3);
		PutX(fp, 3);
		PutX(fp, 3);
	}
	Field(fp, d->amask, 0xff, 8);
	Field(fp, d->maxexp, 0xff, 8);
	fprintf(fp, "\n");
}

void
MyTXM::Put(DDA *d)
{
	if (fp) {
		if (d->isreg)
			RegisterVec(d);
		else
			PrimitiveVec(d);
	}
	TXM::Put(d);
}

/* ---- the MemIF-input vector (the textured 8x2 PixelStamp) ---------- */

void
MyMemIF::RegisterVec(PixelStamp &s)
{
	int reg;
	long long data;
	int i;

	reg = s.reg;
	data = s.data;
	if (reg == 0 || reg == 0x1b)
		return;
	Field(fp, 1, 0x1, 1);
	Field(fp, 1, 0x1, 1);
	PutX(fp, 1);
	Field(fp, reg, 0x7ff, 11);
	PutX(fp, 3);
	Field(fp, (int)data, 0xffffffff, 32);
	for (i = 1; i <= 7; i++)
		XField(fp, 32);
	Field(fp, (int)(data >> 32), 0xffffffff, 32);
	for (i = 9; i <= 15; i++)
		XField(fp, 32);
	for (i = 0; i <= 15; i++)
		XField(fp, 32);
	XLine(fp, "16 1 1");
	fprintf(fp, "\n");
}

void
MyMemIF::PrimitiveVec(PixelStamp &s)
{
	int ok;
	int combmask;
	int livemask;
	int i;

	ok = s.m_34 | s.m_30 | s.m_40;
	ok = ok == 0;
	combmask = s.mask | (s.aamask << 4);
	if (ok)
		livemask = s.mask;
	else
		livemask = s.mask | s.aamask;
	Field(fp, 1, 0x1, 1);
	Field(fp, 0, 0x1, 1);
	Field(fp, s.ctxt, 0x1, 1);
	Field(fp, s.pos.x, 0x7ff, 11);
	Field(fp, s.pos.y, 0x7ff, 11);
	for (i = 0; i <= 15; i++) {
		if ((livemask >> i) & 1)
			Field(fp, s.pix[i].c.A << 24 | s.pix[i].c.B << 16 |
				s.pix[i].c.G << 8 | s.pix[i].c.R,
				0xffffffff, 32);
		else
			XField(fp, 32);
	}
	for (i = 0; i <= 15; i++) {
		if ((livemask >> i) & 1)
			Field(fp, s.pix[i].z, 0xffffffff, 32);
		else
			XField(fp, 32);
	}
	Field(fp, combmask, 0xffff, 16);
	Field(fp, s.ABE, 0x1, 1);
	Field(fp, ok, 0x1, 1);
	fprintf(fp, "\n");
}

void
MyMemIF::Stamp(PixelStamp &s)
{
	if (fp) {
		if (s.type)
			RegisterVec(s);
		else
			PrimitiveVec(s);
	}
	MemIF::Stamp(s);
}

void
MyMemory::Dump()
{
	int i;

	if (fp == 0)
		return;
	for (i = 0; ; i++) {
		if (i > 0xfffff)
			return;
		fprintf(fp, "%08x\n", vram[i]);
	}
}
