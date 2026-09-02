/* libgpu2 - the public API layer of Sony's 1998 GS reference model.
 *
 * Reconstructed from orig/lib/libgpu2.o.  Everything below is pinned by
 * bytes unless a comment says otherwise; doc/notes/libgpu2.md records the
 * evidence, the residuals and the guesses.
 *
 * Build (this object alone in the archive was gcc 2.7.2.1, and i386-tuned):
 *	GCC272_ALT=rh42-2721 tools/gcc272/g++272 -O2 -m386 -Iinclude -c
 *
 * The GS_* entry points are plain C names in the 1998 symbol table, so the
 * header is included inside extern "C"; `initPCRTC' is mangled
 * (initPCRTC__Fv), which is what identifies the file as C++ at all.
 *
 * Three original bugs are reproduced deliberately and must not be "fixed"
 * here -- the point of the exercise is the 1998 object, not a better one:
 *   1. GS_OpenSim writes register 0x17 twice and 0x16 never;
 *   2. GS_SaveImage's TRXPOS write puts posx in SSAY and posy in SSAX;
 *   3. GS_SaveImage byteswaps each pixel and then fwrite()s only 3 of the
 *      4 bytes, so every pixel is shifted by one byte.
 */
#include <stdio.h>

#include "gpu2.h"

extern "C" {
#include "libgpu2.h"
}

/* All three are local (static) symbols in the 1998 object, at these exact
 * bss offsets, under these exact names -- the symbol table kept them.
 *	gpu2	+0x00	the GPU2 singleton
 *	fb	+0x04	save/display area (7 ints, 0x1c bytes)
 *	Field	+0x20	interlace flag, GS_OpenSim's 5th argument
 */
static GPU2 *gpu2;
static FRAME_BUFFER fb;
static int Field;

static void initPCRTC(void);

void
GS_InitSim(void)
{
	Field = 0;
	fb.fbp = 0;
	fb.fbw = 10;
	fb.psm = 0;
	fb.width = 640;
	fb.height = 480;
	fb.posx = 0;
	fb.posy = 0;
}

/* `field' does NOT reach the GPU2 constructor: __4GPU2Pciii takes four
 * arguments (char*, int, int, int) and GS_OpenSim passes title/width/
 * height/disp_on.  field is stashed in the bss `Field' just before
 * initPCRTC(), which is the only other thing that reads it (besides
 * GS_PutPort and GS_SaveImage).  This settles doc/STRUCTS.md's open
 * question about the Pciii/5-argument mismatch.
 */
void
GS_OpenSim(char *title, int width, int height, int disp_on, int field)
{
	gpu2 = new GPU2(title, width, height, disp_on);

	/* power-on defaults: 44 registers cleared, then the PCRTC block */
	gpu2->Put(0x1a, 0);		/* TEXCLUT */
	gpu2->Put(0x1b, 0);		/* (reserved) */
	gpu2->Put(0x0a, 0);		/* FOG */
	gpu2->Put(0x01, 0);		/* RGBAQ */
	gpu2->Put(0x02, 0);		/* ST */
	gpu2->Put(0x03, 0);		/* UV */
	gpu2->Put(0x18, 0);		/* XYOFFSET_1 */
	gpu2->Put(0x19, 0);		/* XYOFFSET_2 */
	gpu2->Put(0x22, 0);		/* SCANMSK */
	gpu2->Put(0x06, 0);		/* TEX0_1 */
	gpu2->Put(0x07, 0);		/* TEX0_2 */
	gpu2->Put(0x14, 0);		/* TEX1_1 */
	gpu2->Put(0x15, 0);		/* TEX1_2 */
	gpu2->Put(0x17, 0);		/* TEX2_2 -- should be 0x16, TEX2_1 */
	gpu2->Put(0x17, 0);		/* TEX2_2 again (original bug, kept) */
	gpu2->Put(0x1c, 0);		/* MIPTBP1_1 */
	gpu2->Put(0x34, 0);		/* MIPTBP1_2 */
	gpu2->Put(0x35, 0);		/* MIPTBP2_1 */
	gpu2->Put(0x36, 0);		/* MIPTBP2_2 */
	gpu2->Put(0x37, 0);
	gpu2->Put(0x3b, 0);		/* TEXA */
	gpu2->Put(0x08, 0);		/* CLAMP_1 */
	gpu2->Put(0x09, 0);		/* CLAMP_2 */
	gpu2->Put(0x3d, 0);		/* FOGCOL */
	gpu2->Put(0x40, 0);		/* SCISSOR_1 */
	gpu2->Put(0x41, 0);		/* SCISSOR_2 */
	gpu2->Put(0x47, 0);		/* TEST_1 */
	gpu2->Put(0x48, 0);		/* TEST_2 */
	gpu2->Put(0x42, 0);		/* ALPHA_1 */
	gpu2->Put(0x43, 0);		/* ALPHA_2 */
	gpu2->Put(0x49, 0);		/* PABE */
	gpu2->Put(0x44, 0);		/* DIMX */
	gpu2->Put(0x45, 0);		/* DTHE */
	gpu2->Put(0x46, 0);		/* COLCLAMP */
	gpu2->Put(0x4a, 0);		/* FBA_1 */
	gpu2->Put(0x4b, 0);		/* FBA_2 */
	gpu2->Put(0x4c, 0);		/* FRAME_1 */
	gpu2->Put(0x4d, 0);		/* FRAME_2 */
	gpu2->Put(0x4e, 0);		/* ZBUF_1 */
	gpu2->Put(0x4f, 0);		/* ZBUF_2 */
	gpu2->Put(0x50, 0);		/* BITBLTBUF */
	gpu2->Put(0x51, 0);		/* TRXPOS */
	gpu2->Put(0x52, 0);		/* TRXREG */
	gpu2->Put(0x53, 0);		/* TRXDIR */

	/* The PCRTC defaults are the only 64-bit constants here whose high
	 * half is non-zero, and the 1998 object materialises exactly those
	 * three into %edx/%ecx and pushes the registers, while the ones
	 * that fit in 32 bits push immediates.  A plain `long long' literal
	 * (or any constant-folded expression) pushes immediates in both
	 * cases; splitting the halves into non-constant int operands is
	 * what reproduces it, because gcc 2.7 CSE re-folds the halves into
	 * the pseudo but will not substitute a DImode CONST_DOUBLE back
	 * into a push -- while it does substitute a CONST_INT.  The braces
	 * matter: fresh pseudos per write, not one reused pair.
	 */
	{ int hi = 0x0003fd00, lo = 0x00010000;
	  gpu2->Put(0x80, ((long long)hi<<32) | lo); }	/* PMODE */
	{ int hi = 0x00000000, lo = 0x00004000;
	  gpu2->Put(0x87, ((long long)hi<<32) | lo); }	/* DISPFB1 */
	{ int hi = 0x00100180, lo = 0x00040080;
	  gpu2->Put(0x88, ((long long)hi<<32) | lo); }	/* DISPLAY1 */
	{ int hi = 0x00000000, lo = 0x00014080;
	  gpu2->Put(0x89, ((long long)hi<<32) | lo); }	/* DISPFB2 */
	{ int hi = 0x00100180, lo = 0x00080100;
	  gpu2->Put(0x8a, ((long long)hi<<32) | lo); }	/* DISPLAY2 */
	gpu2->Put(0x8e, 0);				/* BGCOLOR */

	Field = field;
	initPCRTC();
}

void
GS_CloseSim(void)
{
	delete gpu2;
}

/* Register 0x7f is remapped to 0x100, which GPU2::Put routes to
 * PCRTC::SetRegister (it takes everything > 0xff).  In field mode bit 48
 * of the data is set on the way through.
 */
void
GS_PutPort(int addr, long long data)
{
	if (addr == 0x7f) {
		addr = 0x100;
		if (Field)
			data |= 0x0001000000000000LL;
	}
	gpu2->Put(addr, data);
}

/* Privileged-register window: 0x12000000..0x120000c0 plus six addresses in
 * the 0x120010x0 page.  0x120000n0 maps to 0x80|n (PMODE 0x80 .. BGCOLOR
 * 0x8e), 0x120010n0 to 0xc0|n.  Returns 1 if the address was recognised.
 *
 * Writes to DISPFB1 (0x87) and DISPLAY1 (0x88) are snooped to keep the save
 * area in sync, which is how GS_SaveImage knows what to read back.  Note
 * the masks are narrower than the real register fields: fbp keeps 8 bits
 * where DISPFB has 9, psm 4 where DISPFB has 5.  Both are the object's,
 * not a transcription slip.
 */
int
GS_PutCtlPort(int addr, long long data)
{
	int reg;

	if (addr >= 0x12000000 && addr <= 0x120000c0 ||
	    addr == 0x12001000 || addr == 0x12001010 ||
	    addr == 0x12001040 || addr == 0x12001080 ||
	    addr == 0x12001090 || addr == 0x120010f0) {
		if (addr & 0x1000)
			reg = ((addr >> 4) & 0x7f) | 0xc0;
		else
			reg = ((addr >> 4) & 0x7f) | 0x80;
		if (reg == 0x87) {
			fb.fbp = data & 0xff;		/* DISPFB FBP  */
			fb.fbw = (data >> 9) & 0x3f;	/*        FBW  */
			fb.psm = (data >> 15) & 0xf;	/*        PSM  */
			fb.posx = (data >> 32) & 0x7ff;	/*        DBX  */
			fb.posy = (data >> 43) & 0x7ff;	/*        DBY  */
		}
		if (reg == 0x88) {
			/* DISPLAY DW/(MAGH+1) is the width in pixels */
			fb.width = ((data >> 32) & 0xfff) / (((data >> 23) & 0xf) + 1);
			fb.height = (data >> 44) & 0xfff;	/* DH */
		}
		gpu2->Put(reg, data);
		return 1;
	}
	return 0;
}

/* Read the save area back out of local memory with a local->host BitBLT and
 * dump it raw.  Two pixel formats are handled, and both are buggy:
 *
 *  psm 0 (PSMCT32) and 1 (PSMCT24): two pixels per 64-bit GPU2::Get.  Each
 *	is byteswapped whole, so an ABGR word becomes the bytes A,B,G,R in
 *	memory -- and then only the first three are written, i.e. A,B,G.
 *	The red channel is dropped and everything is shifted by a byte.
 *	(orig/lib/libgpu2-patched.a is this same object with the 3 changed
 *	to a 4, which is how the replay harness gets usable images.)
 *
 *  psm 2 (PSMCT16): four pixels per Get.  Each 5-5-5 pixel is expanded to
 *	0x00RRGGBB, then byteswapped with a three-term expression that has
 *	no >>24 term (the top byte is known zero), giving the bytes
 *	00,RR,GG,BB -- and again only three are written, so every pixel goes
 *	out as 00,RR,GG.
 *
 * Note psm 1 deliberately leaves SPSM at 0 in BITBLTBUF: 24-bit data is
 * read back as 32-bit words and the 3-byte fwrite then happens to be the
 * right length.  Any other psm programs no transfer loop at all and the
 * file is left empty.
 */
int
GS_SaveImage(char *filename)
{
	unsigned int p[4];
	FILE *fp;
	long long d;
	long long data;
	int h;
	int i;

	fp = fopen(filename, "w");
	if (fp == NULL) {
		fprintf(stderr, "Can not open %s!!\n", filename);
		return 0;
	}

	if (Field)
		h = fb.height/2;
	else
		h = fb.height;

	/* BITBLTBUF: SBP = fbp*32, SBW = fbw, SPSM = psm (0 when psm==1) */
	if (fb.psm == 1)
		data = (long long)(fb.fbp<<5) | ((long long)fb.fbw<<16);
	else
		data = (long long)(fb.fbp<<5) | ((long long)fb.fbw<<16) |
		    ((long long)fb.psm<<24);
	gpu2->Put(0x50, data);
	/* TRXPOS: posx lands in SSAY (bits 16-26) and posy in SSAX (0-10).
	 * Swapped relative to the field names -- harmless while the save
	 * area starts at 0,0, which is why it survived.  Original. */
	data = ((long long)fb.posx<<16) | (long long)fb.posy;
	gpu2->Put(0x51, data);
	data = ((long long)h<<32) | (long long)fb.width;	/* TRXREG RRW/RRH */
	gpu2->Put(0x52, data);
	gpu2->Put(0x53, 1);				/* TRXDIR local->host */

	/* the `== 0 || == 1' spelling is pinned: gcc folds it to an
	 * unsigned range test (ja), which is what the object has */
	if (fb.psm == 0 || fb.psm == 1)
		for (i = 0; i < h*fb.width/2; i++) {
			d = gpu2->Get();
			p[0] = d;
			p[1] = d>>32;
			p[0] = (p[0]<<24) | (p[0]>>24) |
			    ((p[0] & 0xff0000)>>8) | ((p[0] & 0xff00)<<8);
			p[1] = (p[1]<<24) | (p[1]>>24) |
			    ((p[1] & 0xff0000)>>8) | ((p[1] & 0xff00)<<8);
			fwrite(&p[0], 1, 3, fp);
			fwrite(&p[1], 1, 3, fp);
		}

	if (fb.psm == 2)
		for (i = 0; i < h*fb.width/4; i++) {
			d = gpu2->Get();
			p[0] = d & 0xffff;
			p[1] = (d>>16) & 0xffff;
			p[2] = (d>>32) & 0xffff;
			p[3] = (d>>48) & 0xffff;
			p[0] = ((p[0] & 0x7c00)<<9) | ((p[0] & 0x3e0)<<6) |
			    ((p[0] & 0x1f)<<3);
			p[1] = ((p[1] & 0x7c00)<<9) | ((p[1] & 0x3e0)<<6) |
			    ((p[1] & 0x1f)<<3);
			p[2] = ((p[2] & 0x7c00)<<9) | ((p[2] & 0x3e0)<<6) |
			    ((p[2] & 0x1f)<<3);
			p[3] = ((p[3] & 0x7c00)<<9) | ((p[3] & 0x3e0)<<6) |
			    ((p[3] & 0x1f)<<3);
			p[0] = (p[0]<<24) | ((p[0] & 0xff00)<<8) |
			    ((p[0] & 0xff0000)>>8);
			p[1] = (p[1]<<24) | ((p[1] & 0xff00)<<8) |
			    ((p[1] & 0xff0000)>>8);
			p[2] = (p[2]<<24) | ((p[2] & 0xff00)<<8) |
			    ((p[2] & 0xff0000)>>8);
			p[3] = (p[3]<<24) | ((p[3] & 0xff00)<<8) |
			    ((p[3] & 0xff0000)>>8);
			fwrite(&p[0], 1, 3, fp);
			fwrite(&p[1], 1, 3, fp);
			fwrite(&p[2], 1, 3, fp);
			fwrite(&p[3], 1, 3, fp);
		}

	fclose(fp);
	return 1;
}

void
GS_SetSaveImageArea(FRAME_BUFFER *fbinfo)
{
	fb.fbp = fbinfo->fbp;
	fb.fbw = fbinfo->fbw;
	fb.psm = fbinfo->psm;
	fb.posx = fbinfo->posx;
	fb.posy = fbinfo->posy;
	fb.width = fbinfo->width;
	fb.height = fbinfo->height;
}

void
GS_GetSaveImageArea(FRAME_BUFFER *fbinfo)
{
	fbinfo->fbp = fb.fbp;
	fbinfo->fbw = fb.fbw;
	fbinfo->psm = fb.psm;
	fbinfo->posx = fb.posx;
	fbinfo->posy = fb.posy;
	fbinfo->width = fb.width;
	fbinfo->height = fb.height;
}

/* Push the save area out to the CRTC as DISPFB1/DISPLAY1.  0xa00 = 2560 is
 * the full-screen width in VCK units, so magh is the horizontal zoom that
 * makes fb.width fill the screen and DW = magh*width is always 2560-ish.
 *
 * DISPFB1: FBP 0-8, FBW 9-14, PSM 15-19, DBX 32-42, DBY 43-53.
 * DISPLAY1: DX 0-11, DY 12-22, MAGH 23-26, MAGV 27-28, DW 32-43, DH 44-54.
 *
 * The two DISPLAY1 arms disagree, and that is what the object does: with
 * Field set, MAGV = Field and MAGH = magh-1; with Field clear, MAGV = 0 and
 * MAGH is the literal 4 (0x2000000 == 4<<23) rather than magh-1.  DX/DY are
 * never written at all.
 */
static void
initPCRTC(void)
{
	long long data;
	int magh, magh1;

	magh = 0xa00 / fb.width;
	magh1 = magh - 1;

	data = (long long)fb.fbp | ((long long)fb.fbw<<9) |
	    ((long long)fb.psm<<15) | ((long long)fb.posx<<32) |
	    ((long long)fb.posy<<43);
	gpu2->Put(0x87, data);

	if (Field)
		data = ((long long)(magh*fb.width)<<32) |
		    ((long long)fb.height<<44) | ((long long)Field<<27) |
		    ((long long)magh1<<23);
	else
		data = ((long long)(magh*fb.width)<<32) |
		    ((long long)fb.height<<44) | 0x2000000;
	gpu2->Put(0x88, data);
}
