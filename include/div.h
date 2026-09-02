/* Reciproc - the GS reciprocal unit (used by PCalc for 1/Q).
 *
 * Two 256-entry tables, built once in the constructor from the exact
 * double-precision reciprocal, describe 1/(1+f) piecewise-linearly over
 * f in [0,1):
 *
 *   tbl0[k] = round(2^35 / (1 + (k+0.5)/256) / 2)   (the midpoint value,
 *             stored biased by -2^34 so it fits, plus the hand-tuned
 *             +9/+2 fudge terms below)
 *   tbl1[k] = round(2^25 / (1 + (k+0.5)/256)^2 / 2) (minus the slope)
 *
 * The 70 `i == N' corrections in the constructor are per-entry tweaks
 * (+2 / -2) that make the model agree with the real hardware's table.
 *
 * reciproc() normalises |x| to t = mantissa<<31, looks up entry
 * t>>23, interpolates linearly against the interval midpoint, and then
 * applies one Newton-style refinement, returning both the unrefined
 * (v0) and refined (v1) 1/x in Q32, the binary exponent (sft) and the
 * normalised mantissa (rem).
 */

class Reciproc {
public:
	long long *tbl0;	/* +0x00 */
	long long *tbl1;	/* +0x04 */

	Reciproc(void);
	void reciproc(long long x, int &sft, long long &v0, long long &v1,
		unsigned int &rem);
};
