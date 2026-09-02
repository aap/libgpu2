#include <stdio.h>
#include <stdlib.h>

#include "gsdefs.h"
#include "addrconv.h"

void
AddrConv::address_convert(int x, int y, int psm, int bw, int tbp,
	int &page, int &blk, int &bnk, int &pos, int &wd, int &np)
{
	static int table[2][4] = {
		{ 0, 1, 2, 3 },
		{ 2, 3, 0, 1 },
	};
	int p;
	int addr;
	int i;
	unsigned char t;

	switch (psm) {
	case PSMCT32:
	case PSMCT24:
	case PSMT8H:
	case PSMT4HL:
	case PSMT4HH:
	case PSMZ32:
	case PSMZ24:
		p = (bw*(y>>5) + (x>>6)) & 0x1ff;
		addr = p << 7;
		addr += (x & 0x20) << 1;
		addr += (y & 0x10) << 1;
		addr += x & 0x10;
		addr += y & 0x8;
		addr += (x & 0x8) >> 1;
		addr += (y>>1) & 0x3;
		addr += tbp*4;
		break;
	case PSMCT16:
	case PSMZ16:
		p = (bw*(y>>6) + (x>>6)) & 0x1ff;
		addr = p << 7;
		addr += (y & 0x20) << 1;
		addr += x & 0x20;
		addr += y & 0x10;
		addr += (x & 0x10) >> 1;
		addr += (y>>1) & 0x7;
		addr += tbp*4;
		break;
	case PSMCT16S:
	case PSMZ16S:
		p = (bw*(y>>6) + (x>>6)) & 0x1ff;
		addr = p << 7;
		addr += (x & 0x20) << 1;
		addr += (y & 0x10) << 1;
		addr += (y & 0x20) >> 1;
		addr += (x & 0x10) >> 1;
		addr += (y>>1) & 0x7;
		addr += tbp*4;
		break;
	case PSMT8:
		p = ((bw>>1)*(y>>6) + (x>>7)) & 0x1ff;
		addr = p << 7;
		addr += x & 0x40;
		addr += y & 0x20;
		addr += (x & 0x20) >> 1;
		addr += (y & 0x10) >> 1;
		addr += (x & 0x10) >> 2;
		addr += (y>>2) & 0x3;
		addr += tbp*4;
		break;
	case PSMT4:
		p = ((bw>>1)*(y>>7) + (x>>7)) & 0x1ff;
		addr = p << 7;
		addr += y & 0x40;
		addr += (x & 0x40) >> 1;
		addr += (y & 0x20) >> 1;
		addr += (x & 0x20) >> 2;
		addr += (y>>2) & 0x7;
		addr += tbp*4;
		break;
	default:
		fprintf(stderr, "illegal <psm> parameter\n");
		exit(1);
	}

	page = addr >> 7;
	if (psm == PSMZ32 || psm == PSMZ24 || psm == PSMZ16 || psm == PSMZ16S) {
		t = (unsigned)addr>>6 ^ 1;
		blk = t & 1;
	} else
		blk = (addr>>6) & 1;
	bnk = ((addr>>6) & 1) ^ ((addr>>5) & 1);
	pos = addr & 0x1f;
	i = ((x << 1) & 0xc) + ((y << 3) & 0x10);
	wd = *(int *)((char *)table + i);

	switch (psm) {
	case PSMCT32:
	case PSMCT24:
	case PSMZ32:
	case PSMZ24:
		np = ((y & 1) << 4) + ((x & 1) << 3);
		break;
	case PSMT8H:
		np = ((y & 1) << 4) + ((x & 1) << 3) + 6;
		break;
	case PSMT4HH:
		np = ((y & 1) << 4) + ((x & 1) << 3) + 7;
		break;
	case PSMT4HL:
		np = ((y & 1) << 4) + ((x & 1) << 3) + 6;
		break;
	case PSMCT16:
	case PSMCT16S:
	case PSMZ16:
	case PSMZ16S:
		np = ((y & 1) << 4) + ((x & 1) << 3) + ((x & 8) >> 1);
		break;
	case PSMT8:
		np = ((y & 1) << 4) + ((x & 1) << 3) + ((x & 8) >> 1) + (y & 2);
		break;
	case PSMT4:
		np = ((y & 1) << 4) + ((x & 1) << 3) + ((x & 0x18) >> 2) + ((y>>1) & 1);
		break;
	}
}
