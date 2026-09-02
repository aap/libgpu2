#ifndef MEMORY_H
#define MEMORY_H

/* Memory - the 4 MB of GS local memory and everything that addresses it.
 *
 * One Memory object is allocated by GPU2::GPU2 (0x4001c8 bytes, GPU2+0x10;
 * doc/STRUCTS.md).  It holds
 *
 *   - the VRAM itself, an array of 0x100000 words at offset 0;
 *   - three FBConfig's: the *active* frame buffer plus the two context
 *     copies FRAME_1/FRAME_2 decode into;
 *   - three ZBConfig's, likewise for ZBUF_1/ZBUF_2;
 *   - the BitBLT transfer engine (include/bitblt.h).
 *
 * FBConfig and ZBConfig are the pixel accessors: they derive from AddrConv
 * (include/addrcalc.h - it has state, and no empty-base optimisation
 * exists in g++ 2.7, so the members below really do start at 0x20), and
 * read and write one pixel or one 8x2 PixelStamp at a time.
 *
 * Memory::SetRegister is the tail of the register path: MemIF::Stamp
 * forwards every write it does not consume itself here, and this is where
 * FRAME/ZBUF/FBA/TEST/PRIM land, and where the whole BITBLTBUF / TRXPOS /
 * TRXREG / TRXDIR / HWREG transfer machine is driven.
 *
 * Layouts verified against orig/lib/memory.o and orig/lib/memif.o; see
 * doc/notes/memory.md.
 */

#include "bitblt.h"

/* GS register numbers, as they travel down the pipe.  Only the ones
 * memory.o and memif.o decode are named.  The 1998 headers did have an
 * enum for the two contexts - MemIF::SetContext's mangled name carries it
 * (`SetContext__5MemIF11Gpu2RegCtxt') - so that one keeps its real name;
 * the register numbers are #defines because nothing in the archive pins
 * what they were called. */
enum Gpu2RegCtxt {
	Gpu2RegCtxt1,		/* 0 */
	Gpu2RegCtxt2		/* 1 */
};

#define GS_PRIM		0x00
#define GS_PRMODE	0x1b
#define GS_TEXFLUSH	0x3f
#define GS_ALPHA_1	0x42
#define GS_ALPHA_2	0x43
#define GS_DIMX		0x44
#define GS_DTHE		0x45
#define GS_COLCLAMP	0x46
#define GS_TEST_1	0x47
#define GS_TEST_2	0x48
#define GS_PABE		0x49
#define GS_FBA_1	0x4a
#define GS_FBA_2	0x4b
#define GS_FRAME_1	0x4c
#define GS_FRAME_2	0x4d
#define GS_ZBUF_1	0x4e
#define GS_ZBUF_2	0x4f
#define GS_BITBLTBUF	0x50
#define GS_TRXPOS	0x51
#define GS_TRXREG	0x52
#define GS_TRXDIR	0x53
#define GS_HWREG	0x54
#define GS_FIELD	0x7f

class Memory;

/* A pixel colour as the pipeline carries it: four 0..255 ints, not packed.
 * Passed to FBConfig::WritePixel *by value* - four stack words. */
struct PixColor {		/* 0x10 */
	int R;			/* 0x00 */
	int G;			/* 0x04 */
	int B;			/* 0x08 */
	int A;			/* 0x0c */
};

/* One of the sixteen pixels of a stamp.  Only the fields memory.o and
 * memif.o touch are named; the rest belong to DDA/TXM. */
struct Pixel {			/* 0x38 */
	PixColor c;		/* 0x00  colour */
	int m_10;		/* 0x10 */
	int m_14;		/* 0x14 */
	unsigned int z;		/* 0x18  Z, clamped to the ZBUF depth */
	int m_1c;		/* 0x1c */
	int m_20;		/* 0x20 */
	int m_24;		/* 0x24 */
	int m_28;		/* 0x28 */
	int m_2c;		/* 0x2c */
	int pass;		/* 0x30  alpha test passed */
	int afail;		/* 0x34  AFAIL when it did not */
};

/* The stamp's position.  An 8-byte record, so g++ 2.7 gives it DImode and
 * copies it with two loads and two stores - which is how memif.o's three
 * ReadStamp callers set up their scratch stamp. */
struct StampPos {		/* 0x08 */
	int x;			/* 0x00  left column of the top row */
	int y;			/* 0x04  row pair: real y is y*2 and y*2+1 */
};

/* The 8x2 pixel quantum the whole back end works on.  The same object is
 * also the carrier for register writes travelling down the pipe: type != 0
 * means "this is register `reg' = `data'", type == 0 means pixels. */
class PixelStamp {		/* 0x3cc */
public:
	int type;		/* 0x000  0 = pixels, else a register write */
	int reg;		/* 0x004 */
	long long data;		/* 0x008 */
	int m_10;		/* 0x010 */
	StampPos pos;		/* 0x014 */
	int mask;		/* 0x01c  which of the 16 pixels are live */
	int m_20;		/* 0x020 */
	int m_24;		/* 0x024 */
	int aamask;		/* 0x028  antialias-only pixels */
	int m_2c;		/* 0x02c */
	int m_30;		/* 0x030 */
	int m_34;		/* 0x034 */
	int ABE;		/* 0x038 */
	int m_3c;		/* 0x03c */
	int m_40;		/* 0x040 */
	int m_44;		/* 0x044 */
	int ctxt;		/* 0x048 */
	Pixel pix[16];		/* 0x04c */

	/* Defined `inline' at the *end* of memif.c and txm.c, so the uses
	 * in this header's clients compile to a call and the body comes
	 * out last in .text as a weak symbol - which is exactly what
	 * memif.o and txm.o carry.  memory.c never calls it, so memory.o
	 * carries no copy. */
	inline int AAMask() const;
};

/* FRAME_1/FRAME_2 + FBA_1/FBA_2, decoded.  Three of these live in Memory:
 * the active one at +0x400000 and the two context copies behind it. */
class FBConfig : public AddrConv {
public:
	int FBP;		/* 0x20  FBP * 2048: a word address */
	int FBW;		/* 0x24  FBW * 64: pixels */
	int PSM;		/* 0x28 */
	int FBMSK;		/* 0x2c  1 bits are *not* written */
	int FBA;		/* 0x30  the alpha correction bit */

	PixColor ReadPixel(Memory *mem, int x, int y);
	void ReadStamp(Memory *mem, PixelStamp &s);
	void WritePixel(Memory *mem, int x, int y, PixColor c,
		int awrite, int fbp, int fbw);
	void WriteStamp(Memory *mem, PixelStamp &s);
};

/* ZBUF_1/ZBUF_2 plus the ZTE bit of TEST_1/TEST_2. */
class ZBConfig : public AddrConv {
public:
	int ZBP;		/* 0x20  ZBP * 2048 */
	int ZBW;		/* 0x24  copied from FRAME's FBW */
	int PSM;		/* 0x28  0x30/0x31/0x32/0x3a */
	int ZMSK;		/* 0x2c  1 = do not write */
	int mask;		/* 0x30  the usable Z bits for that PSM */
	int ZTE;		/* 0x34  TEST.ZTE */

	unsigned ReadZ(Memory *mem, int x, int y);
	void ReadStamp(Memory *mem, PixelStamp &s);
	void WriteZ(Memory *mem, int x, int y, unsigned z);
	void WriteStamp(Memory *mem, PixelStamp &s);
};

class Memory {
public:
	int vram[0x100000];	/* 0x000000  4 MB of local memory */
	FBConfig fb;		/* 0x400000  the active context */
	FBConfig fb1;		/* 0x400034 */
	FBConfig fb2;		/* 0x400068 */
	ZBConfig zb;		/* 0x40009c  the active context */
	ZBConfig zb1;		/* 0x4000d4 */
	ZBConfig zb2;		/* 0x40010c */
	/* The active-context flag is Memory+0x4001c4 and is used *only*
	 * here, so it is really Memory's own `int ctxt' sitting behind an
	 * 0x80-byte BitBLT.  include/bitblt.h (owned by the bitblt drop,
	 * and never reading the field) carries it as BitBLT::m_80 instead;
	 * `bitblt.m_80' below and a trailing `int ctxt' assemble to the
	 * same address, and to the same sizeof 0x4001c8. */
	BitBLT bitblt;		/* 0x400144 (+0x80 = the context flag) */

	void SetRegister(int addr, long long data);
};

#endif
