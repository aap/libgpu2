#include <stdio.h>
#include <stdlib.h>

#include "xif.h"

/* Ordered dither matrices, 8 bit -> 3 bits and 8 bit -> 2 bits.  The 2 bit
 * one is the 3 bit one doubled; both are the classic 4x4 Bayer pattern the
 * X11 sample clients used. */
static int dm8_3[4][4] = {
	{  0, 24,  6, 30 },
	{ 16,  8, 22, 14 },
	{  4, 28,  2, 26 },
	{ 20, 12, 18, 10 },
};

static int dm8_2[4][4] = {
	{  0, 48, 12, 60 },
	{ 32, 16, 44, 28 },
	{  8, 56,  4, 52 },
	{ 40, 24, 36, 20 },
};

/* Ordered dither of one 8-bit component down to 3 or 2 bits.  These are
 * inline, which is what makes DrawPixel() and SetBackground() keep their r,
 * g and b parameters in memory: taking &c makes the parameter addressable.
 * g++ 2.7 still emits an out-of-line copy of each, right after the last
 * non-inline function - see doc/notes/xif.md. */
inline void
XWindow::dither8to3(int x, int y, int *c)
{
	int v = *c & 0x1f;
	int i = (y%4)*4 + (x%4)*16;

	if (v > *(int *)((char *)dm8_3 + i)) {
		*c = (*c & 0xe0) + 0x20;
		if (*c > 0xff)
			*c = 0xe0;
	} else
		*c &= 0xe0;
}

inline void
XWindow::dither8to2(int x, int y, int *c)
{
	int v = *c & 0x3f;
	int i = (y%4)*4 + (x%4)*16;

	if (v > *(int *)((char *)dm8_2 + i)) {
		*c = (*c & 0xc0) + 0x40;
		if (*c > 0xff)
			*c = 0xc0;
	} else
		*c &= 0xc0;
}

/* 3-3-2 ramp for the PseudoColor fallback. */
void
XWindow::SetColormap(Display *dpy, unsigned long cmap)
{
	XColor col;
	int r, g, b;
	int i;

	i = 0;
	for (r = 0; r != 8; r++)
		for (g = 0; g != 8; g++)
			for (b = 0; b != 4; b++) {
				col.pixel = i++;
				col.red = r << 13;
				col.green = g << 13;
				col.blue = b << 14;
				col.flags = DoRed|DoGreen|DoBlue;
				XStoreColor(dpy, cmap, &col);
			}
}

/* Deepest TrueColor visual if there is one deeper than 8 bits, else an
 * 8-bit PseudoColor one with our own ramp installed. */
XVisualInfo *
XWindow::ChooseVisual(Display *dpy)
{
	XVisualInfo tmpl;
	XVisualInfo *vi, *best;
	int n, i;

	tmpl.c_class = TrueColor;
	vi = XGetVisualInfo(dpy, VisualClassMask, &tmpl, &n);
	best = vi;
	for (i = 1; i < n; i++)
		if (vi[i].depth > best->depth)
			best = &vi[i];
	if (best && best->depth > 8) {
		cmap = XCreateColormap(dpy, RootWindow(dpy, best->screen),
			best->visual, AllocNone);
		return best;
	}
	tmpl.c_class = PseudoColor;
	tmpl.depth = 8;
	vi = XGetVisualInfo(dpy, VisualDepthMask|VisualClassMask, &tmpl, &n);
	if (vi == 0)
		return 0;
	cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
		vi->visual, AllocAll);
	SetColormap(dpy, cmap);
	return vi;
}

void
XWindow::OpenWindow(char *name, int w, int h)
{
	XSetWindowAttributes attr;
	XGCValues gcv;
	unsigned long mask;

	dpy = XOpenDisplay(0);
	if (dpy == 0) {
		fprintf(stderr, "Can't connect to X server\n");
		exit(1);
	}
	vis = ChooseVisual(dpy);
	if (vis == 0) {
		fprintf(stderr, "No visual found\n");
		exit(1);
	}
	attr.colormap = cmap;
	attr.background_pixel = 0;
	attr.border_pixel = 1;
	mask = CWBackPixel|CWBorderPixel|CWColormap;
	if (DoesBackingStore(DefaultScreenOfDisplay(dpy))) {
		attr.backing_store = WhenMapped;
		attr.backing_planes = AllPlanes;
		mask = CWBackPixel|CWBorderPixel|CWColormap|
			CWBackingStore|CWBackingPlanes;
	}
	win = XCreateWindow(dpy, RootWindow(dpy, vis->screen), 0, 0, w, h, 2,
		vis->depth, InputOutput, vis->visual, mask, &attr);
	XSetStandardProperties(dpy, win, name, name, None, 0, 0, 0);
	XMapWindow(dpy, win);
	XFlush(dpy);
	gcv.function = GXcopy;
	gcv.foreground = 0xffffff;
	gcv.background = 0;
	gc = XCreateGC(dpy, win, GCFunction|GCForeground|GCBackground, &gcv);
	width = height = 0;
	img = 0;
}

int
XWindow::highbit(unsigned long ul)
{
	unsigned long mask = 0x80000000;
	int i;

	for (i = 31; (ul & mask) == 0 && i >= 0; i--, ul <<= 1)
		;
	return i;
}

void
XWindow::PrepareImgBuffer(int w, int h)
{
	if (width < w || height < h) {
		if (img) {
			img->data = 0;
			XDestroyImage(img);
		}
		img = XCreateImage(dpy, vis->visual, vis->depth, ZPixmap, 0, 0,
			w, h, 32, 0);
		rmask = vis->red_mask;
		gmask = vis->green_mask;
		bmask = vis->blue_mask;
		if (data)
			free(data);
		data = (char *)malloc(h * img->bytes_per_line);
		if (data == 0) {
			perror("TrueColorImage");
			exit(1);
		}
		rshift = 7 - highbit(rmask);
		gshift = 7 - highbit(gmask);
		bshift = 7 - highbit(bmask);
		width = w;
		height = h;
		img->data = data;
	}
}

void
XWindow::DrawPixel(int x, int y, int r, int g, int b)
{
	unsigned long pixel;

	if (vis->c_class == TrueColor) {
		if (rshift < 0)
			r <<= -rshift;
		else
			r >>= rshift;
		if (gshift < 0)
			g <<= -gshift;
		else
			g >>= gshift;
		if (bshift < 0)
			b <<= -bshift;
		else
			b >>= bshift;
		r &= rmask;
		g &= gmask;
		b &= bmask;
		pixel = r | g | b;
	} else {
		dither8to3(x, y, &r);
		dither8to3(x, y, &g);
		dither8to2(x, y, &b);
		pixel = (r & 0xe0) | ((g >> 5) << 2) | (b >> 6);
	}
	XPutPixel(img, x, y, pixel);
}

void
XWindow::DisplayPixel(int x, int y, int w, int h)
{
	XPutImage(dpy, win, gc, img, 0, 0, x, y, w, h);
	XFlush(dpy);
}

void
XWindow::ClearDisplay()
{
	XClearWindow(dpy, win);
}

void
XWindow::SetBackground(int r, int g, int b)
{
	unsigned long pixel;

	if (vis->c_class == TrueColor) {
		if (rshift < 0)
			r <<= -rshift;
		else
			r >>= rshift;
		if (gshift < 0)
			g <<= -gshift;
		else
			g >>= gshift;
		if (bshift < 0)
			b <<= -bshift;
		else
			b >>= bshift;
		r &= rmask;
		g &= gmask;
		b &= bmask;
		pixel = r | g | b;
	} else {
		dither8to3(0, 0, &r);
		dither8to3(0, 0, &g);
		dither8to2(0, 0, &b);
		pixel = (r & 0xe0) | ((g >> 5) << 2) | (b >> 6);
	}
	XSetWindowBackground(dpy, win, pixel);
}

