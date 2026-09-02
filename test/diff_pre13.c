/* Differential test: reconstructed pre1.o+pre3.o against the 1998 objects.
 *
 * Both stage pairs are driven with the same register-write stream through
 * Pre1::Put.  Pre3 forwards to PCalc through its vtable; we install a fake
 * PCalc whose single virtual entry snapshots the whole Pre3 object, so
 * every value the real PCalc would have seen is compared.
 *
 * Built by test/run_pre13.sh, which renames the 1998 symbols to o_* and the
 * reconstructed ones to n_* first.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* glibc 2.0 exported the stderr FILE object itself as _IO_stderr_; the only
   reference to it is MaxExp()'s unreachable "illegal primitive type" exit. */
char _IO_stderr_[1024];

#define PRE1_SIZE	0xac
#define PRE3_SIZE	0x14c
#define PRE3_VPTR	0x148
#define PCALC_SIZE	0xc00
#define PCALC_VPTR	0xbfc

/* g++ 2.7 non-thunk vtable: 8 zero bytes, then {short delta; short pad; fn} */
struct vtable {
	int zero0, zero1;
	short delta;
	short pad;
	void (*fn)(void *pcalc, void *pre3);
};

extern void *o_pre1_ctor(void *pre1, void *pre3);
extern void *o_pre3_ctor(void *pre3, void *pcalc);
extern void o_pre1_put(void *pre1, int addr, long long data);
extern char o_vt_pre3[];

extern void *n_pre1_ctor(void *pre1, void *pre3);
extern void *n_pre3_ctor(void *pre3, void *pcalc);
extern void n_pre1_put(void *pre1, int addr, long long data);
extern char n_vt_pre3[];

/* The two instances legitimately differ in their pointer fields:
   Pre1+0x00 = Pre3*, Pre3+0x00 = PCalc*, Pre3+0x148 = its own _vt.4Pre3. */
static int
cmp_pre1(const void *a, const void *b)
{
	return memcmp((const char *)a + 4, (const char *)b + 4, PRE1_SIZE - 4);
}

static int
cmp_pre3(const void *a, const void *b)
{
	return memcmp((const char *)a + 4, (const char *)b + 4, PRE3_VPTR - 4);
}

/* capture buffers */
#define MAXCAP	64
static unsigned char cap[2][MAXCAP][PRE3_SIZE];
static int ncap[2];
static int side;

static void
capture(void *pcalc, void *pre3)
{
	(void)pcalc;
	if (ncap[side] < MAXCAP)
		memcpy(cap[side][ncap[side]], pre3, PRE3_SIZE);
	ncap[side]++;
}

static struct vtable pcalc_vt;

struct stage {
	void *pre1;
	void *pre3;
	void *pcalc;
};

static void
build(struct stage *s, void *(*p1ctor)(void *, void *),
	void *(*p3ctor)(void *, void *), char *vt)
{
	s->pcalc = calloc(1, PCALC_SIZE);
	s->pre3 = calloc(1, PRE3_SIZE);
	s->pre1 = calloc(1, PRE1_SIZE);
	*(void **)((char *)s->pcalc + PCALC_VPTR) = &pcalc_vt;
	p3ctor(s->pre3, s->pcalc);
	/* the ctor stores its own _vt.4Pre3; make sure it is the right one */
	*(void **)((char *)s->pre3 + PRE3_VPTR) = vt;
	p1ctor(s->pre1, s->pre3);
}

static unsigned int seed = 12345;

static unsigned int
rnd(void)
{
	seed = seed * 1103515245 + 12345;
	return seed >> 8;
}

int
main(void)
{
	struct stage o, n;
	int i, j, k, bad = 0, cmps = 0;
	int addr;
	long long data;
	static const int regs[] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x0a, 0x0c, 0x0d,
		0x11, 0x12, 0x13, 0x18, 0x19, 0x1a, 0x1b, 0x06, 0x14, 0x22
	};

	pcalc_vt.zero0 = pcalc_vt.zero1 = 0;
	pcalc_vt.delta = 0;
	pcalc_vt.pad = 0;
	pcalc_vt.fn = capture;

	build(&o, o_pre1_ctor, o_pre3_ctor, o_vt_pre3);
	build(&n, n_pre1_ctor, n_pre3_ctor, n_vt_pre3);

	if (cmp_pre1(o.pre1, n.pre1) != 0) {
		printf("FAIL: Pre1 constructors differ\n");
		bad++;
	}
	if (cmp_pre3(o.pre3, n.pre3) != 0) {
		printf("FAIL: Pre3 constructors differ\n");
		bad++;
	}

	for (i = 0; i < 2000000; i++) {
		if ((i & 15) == 0) {
			/* Start a fresh primitive fairly often.  PRIM 7 is
			   reserved; both MaxExp()s would print and exit(1). */
			addr = 0x00;
			data = rnd() % 7;
			data |= (long long)(rnd() & 0x7ff) << 3;
		} else if ((i & 63) == 7) {
			/* anything else must travel down as a register write */
			addr = 0x1c + rnd() % 0xe0;	/* pass-down range */
			data = ((long long)rnd() << 32) ^ rnd();
		} else {
			addr = regs[rnd() % (sizeof regs / sizeof regs[0])];
			data = ((long long)rnd() << 32) ^ ((long long)rnd() << 11)
				^ rnd();
		}

		ncap[0] = ncap[1] = 0;
		side = 0;
		o_pre1_put(o.pre1, addr, data);
		side = 1;
		n_pre1_put(n.pre1, addr, data);

		if (ncap[0] != ncap[1]) {
			printf("FAIL %d: addr %#x: %d vs %d PCalc::Put calls\n",
				i, addr, ncap[0], ncap[1]);
			if (++bad > 10)
				break;
			continue;
		}
		for (j = 0; j < ncap[0] && j < MAXCAP; j++) {
			cmps++;
			if (cmp_pre3(cap[0][j], cap[1][j]) == 0)
				continue;
			printf("FAIL %d: addr %#x data %016llx call %d:\n",
				i, addr, (unsigned long long)data, j);
			for (k = 4; k < PRE3_VPTR; k += 4)
				if (memcmp(cap[0][j] + k, cap[1][j] + k, 4))
					printf("   Pre3+%#05x: %08x vs %08x\n", k,
						*(unsigned *)(cap[0][j] + k),
						*(unsigned *)(cap[1][j] + k));
			if (++bad > 10)
				goto done;
		}
		/* the persistent state of both stages must stay in lockstep */
		if (cmp_pre1(o.pre1, n.pre1) != 0) {
			printf("FAIL %d: Pre1 state diverged (addr %#x)\n", i, addr);
			if (++bad > 10)
				break;
		}
		if (cmp_pre3(o.pre3, n.pre3) != 0) {
			printf("FAIL %d: Pre3 state diverged (addr %#x)\n", i, addr);
			if (++bad > 10)
				break;
		}
	}
done:
	printf("%d register writes, %d PCalc::Put snapshots compared, %d failures\n",
		i, cmps, bad);
	return bad != 0;
}
