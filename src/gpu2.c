/* gpu2 - GPU2, the model's top-level class.
 *
 * Reconstructed from orig/lib/gpu2.o (0x1aea .text); doc/notes/gpu2.md
 * records the evidence.  The constructor allocates and wires the whole
 * pipeline (Memory, MemIF, TXM, DDA, the PP front end block with
 * PPOut/PCalc/Pre3/Pre1, and one of the three PCRTC builds), Put routes a
 * register write either down the drawing pipe (Pre1) or to the display
 * (PCRTC::SetRegister, virtual), Get reads one 64-bit word back through
 * the BitBLT engine, and GetCRT/dumpCRT are the frame-capture callback
 * pair behind disp_on == 2.
 *
 * Build:  env GCC272_1998=1 tools/gcc272/g++272 -O -idirafter /usr/include
 *         -Iinclude -c src/gpu2.c
 * (1998-mod codegen for the virtual calls; X11 headers via xif.h.)
 *
 * The pipeline stage classes below are local views, not the real
 * declarations - each stage's own header carries a conflicting stand-in
 * view of its neighbours (see src/dbg.c for the same pattern).  The
 * constructor bodies are copied verbatim from include/{pcalc,pre3,dda}.h:
 * gpu2.o inlines them, and their key-method objects carry the out-of-line
 * copies the bodies were byte-verified against.
 */

#include "param.h"
#include "div.h"

class PCalc;

/* The tap between PCalc and the DDA.  One pure virtual, so this TU gets a
 * local `_vt.5PPDDA' whose entry is `__pure_virtual' - it must be declared
 * before txm.h's DDATXM to keep the vtable emission order (reverse
 * declaration order) identical to the 1998 object. */
class PPDDA {
public:
	virtual void Put(PCalc *p) = 0;
};

#include "txm.h"

class Pre1;
class Pre3;

struct Scissor {		/* 0x10, one per context */
	int scax0;
	int scax1;
	int scay0;
	int scay1;
};

/* PCalc as this TU sees it: the constructor (inlined into GPU2::GPU2) and
 * the members it touches; everything else is a hole.  Body verbatim from
 * include/pcalc.h. */
class PCalc {
public:
	PPDDA *out;		/* 0x000 */
	int sft;		/* 0x004 */
	int msft;		/* 0x008 */
	int pix;		/* 0x00c */
	int one;		/* 0x010 */
	param dx;		/* 0x014 */
	param dy;		/* 0x064 */
	param sv;		/* 0x0b4 */
	Reciproc rcp;		/* 0x104 */
	param A;		/* 0x10c */
	param B;		/* 0x15c */
	param C;		/* 0x1ac */
	char m_1fc[0x264-0x1fc];
	Scissor scissor[2];	/* 0x264 */
	char m_284[0xaa4-0x284];
	long long m_aa4;	/* 0xaa4 */
	long long m_aac;	/* 0xaac */
	int m_ab4;		/* 0xab4 */
	char m_ab8[0xbb0-0xab8];
	int SCANMSK;		/* 0xbb0 */
	char m_bb4[0xbe0-0xbb4];
	int maxexp;		/* 0xbe0 */
	char m_be4[0xbfc-0xbe4];
				/* 0xbfc vptr */

	PCalc(PPDDA *p) {
		out = p;
		sft = 4;
		msft = 0x16;
		pix = 0x10;
		one = 1;
		SCANMSK = 0;
		scissor[0].scax0 = 0;
		scissor[0].scax1 = 0;
		scissor[0].scay0 = 0;
		scissor[0].scay1 = 0;
		scissor[1].scax0 = 0;
		scissor[1].scax1 = 0;
		scissor[1].scay0 = 0;
		scissor[1].scay1 = 0;
		m_aa4 = 0xffff;
		m_aac = 0xffff0000;
		m_ab4 = 0x10;
		maxexp = 0;
	}

	virtual void Put(Pre3 *p);
};

/* Pre3; constructor verbatim from include/pre3.h. */
class Pre3 {
public:
	PCalc *pcalc;		/* 0x000 */
	int nvtx;		/* 0x004 */
	char m_008[0x120-0x008];
	int m_120;		/* 0x120 */
	char m_124[0x138-0x124];
	int maxexp;		/* 0x138 */
	char m_13c[0x144-0x13c];
	int restart;		/* 0x144 */
				/* 0x148 vptr */

	Pre3(PCalc *p) {
		pcalc = p;
		nvtx = 0;
		m_120 = 0;
		maxexp = 0;
		restart = 1;
	}

	virtual void Put(Pre1 *p);
};

/* Pre1 has no vtable and an out-of-line constructor (pre1.o); only its
 * size (the `new' argument) is load-bearing here - include/pre1.h has the
 * real fields. */
class Pre1 {
public:
	char m_000[0xac];

	Pre1(Pre3 *p);
	void Put(int addr, long long data);
};

/* The DDA; constructor verbatim from include/dda.h. */
class DDA {
public:
	PCalc *pcalc;		/* 0x000 */
	DDATXM *txm;		/* 0x004 */
	char m_008[0x1e4-0x008];
	int m_1e4;		/* 0x1e4 */
	int m_1e8;		/* 0x1e8 */
	int m_1ec;		/* 0x1ec */
	char m_1f0[0x240-0x1f0];
	int m_240;		/* 0x240 */
	char m_244[0x250-0x244];
				/* 0x250 vptr */

	DDA(DDATXM *t) {
		txm = t;
		m_240 = 0;
		m_1e4 = m_1e8 = m_1ec = 0;
	}

	virtual void Put(PCalc *p);
};

/* The tap installed between PCalc and the DDA (gpu2vec.o's MyPP* classes
 * override Put to write trace vectors; this is the pass-through). */
class PPOut : public PPDDA {
public:
	DDA *dda;		/* 0x04 */
	PPOut(DDA *d) { dda = d; }
	void Put(PCalc *p) { dda->Put(p); }
};

#include "pcrtc.h"

/* The 0x10-byte front end block; no constructor, no vtable. */
class PP {
public:
	Pre1 *pre1;		/* 0x00 */
	Pre3 *pre3;		/* 0x04 */
	PCalc *pcalc;		/* 0x08 */
	PPOut *ppout;		/* 0x0c */
};

#include "gpu2.h"

/* MemIF's constructor was a header inline in 1998 (GPU2::GPU2 inlines it);
 * include/memif.h declares it and src/memif.c carries the out-of-line
 * copy, so the body is repeated here for inlining.  It must match
 * memif.c's. */
inline MemIF::MemIF(Memory *m)
{
	mem = m;
}

/* The frame capture buffer behind GetCRT.  Declaration order (the bss
 * layout) and first-use order (the symbol table) are both the 1998
 * object's. */
static unsigned int r_count;
static unsigned int r_size;
static unsigned int *r_buf;

/* XWindowDump's callback (disp_on == 2): keep a copy of the finished
 * frame for GetCRT to hand out pixel by pixel. */
static void
dumpCRT(int w, int h, const unsigned int *data)
{
	if (r_buf != 0)
		r_buf = (unsigned int *)realloc(r_buf, w*4*h);
	else
		r_buf = (unsigned int *)malloc(w*4*h);
	r_size = w*h;
	r_count = 0;
	memcpy(r_buf, data, r_size*4);
}

unsigned int
GPU2::GetCRT()
{
	if (r_count < r_size)
		return r_buf[r_count++];
	return 0;
}

GPU2::GPU2(char *title, int width, int height, int disp_on)
{
	PP *p;

	mem = new Memory;
	memif = new MemIF(mem);
	txm = new TXM(memif);
	dda = new DDA(txm);

	p = new PP;
	{
		DDA *d = dda;

		p->ppout = new PPOut(d);
	}
	p->pcalc = new PCalc(p->ppout);
	p->pre3 = new Pre3(p->pcalc);
	p->pre1 = new Pre1(p->pre3);
	pp = p;

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
GPU2::Get()
{
	Memory *m = mem;

	return m->bitblt.ReadPixel(m);
}

int
GPU2::Put(int addr, long long data)
{
	if ((char)addr < 0 || addr > 0xff)
		pcrtc->SetRegister(addr, data);
	else {
		PP *q = pp;
		q->pre1->Put(addr, data);
	}
	return 1;
}

void
GPU2::ResizeWindow(int w, int h)
{
	pcrtc->Resize(w, h);
}
