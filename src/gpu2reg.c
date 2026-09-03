/* gpu2reg - the jtcl command surface: one handler per GS register name,
 * plus image-file upload/save utilities.  See include/gpu2reg.h and
 * doc/notes/gpu2reg.md.
 *
 * Every register handler parses its jtcl arguments (value union at +4
 * of each 8-byte jtcl_data_t record), assembles the 64-bit register
 * datum field by field into named long long locals, and pokes the model
 * through the virtual pGPU2Reg->Put(addr, data).  Addresses 0x100
 * (display mode) and 0x101 (PCRTC merge enable) are pseudo-registers of
 * the model reachable only from here.
 *
 * Original bugs preserved: the gputex21 handler writes register 0x17
 * (TEX2_2) - copy-paste from TEX2_2, so TEX2_1 (0x16) is unreachable
 * from the console (the same family as the double-0x17 init in
 * GS_OpenSim); the Save/CLUT handlers free their new char[] buffers
 * with scalar delete; and IDTEX8Pixel is missing its `static` (it is
 * the one handler with external linkage).
 */
#include <stdio.h>
#include <stdlib.h>

#include "gpu2reg.h"
#include "drawprim.h"

static int
PRIM(int cnt, char *args, jtcl_data_t *data)
{
	long long prim = data[0].d.i & 0x7;
	long long iip  = (long long)(data[1].d.i & 0x1) << 3;
	long long tme  = (long long)(data[2].d.i & 0x1) << 4;
	long long fge  = (long long)(data[3].d.i & 0x1) << 5;
	long long abe  = (long long)(data[4].d.i & 0x1) << 6;
	long long aa1  = (long long)(data[5].d.i & 0x1) << 7;
	long long fst  = (long long)(data[6].d.i & 0x1) << 8;
	long long ctxt = (long long)(data[7].d.i & 0x1) << 9;
	long long fix  = (long long)(data[8].d.i & 0x1) << 10;

	pGPU2Reg->Put(0x00, prim|iip|tme|fge|abe|aa1|fst|ctxt|fix);
	return 0;
}

static int
XYZF(int cnt, char *args, jtcl_data_t *data)
{
	long long x = data[0].d.i & 0xffff;
	long long y = (long long)(data[1].d.i & 0xffff) << 16;
	long long z = (long long)(data[2].d.i & 0xffffff) << 32;
	long long f = (long long)(data[3].d.i & 0xff) << 56;

	pGPU2Reg->Put(0x0a, x|y|z|f);
	return 0;
}

static int
XYZF2(int cnt, char *args, jtcl_data_t *data)
{
	long long x = data[0].d.i & 0xffff;
	long long y = (long long)(data[1].d.i & 0xffff) << 16;
	long long z = (long long)(data[2].d.i & 0xffffff) << 32;
	long long f = (long long)(data[3].d.i & 0xff) << 56;

	pGPU2Reg->Put(0x04, x|y|z|f);
	return 0;
}

static int
XYZF3(int cnt, char *args, jtcl_data_t *data)
{
	long long x = data[0].d.i & 0xffff;
	long long y = (long long)(data[1].d.i & 0xffff) << 16;
	long long z = (long long)(data[2].d.i & 0xffffff) << 32;
	long long f = (long long)(data[3].d.i & 0xff) << 56;

	pGPU2Reg->Put(0x0c, x|y|z|f);
	return 0;
}

static int
XYZ2(int cnt, char *args, jtcl_data_t *data)
{
	int zv = data[2].d.i;
	long long x = data[0].d.i & 0xffff;
	long long y = (long long)(data[1].d.i & 0xffff) << 16;
	long long z = (long long)zv << 32;

	pGPU2Reg->Put(0x05, x|y|z);
	return 0;
}

static int
XYZ3(int cnt, char *args, jtcl_data_t *data)
{
	int zv = data[2].d.i;
	long long x = data[0].d.i & 0xffff;
	long long y = (long long)(data[1].d.i & 0xffff) << 16;
	long long z = (long long)zv << 32;

	pGPU2Reg->Put(0x0d, x|y|z);
	return 0;
}

static int
RGBAQ(int cnt, char *args, jtcl_data_t *data)
{
	long long r = data[0].d.i & 0xff;
	long long g = (long long)(data[1].d.i & 0xff) << 8;
	long long b = (long long)(data[2].d.i & 0xff) << 16;
	long long a = (long long)(data[3].d.i & 0xff) << 24;
	long long q = (long long)(unsigned int)FtoI(data[4].d.f) << 32;

	pGPU2Reg->Put(0x01, r|g|b|a|q);
	return 0;
}

static int
RGBAQ2(int cnt, char *args, jtcl_data_t *data)
{
	long long r = data[0].d.i & 0xff;
	long long g = (long long)(data[1].d.i & 0xff) << 8;
	long long b = (long long)(data[2].d.i & 0xff) << 16;
	long long a = (long long)(data[3].d.i & 0xff) << 24;
	long long q = (long long)(unsigned int)FtoI(data[4].d.f) << 32;

	pGPU2Reg->Put(0x11, r|g|b|a|q);
	return 0;
}

static int
ST(int cnt, char *args, jtcl_data_t *data)
{
	float tf = data[1].d.f;
	long long s = (unsigned int)FtoI(data[0].d.f);
	long long t = (long long)(unsigned int)FtoI(tf) << 32;

	pGPU2Reg->Put(0x02, s|t);
	return 0;
}

static int
ST2(int cnt, char *args, jtcl_data_t *data)
{
	float tf = data[1].d.f;
	long long s = (unsigned int)FtoI(data[0].d.f);
	long long t = (long long)(unsigned int)FtoI(tf) << 32;

	pGPU2Reg->Put(0x12, s|t);
	return 0;
}

static int
UV(int cnt, char *args, jtcl_data_t *data)
{
	long long u = data[0].d.i & 0x3fff;
	long long v = (long long)(data[1].d.i & 0x3fff) << 16;

	pGPU2Reg->Put(0x03, u|v);
	return 0;
}

static int
UV2(int cnt, char *args, jtcl_data_t *data)
{
	long long u = data[0].d.i & 0x3fff;
	long long v = (long long)(data[1].d.i & 0x3fff) << 16;

	pGPU2Reg->Put(0x13, u|v);
	return 0;
}

static int
XYOFFSET_1(int cnt, char *args, jtcl_data_t *data)
{
	long long x = data[0].d.i & 0xffff;
	long long y = (long long)(data[1].d.i & 0xffff) << 32;

	pGPU2Reg->Put(0x18, x|y);
	return 0;
}

static int
XYOFFSET_2(int cnt, char *args, jtcl_data_t *data)
{
	long long x = data[0].d.i & 0xffff;
	long long y = (long long)(data[1].d.i & 0xffff) << 32;

	pGPU2Reg->Put(0x19, x|y);
	return 0;
}

static int
PRMODECONT(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(0x1a, data[0].d.i & 0x1);
	return 0;
}

static int
PRMODE(int cnt, char *args, jtcl_data_t *data)
{
	long long iip  = (long long)(data[0].d.i & 0x1) << 3;
	long long tme  = (long long)(data[1].d.i & 0x1) << 4;
	long long fge  = (long long)(data[2].d.i & 0x1) << 5;
	long long abe  = (long long)(data[3].d.i & 0x1) << 6;
	long long aa1  = (long long)(data[4].d.i & 0x1) << 7;
	long long fst  = (long long)(data[5].d.i & 0x1) << 8;
	long long ctxt = (long long)(data[6].d.i & 0x1) << 9;
	long long fix  = (long long)(data[7].d.i & 0x1) << 10;

	pGPU2Reg->Put(0x1b, iip|tme|fge|abe|aa1|fst|ctxt|fix);
	return 0;
}

static int
SCANMSK(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(0x22, data[0].d.i & 0x3);
	return 0;
}

static int
TEX0_1(int cnt, char *args, jtcl_data_t *data)
{
	long long tbp0 = data[0].d.i & 0x3fff;
	long long tbw  = (long long)(data[1].d.i & 0x3f) << 14;
	long long psm  = (long long)(data[2].d.i & 0x3f) << 20;
	long long tw   = (long long)(data[3].d.i & 0xf) << 26;
	long long th   = (long long)(data[4].d.i & 0xf) << 30;
	long long tcc  = (long long)(data[5].d.i & 0x1) << 34;
	long long tfx  = (long long)(data[6].d.i & 0x3) << 35;
	long long cbp  = (long long)(data[7].d.i & 0x3fff) << 37;
	long long cpsm = (long long)(data[8].d.i & 0xf) << 51;
	long long csm  = (long long)(data[9].d.i & 0x1) << 55;
	long long csa  = (long long)(data[10].d.i & 0x1f) << 56;
	long long cld  = (long long)(data[11].d.i & 0x7) << 61;

	pGPU2Reg->Put(0x06, tbp0|tbw|psm|tw|th|tcc|tfx|cbp|cpsm|csm|csa|cld);
	return 0;
}

static int
TEX0_2(int cnt, char *args, jtcl_data_t *data)
{
	long long tbp0 = data[0].d.i & 0x3fff;
	long long tbw  = (long long)(data[1].d.i & 0x3f) << 14;
	long long psm  = (long long)(data[2].d.i & 0x3f) << 20;
	long long tw   = (long long)(data[3].d.i & 0xf) << 26;
	long long th   = (long long)(data[4].d.i & 0xf) << 30;
	long long tcc  = (long long)(data[5].d.i & 0x1) << 34;
	long long tfx  = (long long)(data[6].d.i & 0x3) << 35;
	long long cbp  = (long long)(data[7].d.i & 0x3fff) << 37;
	long long cpsm = (long long)(data[8].d.i & 0xf) << 51;
	long long csm  = (long long)(data[9].d.i & 0x1) << 55;
	long long csa  = (long long)(data[10].d.i & 0x1f) << 56;
	long long cld  = (long long)(data[11].d.i & 0x7) << 61;

	pGPU2Reg->Put(0x07, tbp0|tbw|psm|tw|th|tcc|tfx|cbp|cpsm|csm|csa|cld);
	return 0;
}

static int
TEXCLUT(int cnt, char *args, jtcl_data_t *data)
{
	long long cbw  = data[0].d.i & 0x3f;
	long long cou  = (long long)(data[1].d.i & 0x3f) << 6;
	long long cov  = (long long)(data[2].d.i & 0x3ff) << 12;

	pGPU2Reg->Put(0x1c, cbw|cou|cov);
	return 0;
}

static int
TEX1_1(int cnt, char *args, jtcl_data_t *data)
{
	long long lcm  = data[0].d.i & 0x3;
	long long mxl  = (long long)(data[1].d.i & 0x7) << 2;
	long long mmag = (long long)(data[2].d.i & 0x1) << 5;
	long long mmin = (long long)(data[3].d.i & 0x7) << 6;
	long long mtba = (long long)(data[4].d.i & 0x1) << 9;
	long long l    = (long long)(data[5].d.i & 0x3) << 19;
	long long k    = (long long)(data[6].d.i & 0xfff) << 32;

	pGPU2Reg->Put(0x14, lcm|mxl|mmag|mmin|mtba|l|k);
	return 0;
}

static int
TEX1_2(int cnt, char *args, jtcl_data_t *data)
{
	long long lcm  = data[0].d.i & 0x3;
	long long mxl  = (long long)(data[1].d.i & 0x7) << 2;
	long long mmag = (long long)(data[2].d.i & 0x1) << 5;
	long long mmin = (long long)(data[3].d.i & 0x7) << 6;
	long long mtba = (long long)(data[4].d.i & 0x1) << 9;
	long long l    = (long long)(data[5].d.i & 0x3) << 19;
	long long k    = (long long)(data[6].d.i & 0xfff) << 32;

	pGPU2Reg->Put(0x15, lcm|mxl|mmag|mmin|mtba|l|k);
	return 0;
}

static int
TEX2_1(int cnt, char *args, jtcl_data_t *data)
{
	long long psm  = (long long)(data[0].d.i & 0x3f) << 20;
	long long cbp  = (long long)(data[1].d.i & 0x3fff) << 37;
	long long cpsm = (long long)(data[2].d.i & 0xf) << 51;
	long long csm  = (long long)(data[3].d.i & 0x1) << 55;
	long long csa  = (long long)(data[4].d.i & 0x1f) << 56;
	long long cld  = (long long)(data[5].d.i & 0x7) << 61;

	pGPU2Reg->Put(0x17, psm|cbp|cpsm|csm|csa|cld);
	return 0;
}

static int
TEX2_2(int cnt, char *args, jtcl_data_t *data)
{
	long long psm  = (long long)(data[0].d.i & 0x3f) << 20;
	long long cbp  = (long long)(data[1].d.i & 0x3fff) << 37;
	long long cpsm = (long long)(data[2].d.i & 0xf) << 51;
	long long csm  = (long long)(data[3].d.i & 0x1) << 55;
	long long csa  = (long long)(data[4].d.i & 0x1f) << 56;
	long long cld  = (long long)(data[5].d.i & 0x7) << 61;

	pGPU2Reg->Put(0x17, psm|cbp|cpsm|csm|csa|cld);
	return 0;
}

static int
MIPTBP1_1(int cnt, char *args, jtcl_data_t *data)
{
	long long tbp1 = data[0].d.i & 0x3fff;
	long long tbw1 = (long long)(data[1].d.i & 0x3f) << 14;
	long long tbp2 = (long long)(data[2].d.i & 0x3fff) << 20;
	long long tbw2 = (long long)(data[3].d.i & 0x3f) << 34;
	long long tbp3 = (long long)(data[4].d.i & 0x3fff) << 40;
	long long tbw3 = (long long)(data[5].d.i & 0x3f) << 54;

	pGPU2Reg->Put(0x34, tbp1|tbw1|tbp2|tbw2|tbp3|tbw3);
	return 0;
}

static int
MIPTBP1_2(int cnt, char *args, jtcl_data_t *data)
{
	long long tbp1 = data[0].d.i & 0x3fff;
	long long tbw1 = (long long)(data[1].d.i & 0x3f) << 14;
	long long tbp2 = (long long)(data[2].d.i & 0x3fff) << 20;
	long long tbw2 = (long long)(data[3].d.i & 0x3f) << 34;
	long long tbp3 = (long long)(data[4].d.i & 0x3fff) << 40;
	long long tbw3 = (long long)(data[5].d.i & 0x3f) << 54;

	pGPU2Reg->Put(0x35, tbp1|tbw1|tbp2|tbw2|tbp3|tbw3);
	return 0;
}

static int
MIPTBP2_1(int cnt, char *args, jtcl_data_t *data)
{
	long long tbp4 = data[0].d.i & 0x3fff;
	long long tbw4 = (long long)(data[1].d.i & 0x3f) << 14;
	long long tbp5 = (long long)(data[2].d.i & 0x3fff) << 20;
	long long tbw5 = (long long)(data[3].d.i & 0x3f) << 34;
	long long tbp6 = (long long)(data[4].d.i & 0x3fff) << 40;
	long long tbw6 = (long long)(data[5].d.i & 0x3f) << 54;

	pGPU2Reg->Put(0x36, tbp4|tbw4|tbp5|tbw5|tbp6|tbw6);
	return 0;
}

static int
MIPTBP2_2(int cnt, char *args, jtcl_data_t *data)
{
	long long tbp4 = data[0].d.i & 0x3fff;
	long long tbw4 = (long long)(data[1].d.i & 0x3f) << 14;
	long long tbp5 = (long long)(data[2].d.i & 0x3fff) << 20;
	long long tbw5 = (long long)(data[3].d.i & 0x3f) << 34;
	long long tbp6 = (long long)(data[4].d.i & 0x3fff) << 40;
	long long tbw6 = (long long)(data[5].d.i & 0x3f) << 54;

	pGPU2Reg->Put(0x37, tbp4|tbw4|tbp5|tbw5|tbp6|tbw6);
	return 0;
}

static int
TEXA(int cnt, char *args, jtcl_data_t *data)
{
	long long ta0 = data[0].d.i & 0xff;
	long long ta1 = (long long)(data[2].d.i & 0xff) << 32;
	long long aem = (long long)(data[1].d.i & 0x1) << 15;

	pGPU2Reg->Put(0x3b, ta0|ta1|aem);
	return 0;
}

static int
CLAMP_1(int cnt, char *args, jtcl_data_t *data)
{
	long long wms  = data[0].d.i & 0x3;
	long long wmt  = (long long)(data[1].d.i & 0x3) << 2;
	long long minu = (long long)(data[2].d.i & 0x3ff) << 4;
	long long maxu = (long long)(data[3].d.i & 0x3ff) << 14;
	long long minv = (long long)(data[4].d.i & 0x3ff) << 24;
	long long maxv = (long long)(data[5].d.i & 0x3ff) << 34;

	pGPU2Reg->Put(0x08, wms|wmt|minu|maxu|minv|maxv);
	return 0;
}

static int
CLAMP_2(int cnt, char *args, jtcl_data_t *data)
{
	long long wms  = data[0].d.i & 0x3;
	long long wmt  = (long long)(data[1].d.i & 0x3) << 2;
	long long minu = (long long)(data[2].d.i & 0x3ff) << 4;
	long long maxu = (long long)(data[3].d.i & 0x3ff) << 14;
	long long minv = (long long)(data[4].d.i & 0x3ff) << 24;
	long long maxv = (long long)(data[5].d.i & 0x3ff) << 34;

	pGPU2Reg->Put(0x09, wms|wmt|minu|maxu|minv|maxv);
	return 0;
}

static int
FOGCOL(int cnt, char *args, jtcl_data_t *data)
{
	long long fcr = data[0].d.i & 0xff;
	long long fcg = (long long)(data[1].d.i & 0xff) << 8;
	long long fcb = (long long)(data[2].d.i & 0xff) << 16;

	pGPU2Reg->Put(0x3d, fcr|fcg|fcb);
	return 0;
}

static int
CACHEINVLD(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(0x3f, 0);
	return 0;
}

static int
SCISSOR_1(int cnt, char *args, jtcl_data_t *data)
{
	long long scax0 = data[0].d.i & 0x7ff;
	long long scax1 = (long long)(data[1].d.i & 0x7ff) << 16;
	long long scay0 = (long long)(data[2].d.i & 0x7ff) << 32;
	long long scay1 = (long long)(data[3].d.i & 0x7ff) << 48;

	pGPU2Reg->Put(0x40, scax0|scax1|scay0|scay1);
	return 0;
}

static int
SCISSOR_2(int cnt, char *args, jtcl_data_t *data)
{
	long long scax0 = data[0].d.i & 0x7ff;
	long long scax1 = (long long)(data[1].d.i & 0x7ff) << 16;
	long long scay0 = (long long)(data[2].d.i & 0x7ff) << 32;
	long long scay1 = (long long)(data[3].d.i & 0x7ff) << 48;

	pGPU2Reg->Put(0x41, scax0|scax1|scay0|scay1);
	return 0;
}

static int
TEST_1(int cnt, char *args, jtcl_data_t *data)
{
	long long ate   = data[0].d.i & 0x1;
	long long atst  = (long long)(data[1].d.i & 0x7) << 1;
	long long aref  = (long long)(data[2].d.i & 0xff) << 4;
	long long afail = (long long)(data[3].d.i & 0x3) << 12;
	long long date  = (long long)(data[4].d.i & 0x1) << 14;
	long long datm  = (long long)(data[5].d.i & 0x1) << 15;
	long long zte   = (long long)(data[6].d.i & 0x1) << 16;
	long long ztst  = (long long)(data[7].d.i & 0x3) << 17;

	pGPU2Reg->Put(0x47, ate|atst|aref|afail|date|datm|zte|ztst);
	return 0;
}

static int
TEST_2(int cnt, char *args, jtcl_data_t *data)
{
	long long ate   = data[0].d.i & 0x1;
	long long atst  = (long long)(data[1].d.i & 0x7) << 1;
	long long aref  = (long long)(data[2].d.i & 0xff) << 4;
	long long afail = (long long)(data[3].d.i & 0x3) << 12;
	long long date  = (long long)(data[4].d.i & 0x1) << 14;
	long long datm  = (long long)(data[5].d.i & 0x1) << 15;
	long long zte   = (long long)(data[6].d.i & 0x1) << 16;
	long long ztst  = (long long)(data[7].d.i & 0x3) << 17;

	pGPU2Reg->Put(0x48, ate|atst|aref|afail|date|datm|zte|ztst);
	return 0;
}

static int
ALPHA_1(int cnt, char *args, jtcl_data_t *data)
{
	long long a   = data[0].d.i & 0x3;
	long long b   = (long long)(data[1].d.i & 0x3) << 2;
	long long c   = (long long)(data[2].d.i & 0x3) << 4;
	long long d   = (long long)(data[3].d.i & 0x3) << 6;
	long long fix = (long long)(data[4].d.i & 0xff) << 32;

	pGPU2Reg->Put(0x42, a|b|c|d|fix);
	return 0;
}

static int
ALPHA_2(int cnt, char *args, jtcl_data_t *data)
{
	long long a   = data[0].d.i & 0x3;
	long long b   = (long long)(data[1].d.i & 0x3) << 2;
	long long c   = (long long)(data[2].d.i & 0x3) << 4;
	long long d   = (long long)(data[3].d.i & 0x3) << 6;
	long long fix = (long long)(data[4].d.i & 0xff) << 32;

	pGPU2Reg->Put(0x43, a|b|c|d|fix);
	return 0;
}

static int
PABE(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(0x49, data[0].d.i & 0x1);
	return 0;
}

static int
DIMX(int cnt, char *args, jtcl_data_t *data)
{
	int dm[4][4];
	int i, j;

	for (i = 0; ; i++) {
		if (i == 4)
			break;
		for (j = 0; ; j++) {
			int n;

			if (j == 4)
				break;
			n = i*4 + j;
			dm[i][j] = data[n].d.i;
		}
	}
	{
	long long d00 = dm[0][0] & 0x7;
	long long d01 = (long long)(dm[0][1] & 0x7) << 4;
	long long d02 = (long long)(dm[0][2] & 0x7) << 8;
	long long d03 = (long long)(dm[0][3] & 0x7) << 12;
	long long d10 = (long long)(dm[1][0] & 0x7) << 16;
	long long d11 = (long long)(dm[1][1] & 0x7) << 20;
	long long d12 = (long long)(dm[1][2] & 0x7) << 24;
	long long d13 = (long long)(dm[1][3] & 0x7) << 28;
	long long d20 = (long long)(dm[2][0] & 0x7) << 32;
	long long d21 = (long long)(dm[2][1] & 0x7) << 36;
	long long d22 = (long long)(dm[2][2] & 0x7) << 40;
	long long d23 = (long long)(dm[2][3] & 0x7) << 44;
	long long d30 = (long long)(dm[3][0] & 0x7) << 48;
	long long d31 = (long long)(dm[3][1] & 0x7) << 52;
	long long d32 = (long long)(dm[3][2] & 0x7) << 56;
	long long d33 = (long long)(dm[3][3] & 0x7) << 60;

	pGPU2Reg->Put(0x44, d00|d01|d02|d03|d10|d11|d12|d13|
	    d20|d21|d22|d23|d30|d31|d32|d33);
	}
	return 0;
}

static int
DTHE(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(0x45, data[0].d.i & 0x1);
	return 0;
}

static int
COLCLAMP(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(0x46, data[0].d.i & 0x1);
	return 0;
}

static int
FBA_1(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(0x4a, data[0].d.i & 0x1);
	return 0;
}

static int
FBA_2(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(0x4b, data[0].d.i & 0x1);
	return 0;
}

static int
FRAME_1(int cnt, char *args, jtcl_data_t *data)
{
	int msk = data[3].d.i;
	long long fbp   = data[0].d.i & 0x1ff;
	long long fbw   = (long long)(data[1].d.i & 0x3f) << 16;
	long long psm   = (long long)(data[2].d.i & 0x3f) << 24;
	long long fbmsk = (long long)msk << 32;

	pGPU2Reg->Put(0x4c, fbp|fbw|psm|fbmsk);
	return 0;
}

static int
FRAME_2(int cnt, char *args, jtcl_data_t *data)
{
	int msk = data[3].d.i;
	long long fbp   = data[0].d.i & 0x1ff;
	long long fbw   = (long long)(data[1].d.i & 0x3f) << 16;
	long long psm   = (long long)(data[2].d.i & 0x3f) << 24;
	long long fbmsk = (long long)msk << 32;

	pGPU2Reg->Put(0x4d, fbp|fbw|psm|fbmsk);
	return 0;
}

static int
ZBUF_1(int cnt, char *args, jtcl_data_t *data)
{
	long long zbp  = data[0].d.i & 0x1ff;
	long long psm  = (long long)(data[1].d.i & 0xf) << 24;
	long long zmsk = (long long)(data[2].d.i & 0x1) << 32;

	pGPU2Reg->Put(0x4e, zbp|psm|zmsk);
	return 0;
}

static int
ZBUF_2(int cnt, char *args, jtcl_data_t *data)
{
	long long zbp  = data[0].d.i & 0x1ff;
	long long psm  = (long long)(data[1].d.i & 0xf) << 24;
	long long zmsk = (long long)(data[2].d.i & 0x1) << 32;

	pGPU2Reg->Put(0x4f, zbp|psm|zmsk);
	return 0;
}

static int
BITBLTBUF(int cnt, char *args, jtcl_data_t *data)
{
	long long sbp  = data[0].d.i & 0x3fff;
	long long sbw  = (long long)(data[1].d.i & 0x3f) << 16;
	long long spsm = (long long)(data[2].d.i & 0x3f) << 24;
	long long dbp  = (long long)(data[3].d.i & 0x3fff) << 32;
	long long dbw  = (long long)(data[4].d.i & 0x3f) << 48;
	long long dpsm = (long long)(data[5].d.i & 0x3f) << 56;

	pGPU2Reg->Put(0x50, sbp|sbw|spsm|dbp|dbw|dpsm);
	return 0;
}

static int
TRXPOS(int cnt, char *args, jtcl_data_t *data)
{
	long long ssax = data[0].d.i & 0x7ff;
	long long ssay = (long long)(data[1].d.i & 0x7ff) << 16;
	long long dsax = (long long)(data[2].d.i & 0x7ff) << 32;
	long long dsay = (long long)(data[3].d.i & 0x7ff) << 48;
	long long dir  = (long long)(data[4].d.i & 0x3) << 59;

	pGPU2Reg->Put(0x51, ssax|ssay|dsax|dsay|dir);
	return 0;
}

static int
TRXREG(int cnt, char *args, jtcl_data_t *data)
{
	long long rrw = data[0].d.i & 0xfff;
	long long rrh = (long long)(data[1].d.i & 0xfff) << 32;

	pGPU2Reg->Put(0x52, rrw|rrh);
	return 0;
}

static int
TRXDIR(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(0x53, data[0].d.i & 0x3);
	return 0;
}

static int
HWREG(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(0x54,
	    (long long)(unsigned int)data[1].d.i << 32 |
	    (unsigned int)data[0].d.i);
	return 0;
}

static int
PMODE(int cnt, char *args, jtcl_data_t *data)
{
	long long en1   = data[0].d.i & 0x1;
	long long en2   = (long long)(data[1].d.i & 0x1) << 1;
	long long crtmd = (long long)(data[2].d.i & 0x7) << 2;
	long long mmod  = (long long)(data[3].d.i & 0x1) << 5;
	long long amod  = (long long)(data[4].d.i & 0x1) << 6;
	long long slbg  = (long long)(data[5].d.i & 0x1) << 7;
	long long alp   = (long long)(data[6].d.i & 0xff) << 8;
	long long nfld  = (long long)(data[7].d.i & 0x1) << 16;
	long long exvwins = (long long)(data[8].d.i & 0x3ff) << 32;
	long long exvwine = (long long)(data[9].d.i & 0x3ff) << 42;
	long long exsyncmd = (long long)(data[10].d.i & 0x1) << 52;

	pGPU2Reg->Put(0x80, en1|en2|crtmd|mmod|amod|slbg|alp|nfld|
	    exvwins|exvwine|exsyncmd);
	return 0;
}

static int
SMODE1(int cnt, char *args, jtcl_data_t *data)
{
	long long rc     = data[0].d.i & 0x7;
	long long lc     = (long long)(data[1].d.i & 0x7f) << 3;
	long long t1248  = (long long)(data[2].d.i & 0x3) << 10;
	long long slck   = (long long)(data[3].d.i & 0x1) << 12;
	long long cmod   = (long long)(data[4].d.i & 0x3) << 13;
	long long ex     = (long long)(data[5].d.i & 0x1) << 15;
	long long prst   = (long long)(data[6].d.i & 0x1) << 16;
	long long sint   = (long long)(data[7].d.i & 0x1) << 17;
	long long xpck   = (long long)(data[8].d.i & 0x1) << 18;
	long long pck2   = (long long)(data[9].d.i & 0x1) << 19;
	long long spml   = (long long)(data[10].d.i & 0x3) << 20;
	long long gcont  = (long long)(data[11].d.i & 0xf) << 22;
	long long phs    = (long long)(data[12].d.i & 0x1) << 26;
	long long phv    = (long long)(data[13].d.i & 0x1) << 27;
	long long pehs   = (long long)(data[14].d.i & 0x1) << 28;
	long long pehv   = (long long)(data[15].d.i & 0x1) << 29;
	long long pvck   = (long long)(data[16].d.i & 0x1) << 30;

	pGPU2Reg->Put(0x81, rc|lc|t1248|slck|cmod|ex|prst|sint|xpck|pck2|
	    spml|gcont|phs|phv|pehs|pehv|pvck);
	return 0;
}

static int
SMODE2(int cnt, char *args, jtcl_data_t *data)
{
	long long inter = data[0].d.i & 0x1;
	long long ffmd  = (long long)(data[1].d.i & 0x1) << 1;
	long long dpms  = (long long)(data[2].d.i & 0x3) << 2;

	pGPU2Reg->Put(0x82, inter|ffmd|dpms);
	return 0;
}

static int
SYNCH1(int cnt, char *args, jtcl_data_t *data)
{
	long long hfp = data[0].d.i & 0x7ff;
	long long hbp = (long long)(data[1].d.i & 0x7ff) << 11;
	long long hseq = (long long)(data[2].d.i & 0x3ff) << 22;
	long long hsvs = (long long)(data[3].d.i & 0x7ff) << 32;
	long long hs   = (long long)(data[4].d.i & 0x3ff) << 43;

	pGPU2Reg->Put(0x84, hfp|hbp|hseq|hsvs|hs);
	return 0;
}

static int
SYNCV(int cnt, char *args, jtcl_data_t *data)
{
	long long vfp  = data[0].d.i & 0x3ff;
	long long vfpe = (long long)(data[1].d.i & 0x3ff) << 10;
	long long vbp  = (long long)(data[2].d.i & 0x3ff) << 20;
	long long vbpe = (long long)(data[3].d.i & 0x3ff) << 32;
	long long vdp  = (long long)(data[4].d.i & 0x7ff) << 42;
	long long vs   = (long long)(data[5].d.i & 0x3ff) << 53;

	pGPU2Reg->Put(0x86, vfp|vfpe|vbp|vbpe|vdp|vs);
	return 0;
}

static int
BGCOLOR(int cnt, char *args, jtcl_data_t *data)
{
	long long r = data[0].d.i & 0xff;
	long long g = (long long)(data[1].d.i & 0xff) << 8;
	long long b = (long long)(data[2].d.i & 0xff) << 16;

	pGPU2Reg->Put(0x8e, r|g|b);
	return 0;
}

static int
DISPFB1(int cnt, char *args, jtcl_data_t *data)
{
	long long fbp = data[0].d.i & 0x1ff;
	long long fbw = (long long)(data[1].d.i & 0x3f) << 9;
	long long psm = (long long)(data[2].d.i & 0x1f) << 15;
	long long dbx = (long long)(data[3].d.i & 0x7ff) << 32;
	long long dby = (long long)(data[4].d.i & 0x7ff) << 43;

	pGPU2Reg->Put(0x87, fbp|fbw|psm|dbx|dby);
	return 0;
}

static int
DISPFB2(int cnt, char *args, jtcl_data_t *data)
{
	long long fbp = data[0].d.i & 0x1ff;
	long long fbw = (long long)(data[1].d.i & 0x3f) << 9;
	long long psm = (long long)(data[2].d.i & 0x1f) << 15;
	long long dbx = (long long)(data[3].d.i & 0x7ff) << 32;
	long long dby = (long long)(data[4].d.i & 0x7ff) << 43;

	pGPU2Reg->Put(0x89, fbp|fbw|psm|dbx|dby);
	return 0;
}

static int
DISPLAY1(int cnt, char *args, jtcl_data_t *data)
{
	long long dx   = data[0].d.i & 0xfff;
	long long dy   = (long long)(data[1].d.i & 0x7ff) << 12;
	long long magh = (long long)(data[2].d.i & 0xf) << 23;
	long long magv = (long long)(data[3].d.i & 0x3) << 27;
	long long dw   = (long long)(data[4].d.i & 0xfff) << 32;
	long long dh   = (long long)(data[5].d.i & 0x7ff) << 44;

	pGPU2Reg->Put(0x88, dx|dy|magh|magv|dw|dh);
	return 0;
}

static int
DISPLAY2(int cnt, char *args, jtcl_data_t *data)
{
	long long dx   = data[0].d.i & 0xfff;
	long long dy   = (long long)(data[1].d.i & 0x7ff) << 12;
	long long magh = (long long)(data[2].d.i & 0xf) << 23;
	long long magv = (long long)(data[3].d.i & 0x3) << 27;
	long long dw   = (long long)(data[4].d.i & 0xfff) << 32;
	long long dh   = (long long)(data[5].d.i & 0x7ff) << 44;

	pGPU2Reg->Put(0x8a, dx|dy|magh|magv|dw|dh);
	return 0;
}

static int
DISPLAY(int cnt, char *args, jtcl_data_t *data)
{
	long long mode;

	if (cnt == 1)
		mode = data[0].d.i & 0x3;
	else
		mode = (long long)(data[0].d.i & 0x3)
		    | (long long)(data[1].d.i & 0xffff) << 32
		    | (long long)(data[2].d.i & 0xffff) << 48;
	pGPU2Reg->Put(0x100, mode);
	return 0;
}

static int
EXTBUF(int cnt, char *args, jtcl_data_t *data)
{
	long long exbp   = data[0].d.i & 0x3fff;
	long long exbw   = (long long)(data[1].d.i & 0x3f) << 14;
	long long fbin   = (long long)(data[2].d.i & 0x3) << 20;
	long long wffmd  = (long long)(data[3].d.i & 0x1) << 22;
	long long emoda  = (long long)(data[4].d.i & 0x3) << 23;
	long long emodc  = (long long)(data[5].d.i & 0x3) << 25;
	long long wdx    = (long long)(data[6].d.i & 0x7ff) << 32;
	long long wdy    = (long long)(data[7].d.i & 0x7ff) << 43;

	pGPU2Reg->Put(0x8b, exbp|exbw|fbin|wffmd|emoda|emodc|wdx|wdy);
	return 0;
}

static int
EXTDATA(int cnt, char *args, jtcl_data_t *data)
{
	long long sx   = data[0].d.i & 0xfff;
	long long sy   = (long long)(data[1].d.i & 0x7ff) << 12;
	long long smph = (long long)(data[2].d.i & 0xf) << 23;
	long long smpv = (long long)(data[3].d.i & 0x3) << 27;
	long long ww   = (long long)(data[4].d.i & 0xfff) << 32;
	long long wh   = (long long)(data[5].d.i & 0x7ff) << 44;

	pGPU2Reg->Put(0x8c, sx|sy|smph|smpv|ww|wh);
	return 0;
}

static int
EXTWRITE(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(0x8d, 0);
	return 0;
}

static int
PCRTC(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(0x101, data[0].d.i & 0x1);
	return 0;
}

static int
SaveRGB24Pixel(int cnt, char *args, jtcl_data_t *data)
{
	int x = data[0].d.i;
	int y = data[1].d.i;
	int w = data[2].d.i;
	int h = data[3].d.i;
	char *buf = new char[w*h*3];
	char *p = buf;
	int n, i, j;
	long long v;
	ImageData img;

	{
		long long sx = x & 0x7ff;
		long long sy = (long long)(y & 0x7ff) << 16;

		pGPU2Reg->Put(0x51, sx|sy);
	}
	{
		long long rw = w & 0xfff;
		long long rh = (long long)(h & 0xfff) << 32;

		pGPU2Reg->Put(0x52, rw|rh);
	}
	pGPU2Reg->Put(0x53, 1);

	n = w * h;
	n = n*3 + 7;
	n = n / 8;
	for (i = 0; i != n; i++) {
		v = pGPU2Reg->Get();
		for (j = 0; j != 8; j++) {
			*p++ = v;
			v >>= 8;
		}
	}

	strcpy(img.name, data[4].d.s);
	img.type = 2;
	img.format = 1;
	img.width = w;
	img.height = h;
	img.pixel = (unsigned char *)buf;
	SaveImageFile(&img);
	delete buf;
	return 0;
}

static int
SaveRGBA32Pixel(int cnt, char *args, jtcl_data_t *data)
{
	int x = data[0].d.i;
	int y = data[1].d.i;
	int w = data[2].d.i;
	int h = data[3].d.i;
	int n = w * h;
	char *buf = new char[n*4];
	char *p = buf;
	int i, j;
	long long v;
	ImageData img;

	{
		long long sx = x & 0x7ff;
		long long sy = (long long)(y & 0x7ff) << 16;

		pGPU2Reg->Put(0x51, sx|sy);
	}
	{
		long long rw = w & 0xfff;
		long long rh = (long long)(h & 0xfff) << 32;

		pGPU2Reg->Put(0x52, rw|rh);
	}
	pGPU2Reg->Put(0x53, 1);

	n = n / 2;
	for (i = 0; i != n; i++) {
		v = pGPU2Reg->Get();
		for (j = 0; j != 8; j++) {
			*p++ = v;
			v >>= 8;
		}
	}

	strcpy(img.name, data[4].d.s);
	img.type = 2;
	img.format = 2;
	img.width = w;
	img.height = h;
	img.pixel = (unsigned char *)buf;
	SaveImageFile(&img);
	delete buf;
	return 0;
}

static int
SaveCRT(int cnt, char *args, jtcl_data_t *data)
{
	int w = data[0].d.i;
	int h = data[1].d.i;
	int chan;
	int v;
	unsigned char a;
	ImageData img;

	if (cnt <= 3)
		chan = 0;
	else
		chan = data[3].d.i;

	int n = w * h;
	char *buf = new char[n*4];
	char *p = buf;
	int i;

	if (chan == 1) {
		for (i = 0; i != n; i++) {
			v = pGPU2Reg->GetCRT();
			a = (unsigned)v >> 24;
			p[0] = a;
			p[1] = a;
			p[2] = a;
			p[3] = a;
			p += 4;
		}
	} else {
		for (i = 0; i != n; i++) {
			v = pGPU2Reg->GetCRT();
			p[0] = v;
			p[1] = (unsigned)v >> 8;
			p[2] = (unsigned)v >> 16;
			p[3] = (unsigned)v >> 24;
			p += 4;
		}
	}

	strcpy(img.name, data[2].d.s);
	img.type = 2;
	img.format = 2;
	img.width = w;
	img.height = h;
	img.pixel = (unsigned char *)buf;
	SaveImageFile(&img);
	delete buf;
	return 0;
}

static int
OpenImage(ImageData *img, char *name)
{
	strcpy(img->name, name);
	if (!OpenImageFile(img)) {
		sprintf(tcl_ip->result, "can not open %s\n", name);
		return 1;
	}
	if (img->format == 3) {
		ImageData tmp;

		ConvImage8to24(img, &tmp);
		FreeImage(img);
		*img = tmp;
	}
	return 0;
}

static int
RGB24Pixel(int cnt, char *args, jtcl_data_t *data)
{
	ImageData img;
	long long d;
	long long pv;
	int pix;
	int i, j;

	if (OpenImage(&img, data[0].d.s) == 1)
		return 1;
	{
		long long rw = img.width & 0xfff;
		long long rh = (long long)(img.height & 0xfff) << 32;

		d = rw|rh;
	}
	pGPU2Reg->Put(0x52, d);
	pGPU2Reg->Put(0x53, 0);

	int bpp = 3;
	if (img.format == 2)
		bpp = 4;
	int count = 0;
	unsigned char *p = img.pixel;
	for (i = img.height; i != 0; i--)
		for (j = img.width; j != 0; j--, count++, p += bpp) {
			pix = p[0] | p[1] << 8 | p[2] << 16;
			pv = pix;
			switch (count % 8) {
			case 0:
				d = pv;
				break;
			case 1:
				d |= pv << 24;
				break;
			case 2:
				d |= pv << 48;
				pGPU2Reg->Put(0x54, d);
				d = (unsigned char)(pv >> 16);
				break;
			case 3:
				d |= pv << 8;
				break;
			case 4:
				d |= pv << 32;
				break;
			case 5:
				d |= pv << 56;
				pGPU2Reg->Put(0x54, d);
				d = (unsigned short)(pv >> 8);
				break;
			case 6:
				d |= pv << 16;
				break;
			case 7:
				d |= pv << 40;
				pGPU2Reg->Put(0x54, d);
				break;
			}
		}
	if (count & 0x7)
		pGPU2Reg->Put(0x54, d);
	FreeImage(&img);
	return 0;
}

static void
PutRGBA32(ImageData *img)
{
	long long d = 0;
	int count;
	int i, j;

	pGPU2Reg->Put(0x53, 0);
	if (img->format == 1) {
		unsigned char *p = img->pixel;

		count = 0;
		for (i = img->height; i != 0; i--)
			for (j = img->width; j != 0; j--, count++, p += 3) {
				int pix = p[0] | p[1] << 8 | p[2] << 16;
				long long pv = pix;

				if (count & 1) {
					d |= pv << 32;
					pGPU2Reg->Put(0x54, d);
				} else
					d = pv;
			}
	} else {
		unsigned char *p = img->pixel;

		count = 0;
		for (i = img->height; i != 0; i--)
			for (j = img->width; j != 0; j--, count++, p += 4) {
				int pix = p[0] | p[1] << 8 | p[2] << 16 |
				    p[3] << 24;
				long long pv = pix;

				if (count & 1) {
					d |= pv << 32;
					pGPU2Reg->Put(0x54, d);
				} else
					d = pv;
			}
	}
	if (count & 1)
		pGPU2Reg->Put(0x54, d);
}

static int
RGBA32Pixel(int cnt, char *args, jtcl_data_t *data)
{
	ImageData img;

	if (OpenImage(&img, data[0].d.s) == 1)
		return 1;
	{
		long long rw = img.width & 0xfff;
		long long rh = (long long)(img.height & 0xfff) << 32;

		pGPU2Reg->Put(0x52, rw|rh);
	}
	PutRGBA32(&img);
	FreeImage(&img);
	return 0;
}

static void
PutRGBA16(ImageData *img)
{
	long long d = 0;
	int count;
	int i, j;

	pGPU2Reg->Put(0x53, 0);
	if (img->format == 1) {
		unsigned char *p = img->pixel;

		count = 0;
		for (i = img->height; i != 0; i--)
			for (j = img->width; j != 0; j--, count++, p += 3) {
				int pix = p[0] >> 3 | (p[1] >> 3) << 5 |
				    (p[2] >> 3) << 10;
				long long pv = pix;
				int sh = count % 4 * 16;

				d = (d & ~((long long)0xffff << sh))
				    | pv << sh;
				if (count % 4 == 3)
					pGPU2Reg->Put(0x54, d);
			}
	} else {
		unsigned char *p = img->pixel;

		count = 0;
		for (i = img->height; i != 0; i--)
			for (j = img->width; j != 0; j--, count++, p += 4) {
				int pix = p[0] >> 3 | (p[1] >> 3) << 5 |
				    (p[2] >> 3) << 10 | (p[3] >> 7) << 15;
				long long pv = pix;
				int sh = count % 4 * 16;

				d = (d & ~((long long)0xffff << sh))
				    | pv << sh;
				if (count % 4 == 3)
					pGPU2Reg->Put(0x54, d);
			}
	}
	if (count & 0x3)
		pGPU2Reg->Put(0x54, d);
}

static int
RGBA16Pixel(int cnt, char *args, jtcl_data_t *data)
{
	ImageData img;

	if (OpenImage(&img, data[0].d.s) == 1)
		return 1;
	{
		long long rw = img.width & 0xfff;
		long long rh = (long long)(img.height & 0xfff) << 32;

		pGPU2Reg->Put(0x52, rw|rh);
	}
	PutRGBA16(&img);
	FreeImage(&img);
	return 0;
}

static void
PutIDX8(ImageData *img)
{
	long long d = 0;
	long long pv;
	int sh;
	int i, j;

	pGPU2Reg->Put(0x53, 0);
	unsigned char *p = img->pixel;
	int count = 0;
	for (i = img->height; i != 0; i--)
		for (j = img->width; j != 0; j--, count++, p++) {
			pv = *p;
			sh = count % 8 * 8;
			d = (d & ~((long long)0xff << sh))
			    | pv << sh;
			if (count % 8 == 7)
				pGPU2Reg->Put(0x54, d);
		}
	if (count & 0x7)
		pGPU2Reg->Put(0x54, d);
}

int
IDTEX8Pixel(int cnt, char *args, jtcl_data_t *data)
{
	ImageData img;
	ImageData idx;

	if (OpenImage(&img, data[0].d.s) == 1)
		return 1;
	if (img.format == 2) {
		sprintf(tcl_ip->result, "CHANNEL_ARGB is not supported yet.\n");
		return 1;
	}
	ConvImage24to8(&img, &idx, 0x100, 1);
	{
		long long rw = idx.width & 0xfff;
		long long rh = (long long)(idx.height & 0xfff) << 32;

		pGPU2Reg->Put(0x52, rw|rh);
	}
	PutIDX8(&idx);
	FreeImage(&img);
	FreeImage(&idx);
	return 0;
}

static void
PutIDX4(ImageData *img)
{
	long long d = 0;
	long long pv;
	int sh;
	int i, j;

	pGPU2Reg->Put(0x53, 0);
	unsigned char *p = img->pixel;
	int count = 0;
	for (i = img->height; i != 0; i--)
		for (j = img->width; j != 0; j--, count++, p++) {
			pv = *p;
			sh = count % 16 * 4;
			d = (d & ~((long long)0xf << sh))
			    | pv << sh;
			if (count % 16 == 15)
				pGPU2Reg->Put(0x54, d);
		}
	if (count & 0xf)
		pGPU2Reg->Put(0x54, d);
}

static int
IDTEX4Pixel(int cnt, char *args, jtcl_data_t *data)
{
	ImageData img;
	ImageData idx;

	if (OpenImage(&img, data[0].d.s) == 1)
		return 1;
	if (img.format == 2) {
		sprintf(tcl_ip->result, "CHANNEL_ARGB is not supported yet.\n");
		return 1;
	}
	ConvImage24to8(&img, &idx, 0x10, 1);
	{
		long long rw = idx.width & 0xfff;
		long long rh = (long long)(idx.height & 0xfff) << 32;

		pGPU2Reg->Put(0x52, rw|rh);
	}
	PutIDX4(&idx);
	FreeImage(&img);
	FreeImage(&idx);
	return 0;
}

static int
CLUTRGBA32Pixel(int cnt, char *args, jtcl_data_t *data)
{
	ImageData img;
	ImageData idx;
	unsigned char *clut;
	unsigned char *p;
	int i, j, e;

	if (OpenImage(&img, data[0].d.s) == 1)
		return 1;
	if (img.format == 2) {
		sprintf(tcl_ip->result, "CHANNEL_ARGB is not supported yet.\n");
		return 1;
	}
	if (data[1].d.i == 3) {
		ConvImage24to8(&img, &idx, 0x100, 1);
		clut = (unsigned char *)new char[0x300];
		p = clut;
		for (i = 0; i != 16; i++)
			for (j = 0; j != 16; j++) {
				e = i/2*32 + i%2*8 + j + (j - j%8);
				*p++ = idx.r[e];
				*p++ = idx.g[e];
				*p++ = idx.b[e];
			}
		pGPU2Reg->Put(0x52, 16 | (long long)16 << 32);
		idx.width = 16;
		idx.height = 16;
	} else {
		ConvImage24to8(&img, &idx, 0x10, 1);
		clut = (unsigned char *)new char[0x30];
		p = clut;
		for (i = 0; i != 16; i++) {
			*p++ = idx.r[i];
			*p++ = idx.g[i];
			*p++ = idx.b[i];
		}
		pGPU2Reg->Put(0x52, 8 | (long long)2 << 32);
		idx.width = 8;
		idx.height = 2;
	}
	FreeImage(&img);
	FreeImage(&idx);
	idx.pixel = clut;
	idx.format = 1;
	PutRGBA32(&idx);
	delete clut;
	return 0;
}

static int
CLUTRGBA16Pixel(int cnt, char *args, jtcl_data_t *data)
{
	ImageData img;
	ImageData idx;
	unsigned char *clut;
	unsigned char *p;
	int i, j, e;

	if (OpenImage(&img, data[0].d.s) == 1)
		return 1;
	if (img.format == 2) {
		sprintf(tcl_ip->result, "CHANNEL_ARGB is not supported yet.\n");
		return 1;
	}
	if (data[1].d.i == 3) {
		ConvImage24to8(&img, &idx, 0x100, 1);
		clut = (unsigned char *)new char[0x300];
		if (data[2].d.i == 0) {
			p = clut;
			for (i = 0; i != 16; i++)
				for (j = 0; j != 16; j++) {
					e = i/2*32 + i%2*8 + j + (j - j%8);
					*p++ = idx.r[e];
					*p++ = idx.g[e];
					*p++ = idx.b[e];
				}
			pGPU2Reg->Put(0x52, 16 | (long long)16 << 32);
			idx.width = 16;
			idx.height = 16;
		} else {
			p = clut;
			for (i = 0; i != 256; i++) {
				*p++ = idx.r[i];
				*p++ = idx.g[i];
				*p++ = idx.b[i];
			}
			pGPU2Reg->Put(0x52, 256 | (long long)1 << 32);
			idx.width = 256;
			idx.height = 1;
		}
	} else {
		ConvImage24to8(&img, &idx, 0x10, 1);
		clut = (unsigned char *)new char[0x30];
		p = clut;
		for (i = 0; i != 16; i++) {
			*p++ = idx.r[i];
			*p++ = idx.g[i];
			*p++ = idx.b[i];
		}
		{
			long long sz;

			if (data[2].d.i == 0) {
				sz = 8 | (long long)2 << 32;
				idx.width = 8;
				idx.height = 2;
			} else {
				sz = 16 | (long long)1 << 32;
				idx.width = 16;
				idx.height = 1;
			}
			pGPU2Reg->Put(0x52, sz);
		}
	}
	FreeImage(&img);
	FreeImage(&idx);
	idx.pixel = clut;
	idx.format = 1;
	PutRGBA16(&idx);
	delete clut;
	return 0;
}

static int
GPU2File(int cnt, char *args, jtcl_data_t *data)
{
	int addr;
	unsigned int dh, dl;
	FILE *fp;

	fp = fopen(data[0].d.s, "r");
	if (fp == 0) {
		sprintf(tcl_ip->result, "%s: can't open", data[0].d.s);
		return 1;
	}
	while (fscanf(fp, "%x %x %x", &addr, &dh, &dl) != EOF) {
		while (fgetc(fp) != '\n')
			;
		pGPU2Reg->Put(addr, (long long)dh << 32 | dl);
	}
	return 0;
}

static int
Gpu2Reg(int cnt, char *args, jtcl_data_t *data)
{
	pGPU2Reg->Put(data[0].d.i,
	    (long long)(unsigned int)data[1].d.i << 32 |
	    (unsigned int)data[2].d.i);
	return 0;
}

static int
Quit(int cnt, char *args, jtcl_data_t *data)
{
	exit(0);
}

jtcl_cmd_t MyCBFuncs[] = {
	{ "gpuprim",		PRIM,		"9I",		0 },
	{ "gpuxyzf",		XYZF,		"4I",		0 },
	{ "gpuxyzf2",		XYZF2,		"4I",		0 },
	{ "gpuxyzf3",		XYZF3,		"4I",		0 },
	{ "gpuxyz2",		XYZ2,		"3I",		0 },
	{ "gpuxyz3",		XYZ3,		"3I",		0 },
	{ "gpurgbaq",		RGBAQ,		"4IF",		0 },
	{ "gpurgbaq2",		RGBAQ2,		"4IF",		0 },
	{ "gpust",		ST,		"2F",		0 },
	{ "gpust2",		ST2,		"2F",		0 },
	{ "gpuuv",		UV,		"2I",		0 },
	{ "gpuuv2",		UV2,		"2I",		0 },
	{ "gpuxyoffset1",	XYOFFSET_1,	"2I",		0 },
	{ "gpuxyoffset2",	XYOFFSET_2,	"2I",		0 },
	{ "gpuprmodecont",	PRMODECONT,	"I",		0 },
	{ "gpuprmode",		PRMODE,		"8I",		0 },
	{ "gpuscanmsk",		SCANMSK,	"I",		0 },
	{ "gputex01",		TEX0_1,		"12I",		0 },
	{ "gputex02",		TEX0_2,		"12I",		0 },
	{ "gputexclut",		TEXCLUT,	"3I",		0 },
	{ "gputex11",		TEX1_1,		"7I",		0 },
	{ "gputex12",		TEX1_2,		"7I",		0 },
	{ "gputex21",		TEX2_1,		"6I",		0 },
	{ "gputex22",		TEX2_2,		"6I",		0 },
	{ "gpumiptbp11",	MIPTBP1_1,	"6I",		0 },
	{ "gpumiptbp12",	MIPTBP1_2,	"6I",		0 },
	{ "gpumiptbp21",	MIPTBP2_1,	"6I",		0 },
	{ "gpumiptbp22",	MIPTBP2_2,	"6I",		0 },
	{ "gputexa",		TEXA,		"3I",		0 },
	{ "gpuclamp1",		CLAMP_1,	"6I",		0 },
	{ "gpuclamp2",		CLAMP_2,	"6I",		0 },
	{ "gpufogcol",		FOGCOL,		"3I",		0 },
	{ "gpucacheinvld",	CACHEINVLD,	0,		0 },
	{ "gpuscissor1",	SCISSOR_1,	"4I",		0 },
	{ "gpuscissor2",	SCISSOR_2,	"4I",		0 },
	{ "gputest1",		TEST_1,		"8I",		0 },
	{ "gputest2",		TEST_2,		"8I",		0 },
	{ "gpualpha1",		ALPHA_1,	"5I",		0 },
	{ "gpualpha2",		ALPHA_2,	"5I",		0 },
	{ "gpupabe",		PABE,		"I",		0 },
	{ "gpudimx",		DIMX,		"16I",		0 },
	{ "gpudthe",		DTHE,		"I",		0 },
	{ "gpucolclamp",	COLCLAMP,	"I",		0 },
	{ "gpufba1",		FBA_1,		"I",		0 },
	{ "gpufba2",		FBA_2,		"I",		0 },
	{ "gpuframe1",		FRAME_1,	"4I",		0 },
	{ "gpuframe2",		FRAME_2,	"4I",		0 },
	{ "gpuzbuf1",		ZBUF_1,		"3I",		0 },
	{ "gpuzbuf2",		ZBUF_2,		"3I",		0 },
	{ "gpubitbltbuf",	BITBLTBUF,	"6I",		0 },
	{ "gputrxpos",		TRXPOS,		"5I",		0 },
	{ "gputrxreg",		TRXREG,		"2I",		0 },
	{ "gputrxdir",		TRXDIR,		"I",		0 },
	{ "gpuhwreg",		HWREG,		"2I",		0 },
	{ "gpupmode",		PMODE,		"11I",		0 },
	{ "gpusmode1",		SMODE1,		"17I",		0 },
	{ "gpusmode2",		SMODE2,		"3I",		0 },
	{ "gpusynch1",		SYNCH1,		"5I",		0 },
	{ "gpusyncv",		SYNCV,		"6I",		0 },
	{ "gpubgcolor",		BGCOLOR,	"3I",		0 },
	{ "gpudispfb1",		DISPFB1,	"5I",		0 },
	{ "gpudispfb2",		DISPFB2,	"5I",		0 },
	{ "gpudisplay1",	DISPLAY1,	"6I",		0 },
	{ "gpudisplay2",	DISPLAY2,	"6I",		0 },
	{ "gpudisplay",		DISPLAY,	"(I)|(3I)",	0 },
	{ "gpuextbuf",		EXTBUF,		"8I",		0 },
	{ "gpuextdata",		EXTDATA,	"6I",		0 },
	{ "gpuextwrite",	EXTWRITE,	"",		0 },
	{ "gpupcrtc",		PCRTC,		"I",		0 },
	{ "savecrt",		SaveCRT,	"(2IS)|(2ISI)",	0 },
	{ "gpurgb24pixel",	RGB24Pixel,	"S",		0 },
	{ "gpurgba32pixel",	RGBA32Pixel,	"S",		0 },
	{ "gpurgba16pixel",	RGBA16Pixel,	"S",		0 },
	{ "gpuclutrgba32pixel",	CLUTRGBA32Pixel, "SI",		0 },
	{ "gpuclutrgba16pixel",	CLUTRGBA16Pixel, "S2I",		0 },
	{ "gpuidtex8pixel",	IDTEX8Pixel,	"S",		0 },
	{ "gpuidtex4pixel",	IDTEX4Pixel,	"S",		0 },
	{ "savergb24",		SaveRGB24Pixel,	"4IS",		0 },
	{ "savergba32",		SaveRGBA32Pixel, "4IS",		0 },
	{ "gpufile",		GPU2File,	"S",		0 },
	{ "gpureg",		Gpu2Reg,	"3I",		0 },
	{ "quit",		Quit,		0,		0 },
	{ 0,			0,		0,		0 }
};

GPU2Reg *pGPU2Reg = 0;

GPU2Reg::GPU2Reg(void)
{
	RegisterCommands(MyCBFuncs);
	grfwSwitchTriangleRasterlizer(DrawTriangle);
	grfwSwitchVertex(Vertex0, Vertex1, Vertex2);
	pGPU2Reg = this;
}
