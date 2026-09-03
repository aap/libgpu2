/* PCalc - GS front end stage 3: primitive setup.
 *
 * Pre3 hands PCalc a complete primitive (or a pass-through register
 * write); PCalc turns it into everything the DDA needs: the DDA start
 * position, the per-pixel and per-scanline slopes of every interpolated
 * attribute, the clipped bounding box and the antialias coverage
 * gradients.  It forwards to the next stage (the always-installed PPOut
 * tap, whose base class is PPDDA) through that object's vtable.
 *
 * Layout verified against orig/lib/pcalc.o and the constructor that
 * gpu2.o inlines: sizeof 0xc00, vptr at 0xbfc, one virtual (Put).
 * The constructor, Floor(), Ceil() and Subpixel() are defined inline
 * here because pcalc.o emits them after Put() - the g++ 2.7 signature
 * of a header inline.  They are declared in the reverse of the order
 * they come out in .text (Subpixel, Ceil, Floor, ctor -> ctor, Floor,
 * Ceil, Subpixel): g++ 2.7 emits deferred inlines last-declared-first,
 * and getting that right makes the whole symbol table match.
 *
 * Pre3 is declared here rather than pulled from include/pre3.h: that
 * header carries a stand-in PCalc of its own, so the two cannot be
 * included together.  The layout below is the same one, verified in
 * doc/notes/pre3.md.
 */

#include "param.h"
#include "div.h"
#include "slong.h"

class PCalc;

struct PVertex {		/* Pre3's vertex, 0x30 bytes */
	int x;
	int y;
	long long z;
	unsigned int r, g, b, a;
	unsigned int f;
	int s, t, q;
};

class Pre3 {
public:
	/* pre3.h's Pre3 has a virtual Put and this view of it has not; the
	 * two agree only because g++ 2.7 puts the vptr *after* the data
	 * (doc/ABI.md).  Modern C++ puts it first, so a modern build needs
	 * the hole here or every offset below is one pointer short. */
#if __GNUC__ >= 3
	void *era_vptr_hole;
#endif
	PCalc *pcalc;		/* 0x000 */
	int nvtx;		/* 0x004 */
	int S[3];		/* 0x008 */
	int T[3];		/* 0x014 */
	int Q[3];		/* 0x020 */
	int dx[3];		/* 0x02c */
	int dy[3];		/* 0x038 */
	int dxzero[3];		/* 0x044 */
	int dyzero[3];		/* 0x050 */
	int steep[3];		/* 0x05c */
	long long area;		/* 0x068 */
	PVertex v[3];		/* 0x070 */
	long long send_reg;	/* 0x100 */
	unsigned int send_addr;	/* 0x108 */
	int type;		/* 0x10c */
	int send_type;		/* 0x110 */
	int CTXT;		/* 0x114 */
	int FST;		/* 0x118 */
	int AA1;		/* 0x11c */
	int m_120;		/* 0x120 */
	int ABE;		/* 0x124 */
	int FGE;		/* 0x128 */
	int TME;		/* 0x12c */
	int IIP;		/* 0x130 */
	int FIX;		/* 0x134 */
	int maxexp;		/* 0x138 */
	int m_13c;		/* 0x13c */
	int m_140;		/* 0x140 */
	int restart;		/* 0x144 */
};

/* The next stage.  Only its vtable matters here: one entry, Put. */
class PPDDA {
public:
	virtual void Put(PCalc *p);
};

struct Scissor {		/* 0x10, one per context */
	int scax0;
	int scax1;
	int scay0;
	int scay1;
};

class PCalc {
public:
	PPDDA *out;		/* 0x000  next stage */
	int sft;		/* 0x004  subpixel bits, 4 */
	int msft;		/* 0x008  0x16 */
	int pix;		/* 0x00c  0x10, one pixel in subpixel units */
	int one;		/* 0x010  1 */

	param dx;		/* 0x014  d/dx of every attribute */
	param dy;		/* 0x064  d/dy */
	param sv;		/* 0x0b4  start value */
	Reciproc rcp;		/* 0x104 */
	param A;		/* 0x10c  the three sorted vertices */
	param B;		/* 0x15c */
	param C;		/* 0x1ac */

	char spoint;		/* 0x1fc  vertex the scan starts at */
	char epointy;		/* 0x1fd  vertex supplying the end Y */
	char epointx;		/* 0x1fe  vertex supplying the end X */

	int sx;			/* 0x200  start point, subpixel */
	int sy;			/* 0x204 */
	int sxi;		/* 0x208  start point, pixels */
	int syi;		/* 0x20c */
	int ex;			/* 0x210  end point, subpixel */
	int ey;			/* 0x214 */
	int exi;		/* 0x218  end point, pixels */
	int eyi;		/* 0x21c */
	int m_220;		/* 0x220 */
	int stampw;		/* 0x224  4 or 8 */
	int m_228;		/* 0x228 */
	int m_22c;		/* 0x22c */
	int m_230;		/* 0x230 */
	int m_234;		/* 0x234 */
	int sortcode;		/* 0x238 */
	int m_23c;		/* 0x23c */
	int m_240;		/* 0x240 */
	int m_244;		/* 0x244 */
	int m_248;		/* 0x248 */
	int cov[6];		/* 0x24c  AA coverage slopes, per edge */
	Scissor scissor[2];	/* 0x264 */
	char m_284[0x800];	/* 0x284  never referenced by pcalc.o */

	long long m_a84;	/* 0xa84 */
	long long m_a8c;	/* 0xa8c */
	long long m_a94;	/* 0xa94 */
	long long m_a9c;	/* 0xa9c */
	long long m_aa4;	/* 0xaa4  0xffff */
	long long m_aac;	/* 0xaac  0xffff0000 */
	int m_ab4;		/* 0xab4  0x10 */
	int FIX;		/* 0xab8 */
	int m_abc;		/* 0xabc */
	int m_ac0;		/* 0xac0 */
	int m_ac4;		/* 0xac4 */
	int ddx[3];		/* 0xac8 */
	int ddy[3];		/* 0xad4 */
	int bbl;		/* 0xae0 */
	int bbt;		/* 0xae4 */
	int bbr;		/* 0xae8 */
	int bbb;		/* 0xaec */
	int m_af0;		/* 0xaf0 */
	int ddax;		/* 0xaf4  DDA start x */
	int dday;		/* 0xaf8  DDA start y */
	unsigned int covs[3];	/* 0xafc  AA coverage start values */
	unsigned int covdx[3];	/* 0xb08 */
	unsigned int covdy[3];	/* 0xb14 */

	long long ozv;		/* 0xb20  output block: start values */
	int ofv;		/* 0xb28 */
	int oav;		/* 0xb2c */
	int orv;		/* 0xb30 */
	int ogv;		/* 0xb34 */
	int obv;		/* 0xb38 */
	int osv;		/* 0xb3c */
	int otv;		/* 0xb40 */
	int oqv;		/* 0xb44 */
	long long ozdx;		/* 0xb48 */
	int ofdx;		/* 0xb50 */
	int oadx;		/* 0xb54 */
	int ordx;		/* 0xb58 */
	int ogdx;		/* 0xb5c */
	int obdx;		/* 0xb60 */
	int osdx;		/* 0xb64 */
	int otdx;		/* 0xb68 */
	int oqdx;		/* 0xb6c */
	long long ozdy;		/* 0xb70 */
	int ofdy;		/* 0xb78 */
	int oady;		/* 0xb7c */
	int ordy;		/* 0xb80 */
	int ogdy;		/* 0xb84 */
	int obdy;		/* 0xb88 */
	int osdy;		/* 0xb8c */
	int otdy;		/* 0xb90 */
	int oqdy;		/* 0xb94 */

	int xdir;		/* 0xb98 */
	int ydir;		/* 0xb9c */
	int steep[3];		/* 0xba0 */
	int flat;		/* 0xbac */
	int SCANMSK;		/* 0xbb0 */
	int send_type;		/* 0xbb4 */
	int send_addr;		/* 0xbb8 */
	long long send_reg;	/* 0xbbc */
	int TME;		/* 0xbc4 */
	int FGE;		/* 0xbc8 */
	int ABE;		/* 0xbcc */
	int AA1;		/* 0xbd0 */
	int m_bd4;		/* 0xbd4 */
	int CTXT;		/* 0xbd8 */
	int FST;		/* 0xbdc */
	int maxexp;		/* 0xbe0 */
	int m_be4;		/* 0xbe4 */
	int m_be8;		/* 0xbe8 */
	int m_bec;		/* 0xbec */
	unsigned int rem;	/* 0xbf0  reciproc's remainder output */
	int m_bf4;		/* 0xbf4 */
	int type;		/* 0xbf8 */
				/* 0xbfc vptr */

	int Subpixel(const int &v) { return v & ((1 << sft) - 1); }
	int Ceil(const int &v) {
		if (v & ((1 << sft) - 1))
			return (v >> sft) + 1;
		else
			return v >> sft;
	}
	int Floor(const int &v) { return v >> sft; }
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

	void SwapLine(int &a, int &b, int &c, int &d);
	void SwapLine(unsigned int &a, unsigned int &b, unsigned int &c,
		unsigned int &d);
	void SortVertex(Pre3 *p, param *v);
	void GetSPoint(void);
	void CorrectSPoint(void);
	void CorrectEPoint(void);
	void Slope(Pre3 *p, param *v);
	void CheckOverFlow(void);
	void StartVal(Pre3 *p, param *v, param w);
	void GetDDAStart(Pre3 *p);
	int AASlope(long long x, int n, int d);
	long long C_Hosei(long long v, int d);
	void SortCoverage(Pre3 *p);
	int AAStartVal(int a, int b, int c, int d);
	void AACoverage(Pre3 *p);
	void DrawTriangle(Pre3 *p);
	void BBox(void);
	void SortLine(param *v);
	void CorrectLineStart(void);
	void CorrectLineEnd(void);
	void LineSlope(param *v, int n, param &d, long long &a, long long &b,
		int &c, int &e);
	void LineDDAEdgeStart(void);
	void LineAACov(Pre3 *p);
	void DrawLine(Pre3 *p);
	void SpriteSlope(long long x, int n, long long &r);
	void SpriteStartVal(long long &r, long long a, long long b, int c);
	void DrawSprite(Pre3 *p);
	void DrawPoint(Pre3 *p);
	void ReverseDir(void);
	void Primitive(Pre3 *p);
	void Register(Pre3 *p);
	virtual void Put(Pre3 *p);
};
