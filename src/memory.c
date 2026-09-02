#include <stdio.h>
#include <stdlib.h>

#include "gsdefs.h"
#include "memory.h"
#include "dbg.h"

/* Load the active context.  Every register that has a per-context copy
 * ends with this, and g++ 2.7 cross-jumps the identical tails back
 * together, which is why memory.o has only two copies of each block.
 *
 * `bitblt.m_80' is Memory+0x4001c4, the active-context flag - semantically
 * Memory's own field sitting behind an 0x80-byte BitBLT, but carried in
 * include/bitblt.h (which never reads it).  See include/memory.h. */
#define SETCONTEXT()			\
	if (bitblt.m_80 == 0)		\
		fb = fb1;		\
	else				\
		fb = fb2;		\
	if (bitblt.m_80 == 0)		\
		zb = zb1;		\
	else				\
		zb = zb2;

/* One pixel out of the frame buffer, unpacked into a PixColor.
 *
 * `bp' and `a' are forced by the bytes: the object keeps the bit position
 * in a pseudo across the inlined Depth() table (which is what the extra
 * stack slot is), and runs the alpha through a register rather than
 * storing the two constants straight into c.A.  See doc/notes/memory.md. */
PixColor
FBConfig::ReadPixel(Memory *mem, int x, int y)
{
	PixColor c;
	unsigned data;
	int bp, a;

	Address(x, y, PSM, FBW, FBP);
	bp = bitpos;
	data = mem->vram[addr];
	if (Depth(PSM) == 16) {
		if (bp != 0)
			data >>= 16;
		data &= 0xffff;
		a = (short)data < 0 ? 0x80 : 0;
		c.A = a;
		c.R = (data & 0x1f) << 3;
		data >>= 5;
		c.G = (data & 0x1f) << 3;
		c.B = (data >> 2) & 0xf8;
	} else {
		if (Depth(PSM) == 24)
			data = data & 0xffffff | 0x80000000;
		c.R = data & 0xff;
		c.G = (data >> 8) & 0xff;
		c.B = (data >> 16) & 0xff;
		c.A = data >> 24;
	}
	return c;
}

/* The colours of a whole 8x2 stamp.  x steps away from the stamp's own
 * parity, i.e. an odd base column runs right to left. */
void
FBConfig::ReadStamp(Memory *mem, PixelStamp &s)
{
	int i, x, x0, y, xstep, mask;

	x0 = s.pos.x;
	x = x0;
	y = s.pos.y*2;
	xstep = 1;
	if (x & 1)
		xstep = -1;
	mask = s.mask;
	i = 0;
	for (;;) {
		PixColor c;

		if (i == 8)
			break;
		if (mask & 1<<i)
			s.pix[i].c = c = ReadPixel(mem, x, y);
		i++;
		x += xstep;
	}
	x = x0;
	y++;
	for (;;) {
		PixColor c;

		if (i == 16)
			break;
		if (mask & 1<<i)
			s.pix[i].c = c = ReadPixel(mem, x, y);
		i++;
		x += xstep;
	}
}

/* One pixel into the frame buffer.  fbp/fbw come from the caller instead
 * of from this->FBP/FBW; awrite == 0 adds alpha to FBMSK (AFAIL 3, the
 * RGB-only arm). */
void
FBConfig::WritePixel(Memory *mem, int x, int y, PixColor c,
	int awrite, int fbp, int fbw)
{
	unsigned data, pix, mask;

	Address(x, y, PSM, fbw, fbp);
	DbgWatch(0, x, y);
	if (Depth(PSM) == 16) {
		pix = (((c.A >> 7) & 1 | FBA) << 15) | ((c.B >> 3) << 10) |
			((c.G >> 3) << 5) | (c.R >> 3);
		data = mem->vram[addr];
		if (bitpos != 0) {
			pix = pix & ~FBMSK | (data >> 16) & FBMSK;
			pix = data & 0xffff | pix << 16;
		} else {
			pix = pix & ~FBMSK | data & FBMSK;
			pix = pix | data & 0xffff0000;
		}
	} else {
		pix = FBA << 7 | c.A;
		if (awrite != 0)
			mask = FBMSK;
		else
			mask = FBMSK | 0xff000000;
		data = mem->vram[addr];
		pix = pix << 24 | c.B << 16 | c.G << 8 | c.R;
		pix = pix & ~mask | data & mask;
		if (PSM == PSMCT24 || PSM == PSMZ24)
			pix = data & 0xff000000 | pix & 0xffffff;
	}
	mem->vram[addr] = pix;
}

/* A whole stamp back into the frame buffer.  A pixel is written when the
 * alpha test passed, or when it failed with AFAIL == FB_ONLY (1) or
 * AFAIL == RGB_ONLY (3) - the last one with alpha masked off. */
void
FBConfig::WriteStamp(Memory *mem, PixelStamp &s)
{
	int i, x, x0, y, xstep, mask;

	x0 = s.pos.x;
	x = x0;
	y = s.pos.y*2;
	xstep = 1;
	if (x & 1)
		xstep = -1;
	mask = s.mask;
	i = 0;
	for (;;) {
		PixColor c;

		if (i == 8)
			break;
		if (mask & 1<<i) {
			if (s.pix[i].pass != 0) {
				c = s.pix[i].c;
				WritePixel(mem, x, y, c, 1, FBP, FBW);
			} else if (s.pix[i].afail == 1) {
				c = s.pix[i].c;
				WritePixel(mem, x, y, c, 1, FBP, FBW);
			} else if (s.pix[i].afail == 3) {
				c = s.pix[i].c;
				WritePixel(mem, x, y, c, 0, FBP, FBW);
			}
		}
		i++;
		x += xstep;
	}
	x = x0;
	y++;
	for (;;) {
		PixColor c;

		if (i == 16)
			break;
		if (mask & 1<<i) {
			if (s.pix[i].pass != 0) {
				c = s.pix[i].c;
				WritePixel(mem, x, y, c, 1, FBP, FBW);
			} else if (s.pix[i].afail == 1) {
				c = s.pix[i].c;
				WritePixel(mem, x, y, c, 1, FBP, FBW);
			} else if (s.pix[i].afail == 3) {
				c = s.pix[i].c;
				WritePixel(mem, x, y, c, 0, FBP, FBW);
			}
		}
		i++;
		x += xstep;
	}
}

/* One Z out of the Z buffer, right justified. */
unsigned
ZBConfig::ReadZ(Memory *mem, int x, int y)
{
	unsigned data;
	long long mask;

	Address(x, y, PSM, ZBW, ZBP);
	data = mem->vram[addr];
	data >>= bitpos;
	mask = (long long)1 << Depth(PSM);
	return data & (mask - 1);
}

void
ZBConfig::ReadStamp(Memory *mem, PixelStamp &s)
{
	int i, x, x0, y, xstep, mask;

	x0 = s.pos.x;
	x = x0;
	y = s.pos.y*2;
	xstep = 1;
	if (x & 1)
		xstep = -1;
	mask = s.mask;
	i = 0;
	for (;;) {
		Pixel *p;

		if (i == 8)
			break;
		if (mask & 1<<i) {
			p = &s.pix[i];
			p->z = ReadZ(mem, x, y);
		}
		i++;
		x += xstep;
	}
	x = x0;
	y++;
	for (;;) {
		Pixel *p;

		if (i == 16)
			break;
		if (mask & 1<<i) {
			p = &s.pix[i];
			p->z = ReadZ(mem, x, y);
		}
		i++;
		x += xstep;
	}
}

/* One Z into the Z buffer. */
void
ZBConfig::WriteZ(Memory *mem, int x, int y, unsigned z)
{
	unsigned data, mask;
	long long m;

	if (ZMSK == 1)
		return;
	Address(x, y, PSM, ZBW, ZBP);
	data = mem->vram[addr];
	m = (long long)1 << Depth(PSM);
	mask = (m - 1) << bitpos;
	z <<= bitpos;
	z &= mask;
	data &= ~mask;
	mem->vram[addr] = z | data;
}

/* A whole stamp into the Z buffer.  Antialias-only pixels never update Z,
 * and the pixel goes in when the alpha test passed or failed with
 * AFAIL == ZB_ONLY (2). */
void
ZBConfig::WriteStamp(Memory *mem, PixelStamp &s)
{
	int i, x, x0, y, xstep, mask, aamask;

	if (ZMSK == 1 || ZTE == 0)
		return;
	x0 = s.pos.x;
	x = x0;
	y = s.pos.y*2;
	xstep = 1;
	if (x & 1)
		xstep = -1;
	mask = s.mask;
	aamask = s.aamask;
	i = 0;
	for (;;) {
		if (i == 8)
			break;
		if ((aamask & 1<<i) == 0)
			if (mask & 1<<i)
				if (s.pix[i].pass != 0 || s.pix[i].afail == 2)
					WriteZ(mem, x, y, s.pix[i].z);
		i++;
		x += xstep;
	}
	x = x0;
	y++;
	for (;;) {
		if (i == 16)
			break;
		if ((aamask & 1<<i) == 0)
			if (mask & 1<<i)
				if (s.pix[i].pass != 0 || s.pix[i].afail == 2)
					WriteZ(mem, x, y, s.pix[i].z);
		i++;
		x += xstep;
	}
}

/* The tail of the register path: everything MemIF did not consume. */
void
Memory::SetRegister(int addr, long long data)
{
	int w, h, psm;

	switch (addr) {
	case GS_FBA_1:
		fb1.FBA = data & 1;
		SETCONTEXT();
		break;

	case GS_FBA_2:
		fb2.FBA = data & 1;
		SETCONTEXT();
		break;

	case GS_FRAME_1:
		fb1.FBP = (data & 0x1ff) << 11;
		fb1.FBW = ((data >> 16) & 0x3f) << 6;
		fb1.PSM = (data >> 24) & 0x3f;
		fb1.FBMSK = data >> 32;
		if (Depth((data >> 24) & 0x3f) == 16)
			fb1.FBMSK = ((fb1.FBMSK >> 3) & 0x1f) |
				((fb1.FBMSK >> 6) & 0x3e0) |
				((fb1.FBMSK >> 9) & 0x7c00) |
				((fb1.FBMSK >> 16) & 0x8000);
		zb1.ZBW = ((data >> 16) & 0x3f) << 6;
		SETCONTEXT();
		break;

	case GS_FRAME_2:
		fb2.FBP = (data & 0x1ff) << 11;
		fb2.FBW = ((data >> 16) & 0x3f) << 6;
		fb2.PSM = (data >> 24) & 0x3f;
		fb2.FBMSK = data >> 32;
		if (Depth((data >> 24) & 0x3f) == 16)
			fb2.FBMSK = ((fb2.FBMSK >> 3) & 0x1f) |
				((fb2.FBMSK >> 6) & 0x3e0) |
				((fb2.FBMSK >> 9) & 0x7c00) |
				((fb2.FBMSK >> 16) & 0x8000);
		zb2.ZBW = ((data >> 16) & 0x3f) << 6;
		SETCONTEXT();
		break;

	case GS_ZBUF_1:
		zb1.ZBP = (data & 0x1ff) << 11;
		psm = (data >> 24) & 0xf;
		switch (psm) {
		case 0:
			zb1.PSM = PSMZ32;
			zb1.mask = 0xffffffff;
			break;
		case 1:
			zb1.PSM = PSMZ24;
			zb1.mask = 0xffffff;
			break;
		case 2:
			zb1.PSM = PSMZ16;
			zb1.mask = 0xffff;
			break;
		case 0xa:
			zb1.PSM = PSMZ16S;
			zb1.mask = 0xffff;
			break;
		}
		zb1.ZMSK = (data >> 32) & 1;
		SETCONTEXT();
		break;

	case GS_ZBUF_2:
		zb2.ZBP = (data & 0x1ff) << 11;
		psm = (data >> 24) & 0xf;
		switch (psm) {
		case 0:
			zb2.PSM = PSMZ32;
			zb2.mask = 0xffffffff;
			break;
		case 1:
			zb2.PSM = PSMZ24;
			zb2.mask = 0xffffff;
			break;
		case 2:
			zb2.PSM = PSMZ16;
			zb2.mask = 0xffff;
			break;
		case 0xa:
			zb2.PSM = PSMZ16S;
			zb2.mask = 0xffff;
			break;
		}
		zb2.ZMSK = (data >> 32) & 1;
		SETCONTEXT();
		break;

	case GS_BITBLTBUF:
		bitblt.SBP = (data & 0x3fff) << 6;
		bitblt.SBW = ((data >> 16) & 0x3f) << 6;
		bitblt.SPSM = (data >> 24) & 0x3f;
		bitblt.DBP = ((data >> 32) & 0x3fff) << 6;
		bitblt.DBW = ((data >> 48) & 0x3f) << 6;
		bitblt.DPSM = (data >> 56) & 0x3f;
		break;

	case GS_TRXPOS:
		bitblt.m_48 = data & 0x7ff;
		bitblt.m_4c = (data >> 16) & 0x7ff;
		bitblt.m_50 = (data >> 32) & 0x7ff;
		bitblt.m_54 = (data >> 48) & 0x7ff;
		bitblt.DIR = (data >> 59) & 3;
		break;

	case GS_TRXREG:
		w = data & 0xfff;
		if (w > 0x800)
			w = 0x800;
		bitblt.m_64 = w;
		h = (data >> 32) & 0xfff;
		if (h > 0x800)
			h = 0x800;
		bitblt.m_68 = h;
		break;

	case GS_TRXDIR:
		bitblt.SSAX = bitblt.m_48;
		bitblt.SSAY = bitblt.m_4c;
		bitblt.DSAX = bitblt.m_50;
		bitblt.DSAY = bitblt.m_54;
		bitblt.RRW = bitblt.m_64;
		bitblt.RRH = bitblt.m_68;
		bitblt.TRXDIR = data & 3;
		bitblt.count = bitblt.RRW;
		bitblt.x = bitblt.DSAX;
		bitblt.phase = 0;
		if (bitblt.TRXDIR == 2) {
			if ((bitblt.SPSM & 7) != (bitblt.DPSM & 7))
				fprintf(stderr, "BITBLTBUF: Depth is different\n");
			else
				bitblt.DoBitBLT(this);
		}
		break;

	case GS_HWREG:
		if (bitblt.TRXDIR == 0) {
			bitblt.WritePixel(this, data);
			bitblt.phase++;
		} else
			fprintf(stderr, "HWREG:Now not Host to Local mode\n");
		break;

	case GS_TEST_1:
		zb1.ZTE = (data >> 16) & 1;
		SETCONTEXT();
		break;

	case GS_TEST_2:
		zb2.ZTE = (data >> 16) & 1;
		SETCONTEXT();
		break;

	case GS_PRIM:
	case GS_PRMODE:
		bitblt.m_80 = (data >> 9) & 1;
		SETCONTEXT();
		break;

	default:
		fprintf(stderr, "Unknown register( 0x%x )\n", addr);
		exit(0);

	case GS_TEXFLUSH:
	case GS_FIELD:
		break;
	}
}
