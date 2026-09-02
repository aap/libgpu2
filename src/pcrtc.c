#include "pcrtc.h"

/* pcrtc.c - the display circuit's painting code and PCRTCxif's register
 * decoder.  Everything else lives in include/pcrtc.h, because the 1998
 * object proves it did: the assert inside PCRTCxif::DisplayPcrtc bakes
 * __FILE__ = "pcrtc.h" and __LINE__ = 574 into .rodata, and PCRTCxif's
 * SetRegister carries inline copies of every SetXxx body.
 *
 * See doc/notes/pcrtc.md.
 */

/* The whole merge, one vsync's worth.  CRTMD picks between the merged
 * output (0), circuit 2 alone (1) and a blank screen (2). */
void
DispInfo::DisplayPixel(int dn, Memory *mem, Xifbase *xif)
{
	int mode, m;

	if (dn == 0) {
		m = CRTMD;
		mode = 2;
		if (m <= 3)
			if (m >= 0)
				mode = 0;
	} else
		switch (CRTMD) {
		case 1:
		case 5:
			mode = 0;
			break;
		case 2:
		case 6:
			mode = 1;
			break;
		default:
			mode = 2;
			break;
		}
	xif->SetBackground(bg.R, bg.G, bg.B);
	xif->ClearDisplay();
	switch (mode) {
	case 0:
		if (dc.over == 0) {
			if (EN1) {
				if (NeedBlend() == 0) {
					if (dc.Magnified(0) == 0)
						displayNoBlend(0, mem, xif,
							aout1);
					else
						displayNoBlendMag(0, mem, xif,
							aout1);
				} else
					displayBlendBG(mem, xif, aout1);
			}
			if (EN2) {
				if (dc.Magnified(1) == 0)
					displayNoBlend(1, mem, xif, aout2);
				else
					displayNoBlendMag(1, mem, xif, aout2);
			}
		} else if (EN1) {
			if (EN2 == 0 || SLBG == 1 && aout1 != AOutZero) {
				if (NeedBlend() == 0) {
					if (dc.Magnified(0) == 0)
						displayNoBlend(0, mem, xif,
							aout1);
					else
						displayNoBlendMag(0, mem, xif,
							aout1);
				} else
					displayBlendBG(mem, xif, aout1);
			} else if (SLBG == 1) {
				if (aout1 != AOutZero)
					displayBlendBG(mem, xif, aout1);
				else
					displayBlendBGAmod2(mem, xif);
			} else
				displayBlend(mem, xif);
		} else if (EN2) {
			if (dc.Magnified(1) == 0)
				displayNoBlend(1, mem, xif, aout2);
			else
				displayNoBlendMag(1, mem, xif, aout2);
		}
		break;
	case 1:
		if (EN2) {
			if (dc.Magnified(1) == 0)
				displayNoBlend(1, mem, xif, AOutAlpha);
			else
				displayNoBlendMag(1, mem, xif, AOutAlpha);
		} else
			xif->Flush();
		break;
	case 2:
		xif->SetBackground(0, 0, 0);
		xif->ClearDisplay();
		xif->Flush();
		break;
	}
}

/* Draw one display circuit, unmagnified and unblended: straight from local
 * memory into the X11 side, one pixel per pixel. */
void
DispInfo::displayNoBlend(int dn, Memory *mem, Xifbase *xif, AOut aout)
{
	int x, y, ox, oy, w, h;

	xif->PrepareImgBuffer(dc.c[dn].DW, dc.c[dn].DH);
	h = dc.c[dn].h;
	y = dc.c[dn].DBY;
	oy = 0;
	for (;;) {
		if (h == 0)
			break;
		w = dc.c[dn].w;
		x = dc.c[dn].DBX;
		ox = 0;
		for (;;) {
			PixColor c;

			if (w == 0)
				break;
			dc.rd[dn]->ReadPixel(mem, x, y, c);
			xif->DrawPixel(ox, oy, c.R, c.G, c.B,
				aout == AOutAlpha ? c.A : 0);
			w--;
			x++;
			ox++;
		}
		h--;
		y++;
		oy++;
	}
	xif->DisplayPixel(dc.c[dn].DX - hstart, dc.c[dn].DY - VStart(),
		dc.c[dn].DW, dc.c[dn].DH);
}

/* The same, with DISPLAY's MAGH/MAGV pixel replication. */
void
DispInfo::displayNoBlendMag(int dn, Memory *mem, Xifbase *xif, AOut aout)
{
	int x, y, ox, oy, w, h, mh, mv;

	mh = dc.c[dn].MAGH + 1;
	mv = dc.c[dn].MAGV + 1;
	xif->PrepareImgBuffer(dc.c[dn].DW, dc.c[dn].DH);
	h = dc.c[dn].h;
	oy = 0;
	for (;;) {
		if (h == 0)
			break;
		y = oy/mv + dc.c[dn].DBY;
		w = dc.c[dn].w;
		ox = 0;
		for (;;) {
			PixColor c;

			if (w == 0)
				break;
			x = ox/mh + dc.c[dn].DBX;
			dc.rd[dn]->ReadPixel(mem, x, y, c);
			xif->DrawPixel(ox, oy, c.R, c.G, c.B,
				aout == AOutAlpha ? c.A : 0);
			w--;
			ox++;
		}
		h--;
		oy++;
	}
	xif->DisplayPixel(dc.c[dn].DX - hstart, dc.c[dn].DY - VStart(),
		dc.c[dn].DW, dc.c[dn].DH);
}

/* Both circuits are on and their rectangles overlap: walk the bounding box
 * and merge.  Circuit 1 goes over circuit 2 through PMODE.MMOD's blender;
 * where only one circuit covers a pixel that circuit is blended with the
 * background instead, and where neither does the background comes out
 * unchanged.  PMODE.AMOD picks whose alpha reaches the display. */
void
DispInfo::displayBlend(Memory *mem, Xifbase *xif)
{
	int x, y, ox, oy, w, h, sx, sy0, sy1, mx, my, mw, mh;
	PixColor c0, c1;

	mx = dc.mx;
	my = dc.my;
	mw = dc.mw;
	mh = dc.mh;
	xif->PrepareImgBuffer(mw, mh);
	y = my;
	h = mh;
	oy = 0;
	for (;;) {
		if (h == 0)
			break;
		sy0 = (y - dc.c[0].DY)/(dc.c[0].MAGV + 1) + dc.c[0].DBY;
		sy1 = (y - dc.c[1].DY)/(dc.c[1].MAGV + 1) + dc.c[1].DBY;
		x = mx;
		w = mw;
		ox = 0;
		for (;;) {
			char in0, in1;

			if (w == 0)
				break;
			in0 = dc.Inside(0, x, y);
			in1 = dc.Inside(1, x, y);
			if (in0) {
				if (in1) {
					sx = (x - dc.c[0].DX)/
						(dc.c[0].MAGH + 1)
						+ dc.c[0].DBX;
					dc.rd[0]->ReadPixel(mem, sx, sy0, c0);
					sx = (x - dc.c[1].DX)/
						(dc.c[1].MAGH + 1)
						+ dc.c[1].DBX;
					dc.rd[1]->ReadPixel(mem, sx, sy1, c1);
					bl->blend(c0, c1);
					if (aout1 == AOutZero)
						c0.A = c1.A;
				} else {
					sx = (x - dc.c[0].DX)/
						(dc.c[0].MAGH + 1)
						+ dc.c[0].DBX;
					dc.rd[0]->ReadPixel(mem, sx, sy0, c0);
					bl->blend(c0, bg);
					if (aout1 == AOutZero)
						c0.A = 0;
				}
			} else {
				if (in1) {
					sx = (x - dc.c[1].DX)/
						(dc.c[1].MAGH + 1)
						+ dc.c[1].DBX;
					dc.rd[1]->ReadPixel(mem, sx, sy1, c0);
					if (aout2 == AOutZero)
						c0.A = 0;
				} else
					c0 = bg;
			}
			xif->DrawPixel(ox, oy, c0.R, c0.G, c0.B, c0.A);
			x++;
			ox++;
			w--;
		}
		y++;
		oy++;
		h--;
	}
	xif->DisplayPixel(mx - hstart, my - VStart(), mw, mh);
}

/* PMODE.SLBG == 1 with the alpha coming from circuit 2: circuit 1 is
 * blended with the background rather than with circuit 2, and circuit 2
 * contributes nothing but its alpha. */
void
DispInfo::displayBlendBGAmod2(Memory *mem, Xifbase *xif)
{
	int x, y, ox, oy, w, h, sx, sy0, sy1, mx, my, mw, mh;
	PixColor c0, c1;

	mx = dc.mx;
	my = dc.my;
	mw = dc.mw;
	mh = dc.mh;
	xif->PrepareImgBuffer(mw, mh);
	y = my;
	h = mh;
	oy = 0;
	for (;;) {
		if (h == 0)
			break;
		sy0 = (y - dc.c[0].DY)/(dc.c[0].MAGV + 1) + dc.c[0].DBY;
		sy1 = (y - dc.c[1].DY)/(dc.c[1].MAGV + 1) + dc.c[1].DBY;
		x = mx;
		w = mw;
		ox = 0;
		for (;;) {
			char in0, in1;

			if (w == 0)
				break;
			in0 = dc.Inside(0, x, y);
			in1 = dc.Inside(1, x, y);
			if (in0) {
				if (in1) {
					sx = (x - dc.c[0].DX)/
						(dc.c[0].MAGH + 1)
						+ dc.c[0].DBX;
					dc.rd[0]->ReadPixel(mem, sx, sy0, c0);
					sx = (x - dc.c[1].DX)/
						(dc.c[1].MAGH + 1)
						+ dc.c[1].DBX;
					dc.rd[1]->ReadPixel(mem, sx, sy1, c1);
					bl->blend(c0, bg);
					c0.A = c1.A;
				} else {
					sx = (x - dc.c[0].DX)/
						(dc.c[0].MAGH + 1)
						+ dc.c[0].DBX;
					dc.rd[0]->ReadPixel(mem, sx, sy0, c0);
					bl->blend(c0, bg);
					c0.A = 0;
				}
			} else {
				if (in1) {
					sx = (x - dc.c[1].DX)/
						(dc.c[1].MAGH + 1)
						+ dc.c[1].DBX;
					dc.rd[1]->ReadPixel(mem, sx, sy1, c0);
					SetRGB(c0, bg.R, bg.G, bg.B);
				} else
					c0 = bg;
			}
			xif->DrawPixel(ox, oy, c0.R, c0.G, c0.B, c0.A);
			x++;
			ox++;
			w--;
		}
		y++;
		oy++;
		h--;
	}
	xif->DisplayPixel(mx - hstart, my - VStart(), mw, mh);
}

/* Only circuit 1 contributes: blend it with the background over its own
 * DISPLAY rectangle. */
void
DispInfo::displayBlendBG(Memory *mem, Xifbase *xif, AOut aout)
{
	int x, y, ox, oy, w, h, sx, sy, dx, dy, dw, dh;

	dx = dc.c[0].DX;
	dy = dc.c[0].DY;
	dw = dc.c[0].DW;
	dh = dc.c[0].DH;
	xif->PrepareImgBuffer(dc.c[0].DW, dh);
	y = dy;
	h = dh;
	oy = 0;
	for (;;) {
		if (h == 0)
			break;
		sy = (y - dc.c[0].DY)/(dc.c[0].MAGV + 1) + dc.c[0].DBY;
		x = dx;
		w = dw;
		ox = 0;
		for (;;) {
			PixColor c;

			if (w == 0)
				break;
			sx = (x - dc.c[0].DX)/(dc.c[0].MAGH + 1) + dc.c[0].DBX;
			dc.rd[0]->ReadPixel(mem, sx, sy, c);
			bl->blend(c, bg);
			xif->DrawPixel(ox, oy, c.R, c.G, c.B,
				aout == AOutAlpha ? c.A : 0);
			x++;
			ox++;
			w--;
		}
		y++;
		oy++;
		h--;
	}
	xif->DisplayPixel(dx - hstart, dy - VStart(), dw, dh);
}

/* The pre-merge display path, kept for the "Display" pseudo-register.  It
 * ignores PMODE completely and paints one circuit at its own size. */
void
DispInfo::oldDispPixel(int dn, Memory *mem, Xifbase *xif)
{
	int x, y, ox, oy, w, h;

	xif->PrepareImgBuffer(dc.c[dn].DW, dc.c[dn].DH);
	h = dc.c[dn].h;
	y = dc.c[dn].DBY;
	oy = 0;
	for (;;) {
		if (h == 0)
			break;
		w = dc.c[dn].w;
		x = dc.c[dn].DBX;
		ox = 0;
		for (;;) {
			PixColor c;

			if (w == 0)
				break;
			dc.rd[dn]->ReadPixel(mem, x, y, c);
			xif->DrawPixel(ox, oy, c.R, c.G, c.B, c.A);
			w--;
			x++;
			ox++;
		}
		h--;
		y++;
		oy++;
	}
	xif->DisplayPixel(dc.c[dn].DX, dc.c[dn].DY, dc.c[dn].DW,
		dc.c[dn].DH);
}

/* The magnifying variant of the same, with the magnification taken from
 * the Display pseudo-register rather than from DISPLAY.  This one reads
 * local memory itself instead of going through a MemRead, and its 16 bit
 * unpacking uses six-bit masks where five were meant - see
 * doc/notes/pcrtc.md. */
void
DispInfo::oldDispPixelMag(int dn, Memory *mem, Xifbase *xif,
	int hmag, int vmag)
{
	int i, j, x, y, ox, oy, w, h, mh, mv, dw, dh;
	unsigned d;

	switch (hmag) {
	case 0:
		mh = 1;
		break;
	case 1:
		mh = 2;
		break;
	case 2:
		mh = 3;
		break;
	case 3:
		mh = 4;
		break;
	default:
		fprintf(stderr, "Invalid hmag (0x%0x)\n", hmag);
		exit(0);
	}
	switch (vmag) {
	case 0:
		mv = 1;
		break;
	case 1:
		mv = 2;
		break;
	case 2:
		mv = 3;
		break;
	case 3:
		mv = 4;
		break;
	default:
		fprintf(stderr, "Invalid vmag (0x%0x)\n", vmag);
		exit(0);
	}
	dw = mh*dc.c[dn].DW;
	dh = mv*dc.c[dn].DH;
	xif->PrepareImgBuffer(dw, dh);
	if (dc.c[dn].PSM == 0 || dc.c[dn].PSM == 1) {
		h = dc.c[dn].h;
		y = dc.c[dn].DBY;
		oy = 0;
		while (h) {
			w = dc.c[dn].w;
			x = dc.c[dn].DBX;
			ox = 0;
			while (w) {
				Address(x, y, dc.c[dn].PSM, dc.c[dn].FBW,
					dc.c[dn].FBP);
				d = mem->vram[addr];
				for (i = 0; i < mv; i++) {
					if (dh < oy + i)
						break;
					for (j = 0; j < mh; j++) {
						if (ox + j > dw)
							break;
						xif->DrawPixel(ox + j, oy + i,
							d & 0xff,
							(d >> 8) & 0xff,
							(d >> 16) & 0xff);
					}
				}
				w--;
				x++;
				ox += mh;
			}
			h--;
			y++;
			oy += mv;
		}
	} else if (dc.c[dn].PSM == 2 || dc.c[dn].PSM == 10) {
		h = dc.c[dn].h;
		y = dc.c[dn].DBY;
		oy = 0;
		while (h) {
			w = dc.c[dn].w;
			x = dc.c[dn].DBX;
			ox = 0;
			while (w) {
				Address(x, y, dc.c[dn].PSM, dc.c[dn].FBW,
					dc.c[dn].FBP);
				d = mem->vram[addr];
				if (bitpos)
					d >>= 16;
				d &= 0xffff;
				for (i = 0; i < mv; i++) {
					if (dh < oy + i)
						break;
					for (j = 0; j < mh; j++) {
						if (ox + j > dw)
							break;
						xif->DrawPixel(ox + j, oy + i,
							(d & 0x3f) << 3,
							((d >> 5) & 0x3f) << 3,
							((d >> 10) & 0x3f) << 3);
					}
				}
				w--;
				x++;
				ox += mh;
			}
			h--;
			y++;
			oy += mv;
		}
	}
	xif->DisplayPixel(0, 0, dw, dh);
}

/* The privileged register path.  Every SetXxx body below is an inline
 * member of PCRTCxif, so this switch carries a copy of each. */
void
PCRTCxif::SetRegister(int addr, long long data)
{
	switch (addr) {
	case 0x80:
		SetPMODE(data);
		break;
	case 0x81:
		SetSMODE1(data);
		break;
	case 0x82:
		SetSMODE2(data);
		break;
	case 0x84:
		SetSYNCH1(data);
		break;
	case 0x85:
		SetSYNCH2(data);
		break;
	case 0x86:
		SetSYNCV(data);
		break;
	case 0x87:
		SetDISPFB1(data);
		break;
	case 0x88:
		SetDISPLAY1(data);
		break;
	case 0x89:
		SetDISPFB2(data);
		break;
	case 0x8a:
		SetDISPLAY2(data);
		break;
	case 0x8b:
	case 0x8c:
	case 0x8d:
		PCRTC::SetRegister(addr, data);
		break;
	case 0x8e:
		SetBGCOLOR(data);
		break;
	case 0x100:
		Display(data);
		break;
	case 0x101:
		DisplayPcrtc(data);
		break;
	default:
		fprintf(stderr, "Unknown register( 0x%x )\n", addr);
		exit(0);
	}
}
