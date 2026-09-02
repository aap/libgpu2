/* TexFunc::Func - texture function (TFX) applied to one fragment.
 *
 * Every component runs the same clamp idiom; the shape
 *   x = <expr>; if (x < 0) v = 0; else { v = x; if (v > 255) v = 255; }
 * is pinned by the object: the 0-arm is a separate block that gcc
 * threads past the 255 test, which is why every positive arm is a
 * 16-byte-aligned label (the 0x90 runs inside the function).
 */
#include <stdio.h>
#include <stdlib.h>

#include "texfunc.h"

void
TexFunc::Func(PixColor &t, PixColor &f)
{
	int r, g, b, a, x;

	switch (func) {
	case 0:
		x = (t.r * f.r) >> 7;
		if (x < 0)
			r = 0;
		else {
			r = x;
			if (r > 255)
				r = 255;
		}
		x = (t.g * f.g) >> 7;
		if (x < 0)
			g = 0;
		else {
			g = x;
			if (g > 255)
				g = 255;
		}
		x = (t.b * f.b) >> 7;
		if (x < 0)
			b = 0;
		else {
			b = x;
			if (b > 255)
				b = 255;
		}
		f.r = r;
		f.g = g;
		f.b = b;
		if (tcc) {
			x = (t.a * f.a) >> 7;
			if (x < 0)
				a = 0;
			else {
				a = x;
				if (a > 255)
					a = 255;
			}
		} else
			a = f.a;
		f.a = a;
		break;
	case 1:
		if (tcc == 0) {
			r = t.r;
			g = t.g;
			b = t.b;
			a = f.a;
		} else {
			r = t.r;
			g = t.g;
			b = t.b;
			a = t.a;
		}
		f.r = r;
		f.g = g;
		f.b = b;
		f.a = a;
		break;
	case 2:
		x = ((t.r * f.r) >> 7) + f.a;
		if (x < 0)
			r = 0;
		else {
			r = x;
			if (r > 255)
				r = 255;
		}
		x = ((t.g * f.g) >> 7) + f.a;
		if (x < 0)
			g = 0;
		else {
			g = x;
			if (g > 255)
				g = 255;
		}
		x = ((t.b * f.b) >> 7) + f.a;
		if (x < 0)
			b = 0;
		else {
			b = x;
			if (b > 255)
				b = 255;
		}
		f.r = r;
		f.g = g;
		f.b = b;
		if (tcc) {
			x = t.a + f.a;
			if (x < 0)
				a = 0;
			else {
				a = x;
				if (a > 255)
					a = 255;
			}
		} else
			a = f.a;
		f.a = a;
		break;
	case 3:
		x = ((t.r * f.r) >> 7) + f.a;
		if (x < 0)
			r = 0;
		else {
			r = x;
			if (r > 255)
				r = 255;
		}
		x = ((t.g * f.g) >> 7) + f.a;
		if (x < 0)
			g = 0;
		else {
			g = x;
			if (g > 255)
				g = 255;
		}
		x = ((t.b * f.b) >> 7) + f.a;
		if (x < 0)
			b = 0;
		else {
			b = x;
			if (b > 255)
				b = 255;
		}
		f.r = r;
		f.g = g;
		f.b = b;
		if (tcc == 0)
			a = f.a;
		else
			a = t.a;
		f.a = a;
		break;
	default:
		fprintf(stderr, "TXM: Illegal Texture function\n");
		exit(0);
	}
}
