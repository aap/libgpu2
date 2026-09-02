/* TexFunc - the GS texture function stage (TEX0.TFX / TEX0.TCC).
 *
 * Func(t, f) applies texture colour `t' to fragment colour `f' in
 * place; `tcc' is TEX0.TCC (0 = RGB only, 1 = RGBA).  All results are
 * clamped to 0..255; the fixed-point product is >>7, so 0x80 is 1.0.
 *
 *   func 0 MODULATE   f = t*f>>7                (a = t.a*f.a>>7 if tcc)
 *   func 1 DECAL      f = t                     (a = t.a if tcc else f.a)
 *   func 2 HIGHLIGHT  f = (t*f>>7) + f.a        (a = t.a + f.a if tcc)
 *   func 3 HIGHLIGHT2 f = (t*f>>7) + f.a        (a = t.a if tcc else f.a)
 *
 * PixColor is the 4-int colour record shared with TXM/MemIF.
 */

class PixColor {
public:
	int r;			/* +0x00 */
	int g;			/* +0x04 */
	int b;			/* +0x08 */
	int a;			/* +0x0c */
};

class TexFunc {
public:
	int func;		/* +0x00  TEX0.TFX */
	int unk4;		/* +0x04 */
	int unk8;		/* +0x08 */
	int tcc;		/* +0x0c  TEX0.TCC */

	void Func(PixColor &t, PixColor &f);
};
