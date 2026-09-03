#include <stdio.h>

#include "dbg.h"

class Pre1;
class Pre3;
class PCalc;
class DDA;

/* The pipeline classes as dbg.c sees them.  These are local views, not the
 * real declarations: include/pre3.h owns Pre3 (and carries a PCalc stand-in
 * with no fields), so including it here would clash - see doc/notes/dbg.md.
 */

struct Vertex {			/* 0x30, cf. include/pre3.h */
	int x;			/* 0x00 */
	int y;			/* 0x04 */
	int z;			/* 0x08 */
	int zh;			/* 0x0c  high half of Pre3's 64 bit Z */
	int r;			/* 0x10 */
	int g;			/* 0x14 */
	int b;			/* 0x18 */
	int a;			/* 0x1c */
	int f;			/* 0x20 */
	int s;			/* 0x24 */
	int t;			/* 0x28 */
	int q;			/* 0x2c */
};

class Pre3 {
public:
	char m_head[0x70 + (sizeof(void *) == 8) * 8];	/* LP64: pre3.h's PCalc* and 8-aligned area */
	Vertex v[3];		/* 0x070 */
	long long send_reg;	/* 0x100 */
	int send_addr;		/* 0x108 */
	int type;		/* 0x10c  0 point 1 line 2 triangle 3 sprite */
	char m_tail[0x148-0x110];
	virtual void Put(Pre1 *p);
};

struct Slope {			/* 0x28 */
	int z;			/* 0x00 */
	int m_04;
	int m_08;
	int a;			/* 0x0c */
	int r;			/* 0x10 */
	int g;			/* 0x14 */
	int b;			/* 0x18 */
	int m_1c;
	int m_20;
	int m_24;
};

class PCalc {
public:
	char m_head[0xb48 + (sizeof(void *) == 8) * 16];	/* LP64: pcalc.h grows 16 bytes before ozdx */
	Slope dx;		/* 0xb48 */
	Slope dy;		/* 0xb70 */
	char m_tail[0xbfc-0xb98];
	virtual void Put(Pre3 *p);
};

class DDA {
public:
	char m_dda[0x250 + (sizeof(void *) == 8) * 40];	/* LP64: dda.h's DDA grows 40 bytes (never allocated here) */
	virtual void Put(PCalc *p);
};

class PPDDA {
public:
	virtual void Put(PCalc *p) = 0;
};

class PPOut : public PPDDA {
public:
	DDA *dda;
	PPOut(DDA *d) { dda = d; }
	void Put(PCalc *p) { dda->Put(p); }
};

class PP {			/* the 0x10 byte front end block */
public:
	Pre1 *pre1;		/* 0x00 */
	Pre3 *pre3;		/* 0x04 */
	PCalc *pcalc;		/* 0x08 */
	PPOut *ppout;		/* 0x0c */
};

class GPU2 {
public:
	PP *pp;			/* 0x00 */
};

struct DbgPos {			/* what DbgInit's void* points at */
	int x;
	int y;
	GPU2 *gpu;
};

class Dbg {
public:
	int mode;		/* 0x00 */
	int x;			/* 0x04 */
	int y;			/* 0x08 */
	Pre3 *pre3;		/* 0x0c */
	PCalc *volatile pcalc;	/* 0x10  volatile: see doc/notes/dbg.md */

	Dbg() { pre3 = 0; y = 0; x = 0; mode = 0; }
	void Dump();
};

static Dbg debug;

void
DbgInit(int mode, void *p)
{
	GPU2 *gpu;

	debug.x = ((DbgPos*)p)->x;
	debug.y = ((DbgPos*)p)->y;
	gpu = ((DbgPos*)p)->gpu;
	debug.pre3 = gpu->pp->pre3;
	debug.pcalc = gpu->pp->pcalc;
}

void
DbgMode(int mode)
{
	debug.mode = mode;
}

void
DbgWatch(int type, int x, int y)
{
	int hit;

	if (debug.mode == 0)
		return;
	hit = debug.x == x && debug.y == y;
	if (hit)
		debug.Dump();
}

void
Dbg::Dump()
{
	Vertex *v = pre3->v;
	int i;

	printf("%d\n", pre3->type);
	for (i = 0; i < 3; i++)
		printf("p%d |%05x %05x %08x |%02x %02x %02x %02x| %04x %04x %04x\n",
			i, v[i].x, v[i].y, v[i].z,
			v[i].r, v[i].g, v[i].b, v[i].a,
			v[i].s, v[i].t, v[i].q);
	printf("d[zrgba]x ");
	printf("%09x %05x %05x %05x %05x\n", pcalc->dx.z,
		pcalc->dx.r, pcalc->dx.g, pcalc->dx.b, pcalc->dx.a);
	printf("d[zrgba]y ");
	printf("%09x %05x %05x %05x %05x\n", pcalc->dy.z,
		pcalc->dy.r, pcalc->dy.g, pcalc->dy.b, pcalc->dy.a);
	printf("\n");
}
