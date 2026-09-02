/* xif - the X11 interface behind PCRTC.
 *
 * Three classes:
 *
 *   Xifbase      the abstract display interface PCRTCxif talks to; nine
 *                virtuals, vptr at offset 0 (sizeof 4).
 *   XWindow      a real X11 window (sizeof 0x40).  The out-of-line half
 *                lives in src/xif.c; only the members defined here are
 *                inline, and they are the ones xif.o emits at the end of
 *                its .text.
 *   Frame2d      a plain 32-bit pixel rectangle with bounds asserts.
 *   XWindowDump  a headless Xifbase that renders into two Frame2d's and
 *                hands the finished one to a callback (sizeof 0x24).
 *
 * LINE NUMBERS ARE LOAD BEARING.  Frame2d's asserts bake __LINE__ into
 * xif.o (and pcrtc.o and gpu2.o): Frame2d::Resize's assert must stay on
 * line 139, Frame2d::Set's on 146 and Frame2d::Copy's on 158.  So is the
 * order of the class and member definitions - see doc/notes/xif.md.
 */

#ifndef XIF_H
#define XIF_H

#define XLIB_ILLEGAL_ACCESS	/* the 1998 Xlib.h had a public Display */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>

/* The era <assert.h> expansion, written out so that the __FILE__ string is
 * "xif.h" no matter which -I found this header. */
extern "C" void __assert_fail(const char *, const char *, unsigned int,
	const char *) __attribute__ ((__noreturn__));
#define assert(e) \
	((e) ? (void)0 : __assert_fail(#e, "xif.h", __LINE__, \
		__PRETTY_FUNCTION__))

class Xifbase {
public:
	virtual ~Xifbase() { }
	virtual void PrepareImgBuffer(int width, int height) = 0;
	virtual void DrawPixel(int x, int y, int r, int g, int b) = 0;
	virtual void DrawPixel(int x, int y, int r, int g, int b, int a) = 0;
	virtual void DisplayPixel(int x, int y, int width, int height) = 0;
	virtual void Resize(int width, int height) = 0;
	virtual void ClearDisplay() = 0;
	virtual void SetBackground(int r, int g, int b) = 0;
	virtual void Flush() = 0;
};

class XWindow : public Xifbase {
public:
	Display *dpy;		/* 0x04 */
	Window win;		/* 0x08 */
	XVisualInfo *vis;	/* 0x0c */
	Colormap cmap;		/* 0x10 */
	GC gc;			/* 0x14 */
	int width;		/* 0x18  size of the image buffer, not the window */
	int height;		/* 0x1c */
	XImage *img;		/* 0x20 */
	char *data;		/* 0x24 */
	unsigned long rmask;	/* 0x28  from the visual */
	unsigned long gmask;	/* 0x2c */
	unsigned long bmask;	/* 0x30 */
	int rshift;		/* 0x34  7 - highbit(mask), so negative shifts left */
	int gshift;		/* 0x38 */
	int bshift;		/* 0x3c */

	XWindow() { OpenWindow(0, 640, 480); data = 0; }
	XWindow(char *name) { OpenWindow(name, 640, 480); data = 0; }
	XWindow(char *name, int w, int h) { OpenWindow(name, w, h); data = 0; }
	~XWindow() { if (data) free(data); }

	void DrawPixel(int x, int y, int r, int g, int b, int a) {
		DrawPixel(x, y, r, g, b);
	}
	void Resize(int w, int h) {
		XResizeWindow(dpy, win, w, h);
		XFlush(dpy);
	}
	void Flush() { XFlush(dpy); }

	void SetColormap(Display *dpy, unsigned long cmap);
	XVisualInfo *ChooseVisual(Display *dpy);
	void OpenWindow(char *name, int width, int height);
	int highbit(unsigned long ul);
	void PrepareImgBuffer(int width, int height);
	void DrawPixel(int x, int y, int r, int g, int b);
	void DisplayPixel(int x, int y, int width, int height);
	void ClearDisplay();
	void SetBackground(int r, int g, int b);
	void dither8to2(int x, int y, int *c);
	void dither8to3(int x, int y, int *c);
};

/* A rectangle of 32-bit pixels.  Everything here is inline, so nothing of
 * Frame2d reaches an object file except the strings its asserts mention -
 * g++ 2.7 builds RTL for an inline member as soon as it parses it, which is
 * why xif.o, pcrtc.o and gpu2.o all carry the whole set even where no code
 * uses them.
 *
 * The next few lines are spaced so that the three asserts whose __LINE__ is
 * baked into the 1998 objects land where they landed in 1998: Resize on
 * 139, Set on 146 and Copy on 158.  Do not reflow this block without
 * re-checking doc/notes/xif.md.
 */

class Frame2d {
public:
	unsigned int *b;	/* 0x00 */
	int w;			/* 0x04 */
	int h;			/* 0x08 */

	Frame2d(int width, int height)
	{
		assert(width > 0 && height > 0);
		w = width;
		h = height;
		b = (unsigned int *)malloc(h*4*w);
	}
	~Frame2d() { free(b); }

	int isValid(int x, int y)
	{
		return b && x >= 0 && x < w && y >= 0 && y < h;
	}

	/* Resize keeps the contents that still fit; the caller is expected
	 * to redraw.  The 1998 code reallocates unconditionally, so a
	 * shrink still holds on to the old buffer size until the next
	 * grow - realloc() is free to keep the block.
	 *
	 * The assert below is on line 139 and the one in Set() on line 146
	 * and the one in Copy() on line 158; those three numbers are baked
	 * into xif.o, pcrtc.o and gpu2.o by __LINE__, so do not move any
	 * line above this one.
	 */
	void Resize(int width, int height)
	{
		assert(width > 0 && height > 0);
		w = width;
		h = height;
		b = (unsigned int *)realloc(b, h*4*w);
	}
	void Set(int x, int y, unsigned int c)
	{
		assert(isValid(x, y));
		b[y*w + x] = c;
	}
	/* Copy a width x height block out of src into this frame at (x,y),
	 * clipping to this frame's own extent.
	 *
	 * The assert only catches the source being too small; the
	 * destination is clipped instead. */
	void Copy(int x, int y, int width, int height, const Frame2d &src)
	{
		int i;

		assert(width <= src.w && height <= src.h);
		if (w < x + width)
			width = w - x;
		if (h < y + height)
			height = h - y;
		for (i = 0; i < height; i++)
			memcpy(&b[(y+i)*w + x], &src.b[i*src.w], width*4);
	}
	unsigned int Get(int x, int y)
	{
		assert(isValid(x, y));
		return b[y*w + x];
	}
};

class XWindowDump : public Xifbase {
public:
	Frame2d out;		/* 0x04  what the callback gets */
	Frame2d draw;		/* 0x10  what DrawPixel writes into */
	unsigned int bg;	/* 0x1c */
	void (*func)(int, int, unsigned int *);	/* 0x20 */

	XWindowDump() : out(256, 256), draw(256, 256)
	{
		bg = 0;
		func = 0;
	}
	~XWindowDump() { }

	void PrepareImgBuffer(int width, int height)
	{
		draw.Resize(width, height);
	}
	void DrawPixel(int x, int y, int r, int g, int b)
	{
		draw.Set(x, y, r | (g<<8) | (b<<16));
	}
	void DrawPixel(int x, int y, int r, int g, int b, int a)
	{
		draw.Set(x, y, r | (g<<8) | (b<<16) | (a<<24));
	}
	void DisplayPixel(int x, int y, int width, int height)
	{
		if (out.isValid(x, y))
			out.Copy(x, y, width, height, draw);
		Flush();
	}
	void Resize(int width, int height)
	{
		out.Resize(width, height);
	}
	void ClearDisplay()
	{
		unsigned int c = bg;
		int n = out.w * out.h;
		unsigned int *p = out.b;
		int i;

		switch (n % 8) {
		case 7: *p++ = c;
		case 6: *p++ = c;
		case 5: *p++ = c;
		case 4: *p++ = c;
		case 3: *p++ = c;
		case 2: *p++ = c;
		case 1: *p++ = c;
		}
		for (i = 0; i < n/8; i++) {
			p[0] = c;
			p[1] = c;
			p[2] = c;
			p[3] = c;
			p[4] = c;
			p[5] = c;
			p[6] = c;
			p[7] = c;
			p += 8;
		}
	}
	void SetBackground(int r, int g, int b)
	{
		bg = r | (g<<8) | (b<<16);
	}
	void Flush()
	{
		if (func)
			func(out.w, out.h, out.b);
	}
};

#endif
