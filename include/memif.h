#ifndef MEMIF_H
#define MEMIF_H

/* MemIF - the interface stage between the texture unit and local memory.
 *
 * TXM hands MemIF one 8x2 PixelStamp at a time through MemIF's single
 * virtual (`Stamp'), and MemIF runs the per-pixel back end on it:
 *
 *	AlphaTest	ATEST/AREF/AFAIL
 *	DAlphaTest	destination alpha test (DATE/DATM)
 *	DepthTest	ZTE/ZTST against the Z buffer
 *	AlphaBlend	(A - B) * C >> 7 + D, with PABE
 *	Dither		DIMX, a signed 3-bit 4x4 matrix
 *	ColorClamp	COLCLAMP: clamp to 0..255 or wrap to 8 bits
 *
 * and then writes the stamp back through FBConfig::WriteStamp and
 * ZBConfig::WriteStamp (include/memory.h).
 *
 * The same Stamp() carries register writes down the pipe (PixelStamp::type
 * != 0).  MemIF consumes ALPHA/DIMX/DTHE/COLCLAMP/PABE itself, consumes
 * *and* forwards TEST and PRIM/PRMODE, and forwards everything else to
 * Memory::SetRegister.
 *
 * Each of the six units has a live copy plus the two context copies the
 * register decoders write; `Context()' loads the live copy from
 * `ctxt'.  Layout verified against orig/lib/memif.o (sizeof 0xfc, vptr at
 * 0xf8 with one entry); see doc/notes/memif.md.
 */

#include "memory.h"

/* ATST 0..7: NEVER, ALWAYS, LESS, LEQUAL, EQUAL, GEQUAL, GREATER,
 * NOTEQUAL - all written the other way round, as AREF <op> A. */
class AlphaTest {
public:
	int ATE;		/* 0x00 */
	int ATST;		/* 0x04 */
	int AFAIL;		/* 0x08 */
	int AREF;		/* 0x0c */

	int Pass(int a);
	void ATest(PixelStamp &s);
};

/* The destination alpha test: keep the pixel only when the frame buffer's
 * alpha MSB matches DATM. */
class DAlphaTest {
public:
	int DATE;		/* 0x00 */
	int DATM;		/* 0x04 */

	void DATest(Memory *mem, PixelStamp &s);
};

/* ZTST 0..3: NEVER, ALWAYS, GEQUAL, GREATER. */
class DepthTest {
public:
	int ZTE;		/* 0x00 */
	int ZTST;		/* 0x04 */

	void ZTest(Memory *mem, PixelStamp &s);
};

/* Cout = (A - B) * C >> 7 + D.  The four selectors are 0 = source,
 * 1 = destination (the frame buffer), else 0 / FIX. */
class AlphaBlend {
public:
	int PABE;		/* 0x00  per-pixel alpha blending */
	int A;			/* 0x04 */
	int B;			/* 0x08 */
	int D;			/* 0x0c  note: D before C, as ALPHA decodes */
	int C;			/* 0x10 */
	int FIX;		/* 0x14 */

	void Blend(Memory *mem, PixelStamp &s);
};

class Dither {
public:
	int DTHE;		/* 0x00 */
	int mat[4][4];		/* 0x04  DIMX, sign extended from 3 bits */

	void Dithering(PixelStamp &s);
};

class ColorClamp {
public:
	int CLAMP;		/* 0x00  1 = clamp, else mask to 8 bits */

	void Clamp(PixelStamp &s);
};

class MemIF {
public:
	Memory *mem;		/* 0x00 */
	int ctxt;		/* 0x04 */
	AlphaTest atest;	/* 0x08  live */
	AlphaTest atestc[2];	/* 0x18  TEST_1 / TEST_2 */
	DAlphaTest datest;	/* 0x38 */
	DAlphaTest datestc[2];	/* 0x40 */
	DepthTest ztest;	/* 0x50 */
	DepthTest ztestc[2];	/* 0x58 */
	AlphaBlend blend;	/* 0x68 */
	AlphaBlend blendc[2];	/* 0x80  ALPHA_1 / ALPHA_2 */
	Dither dither;		/* 0xb0 */
	ColorClamp clamp;	/* 0xf4 */
				/* 0xf8  vptr */

	MemIF(Memory *m);
	virtual void Stamp(PixelStamp &s);
	int ReadWord(int i);
	void SetContext(Gpu2RegCtxt c);
	void SetPABE(long long data);
	void SetCOLCLAMP(long long data);
	void SetDTHE(long long data);
	void SetDIMX(long long data);
	void SetALPHA(int ctx, long long data);
	void SetTEST(int ctx, long long data);
	void Context();
};

#endif
