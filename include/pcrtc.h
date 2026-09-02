#ifndef PCRTC_H
#define PCRTC_H

/* PCRTC - the display / CRTC circuit.
 *
 * Everything behind GPU2::Put's privileged path (addr >= 0x80) lives here:
 * PMODE, SMODE1/2, SYNCH1/2, SYNCV, DISPFB1/2, DISPLAY1/2, EXTBUF, EXTDATA,
 * EXTWRITE and BGCOLOR, plus the model's own two pseudo-registers 0x100
 * ("Display", the old unmerged path) and 0x101 ("DisplayPcrtc", the real
 * two-circuit merge).  There is deliberately NO CSR/IMR/BUSDIR code in this
 * object - those live in gpu2.o.
 *
 * Class shapes verified against orig/lib/pcrtc.o; see doc/notes/pcrtc.md.
 *
 *   PCRTC        0x40   the EXTBUF/EXTDATA/EXTWRITE write-back engine and
 *                       the two virtuals GPU2 talks to (SetRegister,
 *                       Resize).  All members inline -> _vt.5PCRTC is a
 *                       *local* vtable in every including object.
 *   PCRTCdmy     0x40   the no-display build: adds one virtual of its own,
 *                       which is why _vt.8PCRTCdmy has three entries where
 *                       _vt.5PCRTC has two.
 *   PCRTCxif     0x1d4  PCRTC + DispInfo + an Xifbase*.  SetRegister and
 *                       Resize are the *only* out-of-line virtuals in the
 *                       hierarchy, so this class is the one with a key
 *                       method: pcrtc.o carries _vt.8PCRTCxif globally and
 *                       every one of PCRTCxif's inline members as a global
 *                       out-of-line copy (gpu2.o carries none of them).
 *   DispInfo     0x190  the display state proper - PMODE/SMODE/SYNC/BGCOLOR
 *                       fields, the two display circuits, the readers and
 *                       the blenders.  Derives from AddrConv because
 *                       oldDispPixelMag addresses VRAM through itself.
 *   DispCirc     0x114  the two circuits, the three PSM readers and the
 *                       merged output rectangle.
 *
 * ORDER IS LOAD BEARING.  g++ 2.7 builds RTL for an inline member as soon
 * as it parses it, so the string constants below land in .rodata in
 * *declaration* order (DispCirc's two DISPFB messages, then PCRTC's three
 * EXTWRITE ones, then PCRTCxif's), while the deferred out-of-line copies
 * come out at the end of .text in *reverse* declaration order (PCRTCdmy,
 * PCRTCxif, PCRTC, MemRead16/24/32, PixelBlend1a/Alp, XWindowDump,
 * Xifbase).  The vtables in .rodata follow the same reverse order.  Do not
 * reorder the classes or their members without re-checking
 * doc/notes/pcrtc.md.
 *
 * The assert on line 574 (PCRTCxif::DisplayPcrtc) is baked into pcrtc.o by
 * __LINE__.  Do not move any line above it.
 */

/* Xlib.h declares `struct Depth', and g++ 2.7 will not have that tag in the
 * same scope as bitblt.h's `int Depth(int)' - in either order.  pcrtc.c is
 * the one translation unit that needs both, so pull Xlib in first with its
 * `Depth' renamed; nothing here uses Screen::depths.  XLIB_ILLEGAL_ACCESS
 * has to be set before the first Xlib.h, and xif.h's own #define is then a
 * harmless repeat of the same empty definition. */
#define XLIB_ILLEGAL_ACCESS
#define Depth XlibDepth
#include <X11/Xlib.h>
#undef Depth

#include "memory.h"
#include "xif.h"

/* xif.h leaves its own assert() defined, with "xif.h" baked in as the file
 * name; ours has to say "pcrtc.h". */
#undef assert
#define assert(e) \
	((e) ? (void)0 : __assert_fail(#e, "pcrtc.h", __LINE__, \
		__PRETTY_FUNCTION__))

/* What the display circuit does with the alpha it hands to the X11 side.
 * The 1998 name is pinned by the mangling of DispInfo::displayNoBlend
 * (`...P7Xifbase4AOut'); the enumerator names are not. */
enum AOut {
	AOutZero,		/* 0: hand out alpha 0 */
	AOutAlpha		/* 1: hand out the circuit's own alpha */
};

/* Set a colour's RGB and leave its alpha alone.  This has to be a function,
 * not three assignments: g++ 2.7 evaluates an inlined call's arguments into
 * pseudos before it runs the body, so the object loads all three sources
 * and only then stores all three - which is exactly what SetBGCOLOR and
 * displayBlendBGAmod2 do, and what three plain assignments do not. */
static inline void
SetRGB(PixColor &c, int r, int g, int b)
{
	c.R = r;
	c.G = g;
	c.B = b;
}

/* Five bits of colour widened to eight, replicating the top three into the
 * bottom.  Also a function rather than an expression: passing the field
 * through a parameter is what stops g++ 2.7 folding the `<< 3' back into
 * the mask that produced it. */
static inline int
ext5(unsigned v)
{
	return (v << 3) | (v >> 2);
}

/* PMODE.MMOD picks which of these two does the circuit-1-over-circuit-2
 * blend.  `alp' is the blend factor for PixelBlendAlp (PMODE.ALP) and is
 * unused - but still present, and still assigned by SetPMODE - for
 * PixelBlend1a, which uses twice circuit 1's own alpha instead. */
class PixelBlend {
public:
	int alp;		/* 0x00 */
				/* 0x04 vptr */
	virtual void blend(PixColor &d, const PixColor &s) = 0;
};

class PixelBlendAlp : public PixelBlend {
public:
	void blend(PixColor &d, const PixColor &s)
	{
		d.R = (d.R*alp + (255 - alp)*s.R) >> 8;
		d.G = (d.G*alp + (255 - alp)*s.G) >> 8;
		d.B = (d.B*alp + (255 - alp)*s.B) >> 8;
	}
};

class PixelBlend1a : public PixelBlend {
public:
	void blend(PixColor &d, const PixColor &s)
	{
		int a, t;

		t = d.A + d.A;
		if (t <= 0xfe)
			a = t;
		else
			a = 0xff;
		d.R = (d.R*a + (255 - a)*s.R) >> 8;
		d.G = (d.G*a + (255 - a)*s.G) >> 8;
		d.B = (d.B*a + (255 - a)*s.B) >> 8;
	}
};

/* One PSM's worth of "read a display pixel out of local memory".  Three of
 * them live in DispCirc and DISPFB picks one per circuit.  Derives from
 * AddrConv for the same reason FBConfig does. */
class MemRead : public AddrConv {
public:
	int PSM;		/* 0x20 */
	int FBP;		/* 0x24  a word address (DISPFB.FBP*2048) */
	int FBW;		/* 0x28  pixels (DISPFB.FBW*64) */
				/* 0x2c  vptr */

	void Set(int psm, int fbp, int fbw)
	{
		PSM = psm;
		FBP = fbp;
		FBW = fbw;
	}
	virtual void ReadPixel(Memory *mem, int x, int y, PixColor &c) = 0;
};

class MemRead32 : public MemRead {
public:
	MemRead32()
	{
		PSM = 0;
		FBP = 0;
		FBW = 640;
	}
	void ReadPixel(Memory *mem, int x, int y, PixColor &c)
	{
		unsigned data;

		Address(x, y, PSM, FBW, FBP);
		data = mem->vram[addr];
		SetRGB(c, data & 0xff, (data >> 8) & 0xff,
			(data >> 16) & 0xff);
		c.A = data >> 24;
	}
};

class MemRead24 : public MemRead {
public:
	MemRead24()
	{
		PSM = 1;
		FBP = 0;
		FBW = 640;
	}
	void ReadPixel(Memory *mem, int x, int y, PixColor &c)
	{
		unsigned data;

		Address(x, y, PSM, FBW, FBP);
		data = mem->vram[addr];
		data = data & 0xffffff | 0x80000000;
		SetRGB(c, data & 0xff, (data >> 8) & 0xff,
			(data >> 16) & 0xff);
		c.A = data >> 24;
	}
};

class MemRead16 : public MemRead {
public:
	MemRead16()
	{
		PSM = 2;
		FBP = 0;
		FBW = 640;
	}
	void ReadPixel(Memory *mem, int x, int y, PixColor &c)
	{
		unsigned data;
		int d, r, g, b, a;

		Address(x, y, PSM, FBW, FBP);
		data = mem->vram[addr];
		if (bitpos)
			data >>= 16;
		d = data & 0xffff;
		r = ext5(d & 0x1f);
		g = ext5((d >> 5) & 0x1f);
		b = ext5((d >> 10) & 0x1f);
		a = (short)d < 0 ? 0x80 : 0;
		c.R = r;
		c.G = g;
		c.B = b;
		c.A = a;
	}
};

/* One display circuit: where it reads from (DISPFB) and where it lands on
 * the CRT (DISPLAY).  `w'/`h' shadow `DW'/`DH' - the readback loops use the
 * first pair and the merge arithmetic the second, and DISPLAY writes both. */
struct Circuit {		/* 0x34 */
	int FBP;		/* 0x00  DISPFB.FBP*2048, a word address */
	int FBW;		/* 0x04  DISPFB.FBW*64, pixels */
	int PSM;		/* 0x08  DISPFB.PSM, raw */
	int DBX;		/* 0x0c */
	int DBY;		/* 0x10 */
	int w;			/* 0x14  DISPLAY.DW+1 */
	int h;			/* 0x18  DISPLAY.DH+1 */
	int DX;			/* 0x1c */
	int DY;			/* 0x20 */
	int DW;			/* 0x24  DISPLAY.DW+1 again */
	int DH;			/* 0x28  DISPLAY.DH+1 again */
	int MAGV;		/* 0x2c */
	int MAGH;		/* 0x30 */
};

/* Recompute the merged output rectangle and the overlap flag.  This is a
 * macro, not a function: it is ~70 insns, which is over g++ 2.7's
 * INTEGRATE_THRESHOLD, so an inline member would be left out of line - and
 * the 1998 object has no such symbol and carries a full copy in each of
 * SetDISPLAY1/2, in both constructors and twice more inside SetRegister.
 * Same trick as memif.c's Set* macros. */
#define UPDATEMERGE()							\
	if (c[0].DX < c[1].DX + c[1].DW &&				\
	    c[1].DX < c[0].DX + c[0].DW &&				\
	    c[0].DY < c[1].DY + c[1].DH &&				\
	    c[1].DY < c[0].DY + c[0].DH)				\
		over = 1;						\
	else								\
		over = 0;						\
	if (c[0].DX <= c[1].DX)						\
		mx = c[0].DX;						\
	else								\
		mx = c[1].DX;						\
	if (c[0].DY <= c[1].DY)						\
		my = c[0].DY;						\
	else								\
		my = c[1].DY;						\
	if (c[0].DX + c[0].DW > c[1].DX + c[1].DW)			\
		mw = c[0].DX + c[0].DW - mx;				\
	else								\
		mw = c[1].DX + c[1].DW - mx;				\
	if (c[0].DY + c[0].DH > c[1].DY + c[1].DH)			\
		mh = c[0].DY + c[0].DH - my;				\
	else								\
		mh = c[1].DY + c[1].DH - my;

/* The two circuits, their readers, and the rectangle that covers both.
 *
 * `over' says the two DISPLAY rectangles overlap; the merged rectangle
 * (mx,my,mw,mh) is their bounding box and is what the blending paths walk. */
class DispCirc {		/* 0x114 */
public:
	int over;		/* 0x00 */
	int mx;			/* 0x04 */
	int my;			/* 0x08 */
	int mw;			/* 0x0c */
	int mh;			/* 0x10 */
	MemRead *rd[2];		/* 0x14 */
	MemRead32 r32;		/* 0x1c */
	MemRead24 r24;		/* 0x4c */
	MemRead16 r16;		/* 0x7c */
	Circuit c[2];		/* 0xac */

	DispCirc()
	{
		c[0].FBP = 0;
		c[0].FBW = 640;
		c[0].PSM = 0;
		c[0].DBX = c[0].DBY = c[0].DX = c[0].DY = 0;
		c[0].w = c[0].DW = 640;
		c[0].h = c[0].DH = 480;
		c[0].MAGV = c[0].MAGH = 0;
		c[1].FBP = 0;
		c[1].FBW = 640;
		c[1].PSM = 0;
		c[1].DBX = c[1].DBY = c[1].DX = c[1].DY = 0;
		c[1].w = c[1].DW = 640;
		c[1].h = c[1].DH = 480;
		c[1].MAGV = c[1].MAGH = 0;
		rd[0] = &r32;
		rd[1] = &r32;
		UPDATEMERGE();
	}

	void SetDISPFB(int n, long long data)
	{
		int psm;

		c[n].FBP = (data & 0x1ff) << 11;
		c[n].FBW = ((data >> 9) & 0x3f) << 6;
		psm = (data >> 15) & 0x1f;
		switch (psm) {
		case 0:
			c[n].PSM = 0;
			rd[n] = &r32;
			break;
		case 1:
			c[n].PSM = 1;
			rd[n] = &r24;
			break;
		case 2:
			c[n].PSM = 2;
			rd[n] = &r16;
			break;
		case 10:
			c[n].PSM = 10;
			rd[n] = &r16;
			break;
		case 19:
			fprintf(stderr,
				"PS_GPU2 display mode is not supported\n");
			exit(0);
			break;
		default:
			fprintf(stderr, "DISPFB: PSM[%d] is invalid.\n", psm);
			c[n].PSM = 0;
			rd[n] = &r32;
			break;
		}
		c[n].DBX = (data >> 32) & 0x7ff;
		c[n].DBY = (data >> 43) & 0x7ff;
		rd[n]->Set(psm, c[n].FBP, c[n].FBW);
	}

	void SetDISPLAY(int n, long long data)
	{
		c[n].DX = data & 0xfff;
		c[n].DY = (data >> 12) & 0x7ff;
		c[n].w = c[n].DW = ((data >> 32) & 0xfff) + 1;
		c[n].h = c[n].DH = ((data >> 44) & 0x7ff) + 1;
		c[n].MAGH = (data >> 23) & 0xf;
		c[n].MAGV = (data >> 27) & 3;
		UPDATEMERGE();
	}

	int Inside(int n, int x, int y)
	{
		return c[n].DX <= x && x < c[n].DX + c[n].DW &&
			c[n].DY <= y && y < c[n].DY + c[n].DH;
	}
	int Magnified(int n)
	{
		return c[n].MAGH != 0 || c[n].MAGV != 0;
	}
};

/* The whole display state.  PCRTCxif inherits it; the eight functions that
 * actually paint live in src/pcrtc.c. */
class DispInfo : public AddrConv {	/* 0x190 */
public:
	int EN1;		/* 0x20  PMODE */
	int EN2;		/* 0x24 */
	int CRTMD;		/* 0x28 */
	int AMOD;		/* 0x2c */
	int MMOD;		/* 0x30 */
	int SLBG;		/* 0x34 */
	int ALP;		/* 0x38 */
	int hstart;		/* 0x3c  SYNCH1 */
	int vstart;		/* 0x40  SYNCV */
	int interlace;		/* 0x44 */
	int INT;		/* 0x48  SMODE1 */
	int FFMD;		/* 0x4c  SMODE2 */
	AOut aout1;		/* 0x50  AMOD == 0 */
	AOut aout2;		/* 0x54  AMOD != 0 */
	PixColor bg;		/* 0x58  BGCOLOR */
	DispCirc dc;		/* 0x68 */
	PixelBlendAlp balp;	/* 0x17c */
	PixelBlend1a b1a;	/* 0x184 */
	PixelBlend *bl;		/* 0x18c */

	/* The power-on defaults are NTSC's: interlaced, FFMD=1, INT=2, and
	 * the SYNCH1/SYNCV offsets a 640x480 NTSC mode ends up with. */
	DispInfo()
	{
		bl = &b1a;
		b1a.alp = 0;
		aout1 = aout2 = AOutAlpha;
		hstart = 652;
		vstart = 38;
		interlace = 1;
		INT = 2;
		FFMD = 1;
	}

	/* SYNCV's vertical start, halved when the field is interlaced.
	 * This has to be a function, not a ternary in the argument it
	 * feeds: g++ 2.7 distributes the subtraction into both arms of a
	 * COND_EXPR operand, and the 1998 object does one subtraction. */
	int VStart()
	{
		int v;

		v = vstart;
		if (interlace == 1)
			v = v/2;
		return v;
	}

	/* Circuit 1 needs no blending when MMOD says "use PMODE.ALP" and
	 * ALP is fully opaque.  A function, so that the 1998 object's
	 * materialise-then-test shape comes out. */
	int NeedBlend()
	{
		return MMOD != 1 || ALP != 0xff;
	}

	void DisplayPixel(int dn, Memory *mem, Xifbase *xif);
	void displayNoBlend(int dn, Memory *mem, Xifbase *xif, AOut aout);
	void displayNoBlendMag(int dn, Memory *mem, Xifbase *xif, AOut aout);
	void displayBlend(Memory *mem, Xifbase *xif);
	void displayBlendBGAmod2(Memory *mem, Xifbase *xif);
	void displayBlendBG(Memory *mem, Xifbase *xif, AOut aout);
	void oldDispPixel(int dn, Memory *mem, Xifbase *xif);
	void oldDispPixelMag(int dn, Memory *mem, Xifbase *xif,
		int hmag, int vmag);
};

/* PCRTC proper: the external-video write-back path (EXTBUF/EXTDATA/
 * EXTWRITE), which reads a Frame2d the host has filled in and pushes it
 * into local memory through FBConfig::WritePixel.  `ext' is null in every
 * build in the archive, so EXTWRITE is a no-op in practice. */
class PCRTC {			/* 0x40 */
public:
	Memory *mem;		/* 0x00 */
	int EXBP;		/* 0x04  EXTBUF.EXBP*64, a word address */
	int EXBW;		/* 0x08  EXTBUF.EXBW*64, pixels */
	int WDX;		/* 0x0c */
	int WDY;		/* 0x10 */
	int WFFMD;		/* 0x14 */
	int EMODA;		/* 0x18 */
	int EMODC;		/* 0x1c */
	int SX;			/* 0x20  EXTDATA */
	int SY;			/* 0x24 */
	int WW;			/* 0x28  EXTDATA.WW+1 */
	int WH;			/* 0x2c  EXTDATA.WH+1 */
	int SMPH;		/* 0x30 */
	int SMPV;		/* 0x34 */
	Frame2d *ext;		/* 0x38  the incoming video, never set */
				/* 0x3c  vptr */

	PCRTC(Memory *m)
	{
		mem = m;
		EXBP = 0;
		EXBW = 0x20;
		WDX = WDY = 0;
		SX = 0x100;
		SY = 0x80;
		WW = 0x100;
		WH = 0x80;
		WFFMD = 0;
		EMODA = 0;
		EMODC = 0;
		SMPH = SMPV = 0;
		ext = 0;
	}

	/* BT.601 luma, in the integer form the 1998 code uses. */
	int Luminance(int r, int g, int b)
	{
		return ((r*66) >> 8) + ((g*129) >> 8) + ((b*25) >> 8) + 16;
	}
	void ConvertA(PixColor &c)
	{
		switch (EMODA) {
		case 0:
			break;
		case 1:
			c.A = Luminance(c.R, c.G, c.B);
			break;
		case 2:
			c.A = Luminance(c.R, c.G, c.B)/2;
			break;
		case 3:
			c.A = 0;
			break;
		default:
			fprintf(stderr, "EMODA [%d] is invalid.\n", EMODA);
			exit(1);
		}
	}
	void ConvertC(PixColor &c, int a)
	{
		int r, g, b, y, cb, cr;

		switch (EMODC) {
		case 0:
			break;
		case 1:
			y = Luminance(c.R, c.G, c.B);
			c.R = y;
			c.G = y;
			c.B = y;
			break;
		case 2:
			r = c.R;
			g = c.G;
			b = c.B;
			y = Luminance(r, g, b);
			cb = ((g*74) >> 8) - ((r*38) >> 8) - ((b*112) >> 8);
			cr = ((r*112) >> 8) - ((g*96) >> 8) - ((b*18) >> 8);
			c.R = y;
			c.G = cb + 128;
			c.B = cr + 128;
			break;
		case 3:
			c.R = a;
			c.G = a;
			c.B = a;
			break;
		default:
			fprintf(stderr, "EMODC [%d] is invalid.\n", EMODC);
			exit(1);
		}
	}

	void SetEXTBUF(long long data)
	{
		EXBP = (data & 0x3fff) << 6;
		EXBW = ((data >> 14) & 0x3f) << 6;
		WDX = (data >> 32) & 0x7ff;
		WDY = (data >> 43) & 0x7ff;
		WFFMD = (data >> 22) & 1;
		EMODA = (data >> 23) & 3;
		EMODC = (data >> 25) & 3;
	}
	void SetEXTDATA(long long data)
	{
		SX = data & 0xfff;
		SY = (data >> 12) & 0x7ff;
		WW = ((data >> 32) & 0xfff) + 1;
		WH = ((data >> 44) & 0x7ff) + 1;
		SMPH = (data >> 23) & 0xf;
		SMPV = (data >> 27) & 3;
	}
	void SetEXTWRITE(long long data)
	{
		int x, y, sx, sy, a, ex, ey;
		char sz;
		PixColor c;
		unsigned d;

		if (ext == 0)
			return;
		ex = (SMPH + 1)*WW + SX;
		if (WFFMD == 0)
			ey = SY + SY + (SMPV + 1)*WH;
		else
			ey = (SMPV + 1)*WH + SY;
		sz = ext->h < ey || ext->w < ex;
		if (sz) {
			fprintf(stderr,
				"EXTWRITE: ext-video's size is not enough.\n");
			return;
		}
		for (y = 0; y < WH; y++)
			for (x = 0; x < WW; x++) {
				sx = (SMPH + 1)*x + SX;
				if (WFFMD == 0) {
					if (y & 1)
						sy = (SMPV + 1)*(y/2)
							+ (SY + ext->h/2);
					else
						sy = (SMPV + 1)*(y/2) + SY;
				} else
					sy = (SMPV + 1)*y + SY;
				d = ext->Get(sx, sy);
				c.R = d & 0xff;
				c.G = (d >> 8) & 0xff;
				c.B = (d >> 16) & 0xff;
				c.A = d >> 24;
				a = c.A;
				ConvertA(c);
				ConvertC(c, a);
				mem->fb.WritePixel(mem, WDX + x, WDY + y, c,
					1, EXBP, EXBW);
			}
	}

	virtual void SetRegister(int addr, long long data)
	{
		switch (addr) {
		case 0x8b:
			SetEXTBUF(data);
			break;
		case 0x8c:
			SetEXTDATA(data);
			break;
		case 0x8d:
			SetEXTWRITE(data);
			break;
		}
	}
	virtual void Resize(int w, int h) { }
};

/* The X11-backed PCRTC.  The bodies of every SetXxx below are repeated
 * inside SetRegister by inlining, which is why the 1998 object carries both
 * an out-of-line copy of each and a copy inside SetRegister. */
class PCRTCxif : public PCRTC {
public:
	DispInfo di;		/* 0x40 */
	Xifbase *xif;		/* 0x1d0 */

	void SetDISPFB1(long long data) { di.dc.SetDISPFB(0, data); }
	void SetDISPFB2(long long data) { di.dc.SetDISPFB(1, data); }
	void SetDISPLAY1(long long data) { di.dc.SetDISPLAY(0, data); }
	void SetDISPLAY2(long long data) { di.dc.SetDISPLAY(1, data); }
	void SetPMODE(long long data)
	{
		di.EN1 = data & 1;
		di.EN2 = (data >> 1) & 1;
		di.CRTMD = (data >> 2) & 7;
		di.AMOD = (data >> 6) & 1;
		di.MMOD = (data >> 5) & 1;
		di.SLBG = (data >> 7) & 1;
		di.ALP = (data >> 8) & 0xff;
		if (di.MMOD == 1)
			di.bl = &di.balp;
		else
			di.bl = &di.b1a;
		di.bl->alp = di.ALP;
		if (di.AMOD == 0) {
			di.aout1 = AOutAlpha;
			di.aout2 = AOutZero;
		} else {
			di.aout1 = AOutZero;
			di.aout2 = AOutAlpha;
		}
	}
	void SetBGCOLOR(long long data)
	{
		SetRGB(di.bg, data & 0xff, (data >> 8) & 0xff,
			(data >> 16) & 0xff);
		di.bg.A = 0;
	}
	void SetSMODE1(long long data)
	{
		di.INT = (data >> 13) & 3;
		if (di.INT == 0 && di.FFMD == 0)
			di.interlace = 0;
		else
			di.interlace = 1;
	}
	void SetSMODE2(long long data)
	{
		di.FFMD = data & 1;
		if (di.INT == 0 && di.FFMD == 0)
			di.interlace = 0;
		else
			di.interlace = 1;
	}
	void SetSYNCH1(long long data)
	{
		di.hstart = ((data >> 11) & 0x7ff) + ((data >> 43) & 0x3ff);
	}
	void SetSYNCH2(long long data) { }
	void SetSYNCV(long long data)
	{
		di.vstart = ((data >> 20) & 0x3ff) + ((data >> 32) & 0x3ff)
			+ ((data >> 53) & 0x3ff);
	}
	void Display(long long data)
	{
		int dn, hmag, vmag;

		dn = data & 3;
		if (dn > 1) {
			fprintf(stderr, "Invlaid display (0x%x)\n", dn);
			exit(0);
		}
		hmag = (data >> 32) & 0xffff;
		vmag = (data >> 48) & 0xffff;
		if (hmag == 0 && vmag == 0)
			di.oldDispPixel(dn, mem, xif);
		else
			di.oldDispPixelMag(dn, mem, xif, hmag, vmag);
	}
/* The 1998 pcrtc.h put the assert below on line 574 and __LINE__ bakes that
 * number into .rodata twice (here and in the copy inside SetRegister).  This
 * reconstruction carries far more comment than the original did, so re-base
 * the line counter rather than delete the documentation; the same constant
 * would otherwise have to be maintained by counting blank lines.  Nothing
 * else in this header uses __LINE__. */
#line 569
	void DisplayPcrtc(long long data)
	{
		int dn;

		dn = data & 1;
		assert(dn == 0 || dn == 1);
		di.DisplayPixel(dn, mem, xif);
	}

	PCRTCxif(Memory *m, char *name, int w, int h) : PCRTC(m)
	{
		XWindow *win;

		win = new XWindow(name, w, h);
		xif = win;
	}
	PCRTCxif(Memory *m, char *name, int w, int h,
		void (*func)(int, int, const unsigned int *)) : PCRTC(m)
	{
		XWindowDump *dump;

		dump = new XWindowDump;
		dump->func = func;
		dump->out.Resize(w, h);
		xif = dump;
	}

	void SetRegister(int addr, long long data);
	void Resize(int w, int h) { xif->Resize(w, h); }
};

/* The headless build.  GPU2::GPU2 makes one of these when disp_on == 0. */
class PCRTCdmy : public PCRTC {
public:
	PCRTCdmy(Memory *m) : PCRTC(m) { }
	virtual void Resize() { }
};

#endif
