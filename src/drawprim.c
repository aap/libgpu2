/* drawprim - grfw immediate-mode vertex kick helpers.  See
 * include/drawprim.h and doc/notes/drawprim.md.
 *
 * PutVertex converts one _grfwVertex into the RGBAQ / ST / XYZF2
 * register writes of the 1998 vertex-kick protocol: color is truncated
 * to integer (alpha rescaled 0..255 -> 0..128), S/T/Q pass their float
 * bits through untouched (FtoI), X/Y are converted to 12.4 fixed point,
 * Z to 24 bits, fog to 8.  Vertex0 opens a primitive: PRIM gets tristrip
 * (4) when type==1, else trifan (5), with IIP/TME/FGE/AA1 from flag
 * bits 0-3 (ABE, bit 6 of PRIM, is never driven).  DrawLine/DrawTriangle
 * are the parked grfw plug-in slots.
 */
#include <stdio.h>

#include "gpu2reg.h"
#include "drawprim.h"

void
DrawLine(int n, _grfwVertex *v0, _grfwVertex *v1)
{
	static int already;

	if (!already) {
		printf("You must write DrawLine routine!\n");
		already = 1;
	}
}

static void
PutVertex(int tme, _grfwVertex *v)
{
	int r = (int)v->r;
	int g = (int)v->g;
	int b = (int)v->b;
	int a = (int)v->a;
	long long data;

	a = (a << 7) / 255;
	data = (long long)(r & 0xff)
	    | (long long)(g & 0xff) << 8
	    | (long long)(b & 0xff) << 16
	    | (long long)(a & 0xff) << 24
	    | (long long)(unsigned int)FtoI(v->q) << 32;
	pGPU2Reg->Put(0x01, data);

	if (tme) {
		float t = v->t;

		data = (long long)(unsigned int)FtoI(v->s)
		    | (long long)(unsigned int)FtoI(t) << 32;
		pGPU2Reg->Put(0x02, data);
	}

	{
		int x = (int)(16 * v->x);
		int y = (int)(16 * v->y);
		int z = (int)v->z;
		int f = (int)v->f;

		data = (long long)(x & 0xffff)
		    | (long long)(y & 0xffff) << 16
		    | (long long)(z & 0xffffff) << 32
		    | (long long)(f & 0xff) << 56;
		pGPU2Reg->Put(0x04, data);
	}
}

void
Vertex0(int type, int flag, _grfwVertex *v)
{
	int iip = 0;
	int tme, fge, aa1;
	int prim;

	if (flag & 1)
		iip++;
	tme = (unsigned int)flag >> 1 & 1;
	fge = (unsigned int)flag >> 2 & 1;
	aa1 = (unsigned int)flag >> 3 & 1;
	prim = 5;
	if (type == 1)
		prim = 4;
	pGPU2Reg->Put(0x00, (long long)(prim & 0x7)
	    | (long long)(iip & 0x1) << 3
	    | (long long)(tme & 0x1) << 4
	    | (long long)(fge & 0x1) << 5
	    | (long long)(aa1 & 0x1) << 7);
	PutVertex(tme, v);
}

void
Vertex1(int type, int flag, _grfwVertex *v)
{
	PutVertex((unsigned int)flag >> 1 & 1, v);
}

void
Vertex2(int type, int flag, _grfwVertex *v)
{
	PutVertex((unsigned int)flag >> 1 & 1, v);
}

void
DrawTriangle(int n, _grfwVertex *v0, _grfwVertex *v1, _grfwVertex *v2)
{
}
