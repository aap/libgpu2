/* Pre1 - GS front end stage 1.
 *
 * GPU2::Put() calls Pre1::Put() directly (non-virtually) for register
 * addresses 0x00-0x7f.  Pre1 keeps the current vertex attribute state,
 * decodes PRIM/PRMODE into individual flags, and hands vertices and
 * pass-through register writes to Pre3 through Pre3's vtable.
 *
 * Reconstructed from orig/lib/pre1.o; see doc/notes/pre1.md.
 */

#include <stdio.h>
#include <stdlib.h>

#include "pre1.h"
#include "pre3.h"

int
Reverse(int a)
{
	if (a & 0x800000)
		a &= 0x7fffff;
	else
		a |= 0x800000;
	return a;
}

Pre1::Pre1(Pre3 *p)
{
	pre3 = p;
	newprim = 0;
	CTXT = 2;
	PRIM = 0;
	FST = 0;
	AA1 = 0;
	ABE = 0;
	FGE = 0;
	TME = 0;
	IIP = 0;
	X = 0;
	Y = 0;
	Z = 0;
	RGBA = 0;
	Q[2] = 0;
	S[2] = 0;
	T[2] = 0;
	U = 0;
	V = 0;
	OFX1 = 0;
	OFY1 = 0;
	OFX2 = 0;
	OFY2 = 0;
	AC = 0;
}

void
Pre1::Put(int addr, long long data)
{
	int ctxt;

	nodraw = 0;
	switch (addr) {
	case 0x00:
		newprim = 1;
		PRIM = data & 7;
		if (AC == 1) {
			FST = (data >> 8) & 1;
			AA1 = (data >> 7) & 1;
			ABE = (data >> 6) & 1;
			FGE = (data >> 5) & 1;
			TME = (data >> 4) & 1;
			IIP = (data >> 3) & 1;
			FIX = (data >> 10) & 1;
			ctxt = (data >> 9) & 1;
			if (CTXT != ctxt) {
				CTXT = ctxt;
				SendRegister(addr, data);
			}
		}
		break;

	/* XYZF, no drawing kick.  z/f are block-scoped in every XYZ case:
	   at function scope the 1998 object's register allocation is not
	   reproduced (see doc/notes/pre1.md). */
	case 0x0a:
	    {
		int z, f;

		X = data & 0xffff;
		Y = (data >> 16) & 0xffff;
		z = (data >> 32) & 0xffffff;
		f = (data >> 56) & 0xff;
		Z = z | ((long long)f << 32);
		break;
	    }

	case 0x01:
		RGBA = data;
		Q[2] = data >> 32;
		break;

	case 0x02:
		S[2] = data;
		T[2] = data >> 32;
		break;

	case 0x03:
		U = data & 0x3fff;
		V = (data >> 16) & 0x3fff;
		break;

	case 0x18:
		OFX1 = data & 0xffff;
		OFY1 = (data >> 32) & 0xffff;
		break;

	case 0x19:
		OFX2 = data & 0xffff;
		OFY2 = (data >> 32) & 0xffff;
		break;

	case 0x04:
	    {
		int z, f;

		X = data & 0xffff;
		Y = (data >> 16) & 0xffff;
		z = (data >> 32) & 0xffffff;
		f = (data >> 56) & 0xff;
		Z = z | ((long long)f << 32);
		SendData();
		break;
	    }

	case 0x11:
		RGBA = data;
		Q[2] = data >> 32;
		SendData();
		break;

	case 0x12:
		S[2] = data;
		T[2] = data >> 32;
		SendData();
		break;

	case 0x13:
		U = data & 0x3fff;
		V = (data >> 16) & 0x3fff;
		SendData();
		break;

	case 0x0c:
	    {
		int z, f;

		X = data & 0xffff;
		Y = (data >> 16) & 0xffff;
		z = (data >> 32) & 0xffffff;
		f = (data >> 56) & 0xff;
		Z = z | ((long long)f << 32);
		nodraw = 1;
		SendData();
		break;
	    }

	case 0x05:
	    {
		int z;

		X = data & 0xffff;
		Y = (data >> 16) & 0xffff;
		z = data >> 32;
		Z = (Z & 0xff00000000LL) | (z & 0xffffffffLL);
		SendData();
		break;
	    }

	case 0x0d:
	    {
		int z;

		X = data & 0xffff;
		Y = (data >> 16) & 0xffff;
		z = data >> 32;
		Z = (Z & 0xff00000000LL) | (z & 0xffffffffLL);
		nodraw = 1;
		SendData();
		break;
	    }

	case 0x1a:
		AC = data & 1;
		break;

	case 0x1b:
		if (AC == 0) {
			FST = (data >> 8) & 1;
			AA1 = (data >> 7) & 1;
			ABE = (data >> 6) & 1;
			FGE = (data >> 5) & 1;
			TME = (data >> 4) & 1;
			IIP = (data >> 3) & 1;
			FIX = (data >> 10) & 1;
			ctxt = (data >> 9) & 1;
			if (CTXT != ctxt) {
				CTXT = ctxt;
				SendRegister(addr, data);
			}
		}
		break;

	default:
		SendRegister(addr, data);
		break;
	}
}

/* Hand the current vertex to Pre3.  XYOFFSET is applied only for the
 * duration of the call (X/Y are restored afterwards), then the S/T/Q
 * ring is rotated for the next vertex of the same primitive. */
void
Pre1::SendData()
{
	int ofx, ofy;

	send_type = 0;
	if ((CTXT & 1) == 0) {
		ofx = OFX1;
		ofy = OFY1;
	} else {
		ofx = OFX2;
		ofy = OFY2;
	}
	X -= ofx;
	Y -= ofy;
	if (FST == 0) {
		send_U = S[2] >> 8;
		send_V = T[2] >> 8;
		send_Q = Q[2] >> 8;
		maxexp = TME == 1 ? MaxExp() : 0;
	} else {
		send_U = U;
		send_V = V;
		send_Q = Q[2] >> 8;
		maxexp = TME == 1 ? MaxExp() : 0;
	}
	/* Sign-magnitude 24-bit floats: if Q is negative, flip the sign of
	   the whole S/T/Q triple so that the divider downstream sees Q>0. */
	if (send_Q & 0x800000) {
		if (FST == 0) {
			send_U = Reverse(send_U);
			send_V = Reverse(send_V);
		}
		send_Q = Reverse(send_Q);
	}
	pre3->Put(this);
	X += ofx;
	Y += ofy;
	/* Vertex ring: S/T/Q[0] and [1] are the two previous vertices,
	   [2] is the current one.  A triangle fan (PRIM 5) pins slot 0 to
	   the first vertex after PRIM instead of shifting it. */
	if (PRIM == 5) {
		if (newprim) {
			S[0] = S[2];
			T[0] = T[2];
			Q[0] = Q[2];
		} else {
			S[1] = S[2];
			T[1] = T[2];
			Q[1] = Q[2];
		}
	} else {
		S[0] = S[1];
		T[0] = T[1];
		Q[0] = Q[1];
		S[1] = S[2];
		T[1] = T[2];
		Q[1] = Q[2];
	}
	newprim = 0;
	pre3->restart = 0;
}

void
Pre1::SendRegister(int addr, long long data)
{
	send_type = 1;
	send_addr = addr;
	send_reg = data;
	pre3->Put(this);
}

/* Largest IEEE-754 exponent among the S/T/Q of the vertices that make up
 * the primitive about to be drawn (Q only when FST selects integer UV).
 * Pre3::Float2Fix() shifts every mantissa down to this common exponent.
 *
 * The two loops are written as for(;;)+break with a block-scoped variable
 * so that g++ 2.7's expand_end_loop() does not roll the exit test to the
 * bottom -- the 1998 object has it at the top.  See doc/notes/pre1.md. */
int
Pre1::MaxExp()
{
	int max, i;

	max = 0;
	switch (PRIM) {
	case 0:
		i = 2;
		break;
	case 1:
	case 2:
	case 6:
		i = 1;
		break;
	case 3:
	case 4:
	case 5:
		i = 0;
		break;
	default:
		fprintf(stderr, "PRE1:Illegal primitive type\n");
		exit(1);
	}
	if (FST == 0) {
		for (;;) {
			int e;
			if (i == 3)
				break;
			e = (S[i] >> 23) & 0xff;
			if (e > max)
				max = e;
			e = (T[i] >> 23) & 0xff;
			if (e > max)
				max = e;
			e = (Q[i] >> 23) & 0xff;
			if (e > max)
				max = e;
			i++;
		}
	} else {
		for (;;) {
			int e;
			if (i == 3)
				break;
			e = (Q[i] >> 23) & 0xff;
			if (e > max)
				max = e;
			i++;
		}
	}
	return max;
}
