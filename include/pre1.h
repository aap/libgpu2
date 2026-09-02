/* Pre1 - GS front end stage 1: register decode / vertex assembly.
 *
 * Layout verified against orig/lib/pre1.o (sizeof 0xac, no vtable);
 * field names follow aap's IDA export where they were already known.
 */

class Pre3;

class Pre1 {
public:
	Pre3 *pre3;		/* 0x00 */
	int OFX1, OFY1;		/* 0x04 0x08  XYOFFSET_1 */
	int OFX2, OFY2;		/* 0x0c 0x10  XYOFFSET_2 */
	int S[3];		/* 0x14 0x18 0x1c */
	int T[3];		/* 0x20 0x24 0x28 */
	int Q[3];		/* 0x2c 0x30 0x34 */
	int U, V;		/* 0x38 0x3c */
	int AC;			/* 0x40  PRMODECONT */
	int X, Y;		/* 0x44 0x48 */
	long long Z;		/* 0x4c  Z in [23:0], F in [39:32] */
	int RGBA;		/* 0x54 */
	int send_addr;		/* 0x58 */
	long long send_reg;	/* 0x5c */
	int send_U;		/* 0x64 */
	int send_V;		/* 0x68 */
	int send_Q;		/* 0x6c */
	int maxexp;		/* 0x70 */
	int send_type;		/* 0x74  0 = vertex data, 1 = register */
	int PRIM;		/* 0x78 */
	int nodraw;		/* 0x7c  set by XYZF3/XYZ3 */
	int CTXT;		/* 0x80 */
	int FST;		/* 0x84 */
	int AA1;		/* 0x88 */
	int ABE;		/* 0x8c */
	int FGE;		/* 0x90 */
	int TME;		/* 0x94 */
	int IIP;		/* 0x98 */
	int FIX;		/* 0x9c */
	int newprim;		/* 0xa0  first vertex after PRIM */
	int m_a4;		/* 0xa4 */
	int m_a8;		/* 0xa8 */

	Pre1(Pre3 *p);
	void Put(int addr, long long data);
	void SendData();
	void SendRegister(int addr, long long data);
	int MaxExp();
};
