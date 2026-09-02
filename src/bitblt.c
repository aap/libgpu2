#include <stdio.h>
#include <stdlib.h>

#include "gsdefs.h"
#include "bitblt.h"

class Memory {
public:
	int vram[0x100000];	/* 0x000000, 4 MB of local memory */
};

/* One pixel out of the source buffer, right justified. */
unsigned
BitBLT::read(Memory *mem, int x, int y)
{
	unsigned d;

	if (SBW > 1024)
		x &= 0x7ff;
	else
		x &= 0x3ff;
	Address(x, y & 0x7ff, SPSM, SBW, SBP);
	d = mem->vram[addr];
	if (Depth(SPSM) != 32) {
		d >>= bitpos;
		d &= (1 << Depth(SPSM)) - 1;
	}
	return d;
}

/* One pixel into the destination buffer, merged with its neighbours. */
void
BitBLT::write(Memory *mem, unsigned data, int x, int y)
{
	unsigned mask, old;

	if (DBW > 1024)
		x &= 0x7ff;
	else
		x &= 0x3ff;
	if (DBW <= x)
		return;
	Address(x, y & 0x7ff, DPSM, DBW, DBP);
	if (Depth(DPSM) == 32)
		mem->vram[addr] = data;
	else {
		old = mem->vram[addr];
		mask = ((1 << Depth(DPSM)) - 1) << bitpos;
		data = (data << bitpos) & mask;
		old &= ~mask;
		data |= old;
		mem->vram[addr] = data;
	}
}

/* TRXDIR 2: local to local. */
void
BitBLT::DoBitBLT(Memory *mem)
{
	int sy, dy;
	int xstep, ystep;
	int sx, dx;
	int x, xd, n;

	switch (DIR) {
	case 0:
		xstep = 1;
		ystep = 1;
		sx = SSAX;
		sy = SSAY;
		dx = DSAX;
		dy = DSAY;
		break;
	case 1:
		xstep = 1;
		ystep = -1;
		sx = SSAX;
		sy = SSAY + RRH - 1;
		dx = DSAX;
		dy = DSAY + RRH - 1;
		break;
	case 2:
		xstep = -1;
		ystep = 1;
		sx = SSAX + RRW - 1;
		sy = SSAY;
		dx = DSAX + RRW - 1;
		dy = DSAY;
		break;
	case 3:
		xstep = -1;
		ystep = -1;
		sx = SSAX + RRW - 1;
		sy = SSAY + RRH - 1;
		dx = DSAX + RRW - 1;
		dy = DSAY + RRH - 1;
		break;
	default:
		fprintf(stderr, "BitBLT:Unknown direction\n");
		return;
	}
	for (; RRH != 0; RRH--, sy += ystep, dy += ystep) {
		x = sx;
		xd = dx;
		for (n = RRW; n != 0; n--, x += xstep, xd += xstep)
			write(mem, read(mem, x & 0x7ff, sy & 0x7ff), xd, dy);
	}
}

/* TRXDIR 0: one 64 bit host word into local memory. */
void
BitBLT::WritePixel(Memory *mem, long long data)
{
	int n, sh, mask;
	int t;

	if (DPSM == PSMCT24 || DPSM == PSMZ24) {
		if (count == 0) {
			if (--RRH == 0)
				return;
			count = RRW;
			x = DSAX;
			DSAY++;
		}
		t = phase % 3;
		switch (t) {
		case 0:
			for (n = 0; n != 2; n++, data >>= 24, x++, count--) {
				if (count == 0) {
					if (--RRH == 0)
						return;
					count = RRW;
					x = DSAX;
					DSAY++;
				}
				write(mem, data & 0xffffff, x, DSAY);
			}
			save = data & 0xffff;
			break;
		case 1:
			write(mem, (((int)data & 0xff) << 16) | save, x, DSAY);
			n = 0;
			data >>= 8;
			x++;
			count--;
			for (; n != 2; n++, data >>= 24, x++, count--) {
				if (count == 0) {
					if (--RRH == 0)
						return;
					count = RRW;
					x = DSAX;
					DSAY++;
				}
				write(mem, data & 0xffffff, x, DSAY);
			}
			save = data & 0xff;
			break;
		case 2:
			write(mem, (((int)data & 0xffff) << 8) | save, x, DSAY);
			n = 0;
			data >>= 16;
			x++;
			count--;
			for (; n != 2; n++, data >>= 24, x++, count--) {
				if (count == 0) {
					if (--RRH == 0)
						return;
					count = RRW;
					x = DSAX;
					DSAY++;
				}
				write(mem, data & 0xffffff, x, DSAY);
			}
			break;
		}
	} else {
		switch (DPSM) {
		case PSMCT32:
		case PSMZ32:
			n = 2;
			sh = 32;
			mask = -1;
			break;
		case PSMCT16:
		case PSMCT16S:
		case PSMZ16:
		case PSMZ16S:
			n = 4;
			sh = 16;
			mask = 0xffff;
			break;
		case PSMT8:
		case PSMT8H:
			n = 8;
			sh = 8;
			mask = 0xff;
			break;
		default:
			n = 16;
			sh = 4;
			mask = 0xf;
			break;
		}
		for (; n != 0; n--, data >>= sh, x++, count--) {
			if (RRH <= 0)
				break;
			if (count == 0) {
				if (--RRH == 0)
					return;
				count = RRW;
				x = DSAX;
				DSAY++;
			}
			write(mem, data & mask, x, DSAY);
		}
	}
}

/* TRXDIR 1: one 64 bit host word out of local memory. */
long long
BitBLT::ReadPixel(Memory *mem)
{
	int n, sh, i;
	long long d;
	long long data;

	if (TRXDIR == 3)
		return 0;
	if (TRXDIR != 1) {
		fprintf(stderr, "not in Local-Host mode\n");
		return 0;
	}
	if (SPSM == PSMCT24 || SPSM == PSMZ24) {
		if (count == 0) {
			if (--RRH == 0) {
				fprintf(stderr, "no more Local-Host data\n");
				return 0;
			}
			count = RRW;
			x = SSAX;
			SSAY++;
		}
		switch (phase++ % 3) {
		case 0:
			data = 0;
			for (n = 0; n != 3; n++, x++, count--) {
				if (count == 0) {
					if (--RRH == 0)
						return data;
					count = RRW;
					x = SSAX;
					SSAY++;
				}
				d = read(mem, x, SSAY);
				data |= d << (n*24);
			}
			save = (d >> 16) & 0xff;
			break;
		case 1:
			data = save;
			for (n = 0; n != 3; n++, x++, count--) {
				if (count == 0) {
					if (--RRH == 0)
						return data;
					count = RRW;
					x = SSAX;
					SSAY++;
				}
				d = read(mem, x, SSAY);
				data |= d << (n*24 + 8);
			}
			save = (d >> 8) & 0xffff;
			break;
		case 2:
			data = save;
			for (n = 0; n != 2; n++, x++, count--) {
				if (count == 0) {
					if (--RRH == 0)
						return data;
					count = RRW;
					x = SSAX;
					SSAY++;
				}
				d = read(mem, x, SSAY);
				data |= d << (n*24 + 16);
			}
			break;
		default:
			return 0;
		}
	} else {
		switch (SPSM) {
		case PSMCT32:
		case PSMZ32:
			n = 2;
			sh = 32;
			break;
		case PSMCT16:
		case PSMCT16S:
		case PSMZ16:
		case PSMZ16S:
			n = 4;
			sh = 16;
			break;
		case PSMT8:
		case PSMT8H:
			n = 8;
			sh = 8;
			break;
		default:
			n = 16;
			sh = 4;
			break;
		}
		data = 0;
		for (i = 0; i != n; i++, x++, count--) {
			if (RRH <= 0)
				break;
			if (count == 0) {
				if (--RRH == 0)
					return data;
				count = RRW;
				x = SSAX;
				SSAY++;
			}
			d = read(mem, x, SSAY);
			data |= d << (i*sh);
		}
	}
	return data;
}
