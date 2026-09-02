/* NormTexCoord - the texture-coordinate perspective divide.
 *
 * TexDiv(u, q, f) normalises u and q to sign/exponent/15-bit-mantissa
 * form and produces 1/q from a 128-entry piecewise-linear table:
 *
 *   rm = (OFFSET_TBL[m>>8] - ((m & 0xff) * SLOPE_TBL[m>>8] >> 6)) >> 1
 *
 * where m is q's mantissa.  The two tables are built once, at the first
 * InitTable(), by mktable(): for each of the 128 mantissa buckets it
 * searches 100 offsets (a small downward sweep of the exact 1/x value)
 * and keeps the (offset, slope) pair minimising the worst-case error
 * over the 256 mantissa values in the bucket - evalute()/cal_y() are
 * that error metric, cal_y being the fixed-point evaluation of the
 * line and evalute the max |cal_y/65536 - 1/x| over the bucket.
 */

class NormTexCoord {
public:
	int sign;		/* +0x00  sign of u */
	int qzero;		/* +0x04  set when the f argument is nonzero */
	int um;			/* +0x08  mantissa of |u| */
	int rm;			/* +0x0c  mantissa of 1/q */
	int ue;			/* +0x10  exponent of |u| */
	int re;			/* +0x14  exponent of 1/q */

	static int OFFSET_TBL[128];
	static int SLOPE_TBL[128];
	static int table_init;

	void TexDiv(int u, int q, int f);
	void InitTable(void);
	void mktable(void);
	double evalute(int x, int off, int slope);
	int cal_y(int x, int off, int slope);
};
