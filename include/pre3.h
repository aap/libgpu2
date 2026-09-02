/* Pre3 - GS front end stage 2: primitive assembly.
 *
 * Pre1 hands Pre3 one vertex (or one pass-through register write) at a
 * time; Pre3 keeps the 3-slot vertex queue, decides when a primitive is
 * complete, computes the edge deltas and the signed area, converts S/T/Q
 * to a common fixed point, and forwards to PCalc through PCalc's vtable.
 *
 * Layout verified against orig/lib/pre3.o and the constructor that
 * gpu2.o inlines (sizeof 0x14c, vptr at 0x148, one virtual: Put).
 * The constructor and NumVertex() are inline here because pre3.o emits
 * them last, after Put() - the g++ 2.7 signature of a header inline.
 */

class Pre1;
class Pre3;

/* Minimal stand-in for the next pipeline stage until pcalc.h exists.
 * Only two things matter to pre3.o: sizeof(PCalc) == 0xc00 with the vptr
 * at +0xbfc, and Put() being vtable entry 0.  No vtable is emitted for it
 * here because none of its virtuals is defined in this TU. */
class PCalc {
public:
	char m_pcalc[0xbfc];
	virtual void Put(Pre3 *p);
};

struct Vertex {			/* 0x30 bytes */
	int x;			/* 0x00  screen X, XYOFFSET already applied */
	int y;			/* 0x04 */
	long long z;		/* 0x08  24-bit Z, zero extended */
	int r;			/* 0x10 */
	int g;			/* 0x14 */
	int b;			/* 0x18 */
	int a;			/* 0x1c */
	int f;			/* 0x20  fog */
	int s;			/* 0x24  fixed point, common exponent */
	int t;			/* 0x28 */
	int q;			/* 0x2c */
};

class Pre3 {
public:
	PCalc *pcalc;		/* 0x000 */
	int nvtx;		/* 0x004  vertices in the queue == next slot */
	int S[3];		/* 0x008  raw 24-bit floats from Pre1 */
	int T[3];		/* 0x014 */
	int Q[3];		/* 0x020 */
	int dx[3];		/* 0x02c  dx[i] = v[i].x - v[i+1].x */
	int dy[3];		/* 0x038  dy[i] = v[i+1].y - v[i].y */
	int dxzero[3];		/* 0x044  dx[i] == 0 */
	int dyzero[3];		/* 0x050  dy[i] == 0 */
	int steep[3];		/* 0x05c  |dx[i]| < |dy[i]|, Y is the major axis */
	long long area;		/* 0x068  2 * signed triangle area */
	Vertex v[3];		/* 0x070 */
	long long send_reg;	/* 0x100  pass-through register value */
	int send_addr;		/* 0x108  pass-through register address */
	int type;		/* 0x10c  0 point 1 line 2 triangle 3 sprite */
	int send_type;		/* 0x110  0 = primitive, 1 = register */
	int CTXT;		/* 0x114 */
	int FST;		/* 0x118 */
	int AA1;		/* 0x11c  forced to 0 for points and sprites */
	int m_120;		/* 0x120  cleared by the ctor, never written */
	int ABE;		/* 0x124 */
	int FGE;		/* 0x128 */
	int TME;		/* 0x12c */
	int IIP;		/* 0x130 */
	int FIX;		/* 0x134 */
	int maxexp;		/* 0x138  Pre1::MaxExp() for this primitive */
	int m_13c;		/* 0x13c  never touched by pre3.o */
	int m_140;		/* 0x140  never touched by pre3.o */
	int restart;		/* 0x144  reload the queue; Pre1 clears it */
				/* 0x148 vptr */

	Pre3(PCalc *p) {
		pcalc = p;
		nvtx = 0;
		m_120 = 0;
		maxexp = 0;
		restart = 1;
	}
	int NumVertex() { return nvtx; }

	void Register(Pre1 *p);
	int Float2Fix(int val, unsigned int maxexp);
	void SetAttr(Pre1 *p);
	int Triangle(Pre1 *p);
	void Point(Pre1 *p);
	int Line(Pre1 *p);
	int Sprite(Pre1 *p);
	void Primitive(Pre1 *p);
	virtual void Put(Pre1 *p);
};
