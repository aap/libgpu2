/* param - one vertex/slope parameter set, the arithmetic PCalc works in.
 *
 * sizeof 0x50: two ints (the screen position) followed by nine 64-bit
 * fixed-point values, one per interpolated GS vertex attribute:
 *
 *   +0x00 x   +0x04 y                     (int)
 *   +0x08 z   +0x10 r   +0x18 g   +0x20 b
 *   +0x28 a   +0x30 f   +0x38 s   +0x40 t   +0x48 q
 *
 * The names are inferred from the GS attribute set (XYZ / RGBA / F /
 * STQ) and from ShiftARGBSlope(), which scales exactly the five fields
 * +0x10..+0x30 (r,g,b,a,f).  operator= copies everything *except* x and
 * y; SetXY() is the separate copy for those two.
 *
 * The whole-object operators return a param by value, so each has a
 * hidden return-slot pointer as its first argument.
 */

class param {
public:
	int x;			/* +0x00 */
	int y;			/* +0x04 */
	long long z;		/* +0x08 */
	long long r;		/* +0x10 */
	long long g;		/* +0x18 */
	long long b;		/* +0x20 */
	long long a;		/* +0x28 */
	long long f;		/* +0x30 */
	long long s;		/* +0x38 */
	long long t;		/* +0x40 */
	long long q;		/* +0x48 */

	param(void);
	param operator=(const param &p);
	void SetXY(const param &p);
	void IfMinus(param &p);
	void GetAbs(void);
	void ShiftARGBSlope(int n);
};

param operator<<(const param &p, int n);
param operator<<(const param &p, const int &n);
param operator>>(const param &p, int n);
param operator+(const param &p, const param &o);
param operator-(const param &p, const param &o);
param operator*(const param &p, const param &o);
param operator*(const param &p, int n);
param operator/(const param &p, int n);
