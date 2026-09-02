/* AddrConv - GS local memory address decomposition.
 *
 * Stateless base class of FBConfig/ZBConfig (and others); turns a pixel
 * position into the model's memory coordinates:
 *
 *   page  8 KB page number (includes the buffer base)
 *   blk   bit 6 of the in-page column index (inverted for the Z formats -
 *         the Z block arrangement is the color one with the halves swapped)
 *   bnk   bit 6 ^ bit 5 of the column index
 *   pos   low 5 bits of the column index
 *   wd    word group 0-3 inside the column (the 2x4 interleave table)
 *   np    nibble position 0-31 inside the 128-bit word group
 *
 * Callers rebuild a word address as
 *   page<<11 | blk<<10 | bnk<<9 | pos<<4 | wd<<2 | np>>3
 * (see FBConfig::ReadPixel) so the effective block index bits are
 * blk = bx2', bnk^blk = by1', reproducing the by1^bx2 parity the replay
 * harness measured against retail (gsreplay notes).
 */

class AddrConv {
public:
	void address_convert(int x, int y, int psm, int bw, int tbp,
		int &page, int &blk, int &bnk, int &pos, int &wd, int &np);
};
