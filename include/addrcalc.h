#ifndef ADDRCALC_H
#define ADDRCALC_H

/* AddrConv, with the state its users actually share.
 *
 * TexClut (clut.o), BitBLT (bitblt.o) and FBConfig/ZBConfig (memory.o) all
 * begin with the same eight ints, all pass `&this->page' ... `&this->np' to
 * address_convert(), and all recompose the word address from them with the
 * same six-term sum:
 *
 *      addr   = page<<11 + blk<<10 + bnk<<9 + pos<<4 + wd*4 + np>>3
 *      bitpos = (np & 7) * 4                  bit offset inside the word
 *
 * That only works if the fields live in AddrConv itself and every user
 * derives from it - the derived members start at offset 0, which they do
 * in all three objects, and an *empty* AddrConv base would push them to 4
 * (g++ 2.7 has no empty-base optimisation; measured).
 *
 * include/addrconv.h is owned by the addrconv work and declares AddrConv
 * stateless (true of address_convert() itself, which touches no member),
 * so this file carries the fuller declaration instead.  Include ONE of the
 * two, never both: this one supersedes it.  See doc/notes/clut.md.
 */

class AddrConv {
public:
	int addr;		/* 0x00  word address in local memory */
	int page;		/* 0x04 */
	int blk;		/* 0x08 */
	int bnk;		/* 0x0c */
	int pos;		/* 0x10 */
	int wd;			/* 0x14 */
	int np;			/* 0x18 */
	int bitpos;		/* 0x1c  bit offset of the pixel in the word */

	void address_convert(int x, int y, int psm, int bw, int tbp,
		int &page, int &blk, int &bnk, int &pos, int &wd, int &np);

	void Address(int x, int y, int psm, int bw, int tbp) {
		address_convert(x, y, psm, bw/64, tbp/64,
			page, blk, bnk, pos, wd, np);
		addr = (page<<11) + (blk<<10) + (bnk<<9) + (pos<<4)
			+ wd*4 + (np>>3);
		bitpos = (np & 7)*4;
	}
};

#endif
