#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gsdefs.h"
#include "bitblt.h"
#include "clut.h"

class Memory {
public:
	int vram[0x100000];	/* 0x000000, 4 MB of local memory */
};

/* CSM1: the CLUT is a 16x16 (or 8x2) block of texels in local memory. */
void
TexClut::load1(ClutAttr &a)
{
	int x, y;
	int csa = a.CSA;
	int k, n;
	unsigned d;

	if (a.CPSM == 0) {
		if (Depth(a.PSM) == 8) {
			y = 0;
			for (;;) {
				if (y == 16)
					break;
				x = 0;
				for (;;) {
					if (x == 16)
						break;
					Address(x, y, PSMCT32, 64, a.CBP);
					n = y/2*32 + y%2*8 + x + x/8*8;
					clut[n] = memif->mem->vram[addr];
					x++;
				}
				y++;
			}
		} else {
			n = csa;
			y = 0;
			for (;;) {
				if (y == 2)
					break;
				x = 0;
				for (;;) {
					if (x == 8)
						break;
					Address(x, y, PSMCT32, 64, a.CBP);
					clut[n] = memif->mem->vram[addr];
					x++;
					n++;
				}
				y++;
			}
		}
	} else {
		if (Depth(a.PSM) == 8) {
			y = 0;
			for (;;) {
				if (y == 16)
					break;
				x = 0;
				for (;;) {
					if (x == 16)
						break;
					Address(x, y, PSMCT16, 64, a.CBP);
					n = y/2*32 + y%2*8 + x + x/8*8;
					d = memif->mem->vram[addr];
					if (bitpos)
						d >>= 16;
					d &= 0xffff;
					k = csa + n;
					if (k > 0xff)
						clut[k & 0xff] = (clut[k & 0xff] & 0xffff) | (d << 16);
					else
						clut[k] = (clut[k] & 0xffff0000) | d;
					x++;
				}
				y++;
			}
		} else {
			n = 0;
			y = 0;
			for (;;) {
				if (y == 2)
					break;
				x = 0;
				for (;;) {
					if (x == 8)
						break;
					Address(x, y, PSMCT16, 64, a.CBP);
					d = memif->mem->vram[addr];
					if (bitpos)
						d >>= 16;
					d &= 0xffff;
					k = csa + n;
					if (k > 0xff)
						clut[k & 0xff] = (clut[k & 0xff] & 0xffff) | (d << 16);
					else
						clut[k] = (clut[k] & 0xffff0000) | d;
					x++;
					n++;
				}
				y++;
			}
		}
	}
}

/* CSM2: the CLUT is a run of 16-bit texels starting at (COU, COV). */
void
TexClut::load2(ClutAttr &a)
{
	int i, n;
	int cbp, cbw, cov, cou;
	unsigned d;

	cbp = a.CBP;
	cbw = a.CBW;
	cou = a.COU;
	cov = a.COV;
	if (a.CSA != 0) {
		fprintf(stderr, "CSA must be 0 when CSM2\n");
		exit(1);
	}
	if (a.CPSM == 0) {
		fprintf(stderr, "CPSM must be PSMCT16(S) when CSM2\n");
		exit(1);
	}
	int b = Depth(a.PSM);
	n = 16;
	if (b == 8)
		n = 256;
	i = 0;
	for (;;) {
		if (i == n)
			break;
		Address(cou, cov, PSMCT16, cbw, cbp);
		d = memif->mem->vram[addr];
		if (bitpos)
			d >>= 16;
		*(short*)&clut[i] = d;
		i++;
		cou++;
	}
}

void
TexClut::LoadData(ClutAttr &a)
{
	int load = 0;
	int cld = a.CLD;

	switch (cld) {
	case 0:
		break;
	case 1:
		load = 1;
		break;
	case 2:
		cbp0 = a.CBP;
		load = 1;
		break;
	case 3:
		cbp1 = a.CBP;
		load = 1;
		break;
	case 4:
		if (a.CBP != cbp0) {
			load = 1;
			cbp0 = a.CBP;
		}
		break;
	case 5:
		if (a.CBP != cbp1) {
			load = 1;
			cbp1 = a.CBP;
		}
		break;
	default:
		fprintf(stderr, "TXM: Illegal CLD\n");
		exit(1);
	}
	if (load) {
		if (a.CSM == 0)
			load1(a);
		else
			load2(a);
	}
}

int
TexClut::Lookup(int i)
{
	int k;
	unsigned d;

	if (attr.CPSM == 0)
		return clut[i + attr.CSA];
	k = attr.CSA;
	k += i;
	d = clut[k & 0xff];
	if (k > 0xff)
		d >>= 16;
	return d & 0xffff;
}
