#ifndef BITBLT_H
#define BITBLT_H

/* BitBLT - the local memory transfer engine (BITBLTBUF/TRXPOS/TRXREG/
 * TRXDIR/HWREG).
 *
 * One BitBLT object sits at the end of the Memory allocation
 * (Memory+0x400144, sizeof 0x84 - doc/STRUCTS.md); memory.o's
 * Memory::SetRegister decodes the transfer registers into it and drives it:
 *
 *   TRXDIR 0  host  -> local   pixels arrive through WritePixel(mem, data)
 *   TRXDIR 1  local -> host    pixels leave through ReadPixel(mem)
 *   TRXDIR 2  local -> local   DoBitBLT(mem) copies the rectangle at once
 *   TRXDIR 3  deactivated
 *
 * read()/write() are the pixel-granular accessors underneath all three, and
 * the only things here that touch AddrConv.  The pack/unpack state (x,
 * count, phase, save) lives in the object because a host transfer arrives
 * 64 bits at a time and PSMCT24 pixels straddle those boundaries.
 *
 * This header is the one every VRAM client includes: clut.o, bitblt.o,
 * memif.o, memory.o, txm.o, pcrtc.o and gpu2.o all open their .rodata with
 * the two strings the inline register setters below carry, and all carry an
 * unreferenced undefined `memcpy' from the one that calls it.  g++ 2.7
 * builds RTL for an inline member as soon as it parses it, so its string
 * constants and libcall externals land in every including object even when
 * the function is never called - which is how those two messages reached
 * objects with no transfer code at all.  The two setters are therefore
 * reconstructed only as far as those strings: nothing in the archive emits
 * their bodies, so nothing pins them.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "addrcalc.h"

class Memory;

/* Bits per pixel of a PSM, from its low three bits.  Shared by bitblt.c and
 * clut.c; both objects expand the five stores of the table at their use
 * sites, so it was a header inline in 1998 too.  The element type has to be
 * an aggregate: with a plain `int bit[]' g++ 2.7 folds the subscript into
 * the addressing mode, and the originals compute the base separately.  See
 * doc/notes/clut.md. */
struct PixBit { int bit; };

inline int
Depth(int psm)
{
	PixBit bit[] = { {32}, {24}, {16}, {8}, {4} };

	return bit[psm & 7].bit;
}

class BitBLT : public AddrConv {
public:
	int SBP;		/* 0x20  BITBLTBUF, a word address (SBP*64) */
	int SBW;		/* 0x24  pixels (SBW*64) */
	int DBP;		/* 0x28 */
	int DBW;		/* 0x2c */
	int SPSM;		/* 0x30 */
	int DPSM;		/* 0x34 */
	int SSAX;		/* 0x38  TRXPOS */
	int SSAY;		/* 0x3c */
	int DSAX;		/* 0x40 */
	int DSAY;		/* 0x44 */
	int m_48;		/* 0x48  written by Memory::SetRegister */
	int m_4c;		/* 0x4c */
	int m_50;		/* 0x50 */
	int m_54;		/* 0x54 */
	int DIR;		/* 0x58  TRXPOS.DIR */
	int RRW;		/* 0x5c  TRXREG */
	int RRH;		/* 0x60  counted down as the transfer runs */
	int m_64;		/* 0x64 */
	int m_68;		/* 0x68 */
	int TRXDIR;		/* 0x6c */
	int count;		/* 0x70  pixels left in the current row */
	int x;			/* 0x74  current column */
	int phase;		/* 0x78  24 bit packing phase; Memory drives it */
	int save;		/* 0x7c  leftover bits of a straddling pixel */
	int m_80;		/* 0x80 */

	void SetBITBLTBUF(long long data) {
		if (SPSM != DPSM) {
			fprintf(stderr, "BITBLTBUF: Depth is different\n");
			exit(1);
		}
	}
	void SetHWREG(long long data, char *buf, int len) {
		if (TRXDIR != 0)
			fprintf(stderr, "HWREG:Now not Host to Local mode\n");
		memcpy(buf, (char*)&data, len);
	}

	unsigned read(Memory *mem, int x, int y);
	void write(Memory *mem, unsigned data, int x, int y);
	void DoBitBLT(Memory *mem);
	void WritePixel(Memory *mem, long long data);
	long long ReadPixel(Memory *mem);
};

#endif
