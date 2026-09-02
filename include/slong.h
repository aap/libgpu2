/* slong - 96-bit signed-magnitude fixed-point accumulator.
 *
 * The value represented is  sign * (hi * 2^32 + lo)  with lo kept in
 * [0, 2^32) by masking against `mask' (= 0xffffffff, set by the ctor).
 * Multiply() forms an exact 64x64 product in that split form; Combine()
 * folds it back into a single (truncating) long long.
 *
 * Used by PCalc for the perspective/slope arithmetic.
 */

class slong {
public:
	unsigned long long hi;		/* +0x00 */
	unsigned long long lo;		/* +0x08 */
	int sign;			/* +0x10 */
	unsigned long long mask;	/* +0x14 */

	slong(void) { mask = 0xffffffff; }
	slong Multiply(long long a, long long b);
	slong operator=(slong s);
	long long Combine(void);
};

slong operator>>(slong a, int i);
slong operator<<(slong a, int i);
