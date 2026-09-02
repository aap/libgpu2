#ifndef CLUT_H
#define CLUT_H

/* TexClut - the texture CLUT (palette) cache.
 *
 * One TexClut lives inside TXM (at TXM+0x258, sizeof 0x498 - see
 * doc/notes/clut.md); TXM keeps the decoded TEX0/TEXCLUT CLUT fields in
 * three ClutAttr's, hands the active one to LoadData() and reads palette
 * entries back with Lookup().
 *
 * The cache holds 256 words.  For a 32-bit CPSM that is 256 entries, one
 * per word; for a 16-bit CPSM it is 512 entries, entry 0..255 in the low
 * half of a word and 256..511 in the high half.
 */

#include "addrcalc.h"

class Memory;

/* Stand-in for memif.o's class - clut.c only needs MemIF+0x00 to be the
 * Memory*.  Guarded so a real memif.h can take over later. */
#ifndef MEMIF_DECLARED
#define MEMIF_DECLARED
class MemIF {
public:
	Memory *mem;		/* 0x00 */
};
#endif

/* The CLUT half of TEX0 plus TEXCLUT, decoded by TXM.  9 ints; TXM copies
 * whole ClutAttr's around with a 9-word block move (SetTEXCLUT). */
struct ClutAttr {
	int CBP;		/* 0x00  TEX0 CBP * 64: a word address */
	int CBW;		/* 0x04  TEXCLUT CBW * 64: pixels */
	int PSM;		/* 0x08  the *texture* PSM - picks 16 or 256 */
	int CPSM;		/* 0x0c  0 = PSMCT32, else 16 bit */
	int CSM;		/* 0x10  0 = CSM1, 1 = CSM2 */
	int COU;		/* 0x14  TEXCLUT COU * 16 */
	int COV;		/* 0x18  TEXCLUT COV */
	int CSA;		/* 0x1c  TEX0 CSA * 16: an entry index */
	int CLD;		/* 0x20 */
};

class TexClut : public AddrConv {
public:
	MemIF *memif;		/* 0x020 */
	int clut[256];		/* 0x024 */
	int cbp0;		/* 0x424  CLD 2/4 remembered CBP */
	int cbp1;		/* 0x428  CLD 3/5 remembered CBP */
	ClutAttr attr;		/* 0x42c  the active one */
	ClutAttr attr1;		/* 0x450  context 1 */
	ClutAttr attr2;		/* 0x474  context 2 */

	void load1(ClutAttr &a);
	void load2(ClutAttr &a);
	void LoadData(ClutAttr &a);
	int Lookup(int i);
};

#endif
