/* diff_texfunc - differential test: Sony's texfunc.o vs src/texfunc.c.
 *
 * Both objects are linked into one i386 binary with the (identical)
 * mangled symbol renamed by objcopy (old_/new_, see run_texfunc.sh).
 * TexFunc::Func(PixColor&, PixColor&) writes its result into the second
 * argument; references are pointers at the ABI level.  The illegal-TFX
 * path exits, so only func 0..3 are fed.
 */
#include <stdio.h>
#include <string.h>

typedef struct { int r, g, b, a; } PixColor;
typedef struct { int func, unk4, unk8, tcc; } TexFunc;

/* both objects reference the old libio stderr object */
char _IO_stderr_[256];

extern void old_func(TexFunc *, PixColor *, PixColor *);
extern void new_func(TexFunc *, PixColor *, PixColor *);

static long mismatch, calls;

static void
one(int fn, int tcc, PixColor *t, PixColor *f)
{
	TexFunc tf;
	PixColor a, b;

	tf.func = fn;
	tf.unk4 = 0x1111;
	tf.unk8 = 0x2222;
	tf.tcc = tcc;
	a = *f;
	b = *f;
	old_func(&tf, t, &a);
	new_func(&tf, t, &b);
	calls++;
	if (memcmp(&a, &b, sizeof a) != 0) {
		if (mismatch++ < 8)
			printf("MISMATCH func=%d tcc=%d t=%d,%d,%d,%d "
			    "f=%d,%d,%d,%d: old %d,%d,%d,%d new %d,%d,%d,%d\n",
			    fn, tcc, t->r, t->g, t->b, t->a,
			    f->r, f->g, f->b, f->a,
			    a.r, a.g, a.b, a.a, b.r, b.g, b.b, b.a);
	}
}

static unsigned s = 777777;
static unsigned
rnd(void)
{
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	return s;
}

int
main(void)
{
	PixColor t, f;
	int fn, tcc, i, j;

	/* exhaustive over the 0..255 range for one component pair */
	for (fn = 0; fn < 4; fn++)
		for (tcc = 0; tcc < 2; tcc++)
			for (i = 0; i < 256; i++)
				for (j = 0; j < 256; j += 3) {
					t.r = i; t.g = 255 - i; t.b = j;
					t.a = (i + j) & 0xff;
					f.r = j; f.g = i; f.b = 255 - j;
					f.a = (i ^ j) & 0xff;
					one(fn, tcc, &t, &f);
				}
	/* out-of-range and negative inputs exercise both clamp arms */
	for (i = 0; i < 3000000; i++) {
		t.r = (int)rnd() % 4096 - 2048;
		t.g = (int)rnd() % 4096 - 2048;
		t.b = (int)rnd() % 4096 - 2048;
		t.a = (int)rnd() % 4096 - 2048;
		f.r = (int)rnd() % 4096 - 2048;
		f.g = (int)rnd() % 4096 - 2048;
		f.b = (int)rnd() % 4096 - 2048;
		f.a = (int)rnd() % 4096 - 2048;
		one(rnd() & 3, rnd() & 1, &t, &f);
	}
	/* extreme magnitudes (the >>7 of a full-range product) */
	for (i = 0; i < 500000; i++) {
		t.r = (int)rnd(); t.g = (int)rnd();
		t.b = (int)rnd(); t.a = (int)rnd();
		f.r = (int)rnd(); f.g = (int)rnd();
		f.b = (int)rnd(); f.a = (int)rnd();
		one(rnd() & 3, rnd() & 1, &t, &f);
	}
	printf("%ld calls, %ld mismatches\n", calls, mismatch);
	return mismatch != 0;
}
