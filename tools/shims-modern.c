/*
 * shims-modern.c - the small runtime the modern (non-era) build of the
 * model needs.  Compiled as C++ by tools/build-modern.sh, for both -m32
 * and -m64.  tools/shims.c is the equivalent for the 1998 objects and is
 * left untouched.
 *
 * Three jobs, and only three:
 *
 *  1. operator new / delete.  The era objects called libg++ 2.7's
 *     __builtin_new; a modern build calls the ISO operators instead.
 *     Defining them here (a) keeps the big-allocation hook the harness
 *     uses to find the model's 4 MB local memory, and (b) means the link
 *     needs no libstdc++ at all -- which matters on hosts that have no
 *     32-bit libstdc++.  Same contract as shims.c: every allocation of
 *     4 MB or more is recorded, in the order the ctors run.  GPU2::GPU2
 *     makes exactly one -- `new Memory', 0x4001c8, the 4 MB of local
 *     memory plus the configs and the trailing BitBLT (memory.h) -- and
 *     that is the pointer probe.c and gsreplay.c read VRAM through.
 *
 *  2. __cxa_pure_virtual, the vtable terminator (libstdc++'s job).
 *
 *  3. The i386 64-bit divide helpers.  A non-multilib gcc has no 32-bit
 *     libgcc, so -m32 has to bring its own __divdi3.  On amd64 the
 *     hardware divides 64 bits and none of this is referenced.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

/* --- big-allocation hook (same names/types tools/probe.c declares) --- */
void *lg2_bigalloc[8];
unsigned int lg2_bigsize[8];
int lg2_nbig;

static void *
lg2_alloc(size_t sz)
{
	void *p = malloc(sz ? sz : 1);

	if (p == 0)
		abort();
	if (sz >= 0x400000 && lg2_nbig < 8) {
		lg2_bigalloc[lg2_nbig] = p;
		lg2_bigsize[lg2_nbig] = (unsigned int)sz;
		lg2_nbig++;
	}
	return p;
}

void *operator new(size_t sz)             { return lg2_alloc(sz); }
void *operator new[](size_t sz)           { return lg2_alloc(sz); }
void operator delete(void *p)             { if (p) free(p); }
void operator delete[](void *p)           { if (p) free(p); }
void operator delete(void *p, size_t)     { if (p) free(p); }
void operator delete[](void *p, size_t)   { if (p) free(p); }

extern "C" void
__cxa_pure_virtual(void)
{
	fputs("pure virtual function called\n", stderr);
	abort();
}

/* --- i386 64-bit divide helpers (no 32-bit libgcc on a non-multilib gcc).
 * Shift-subtract, written so the compiler emits no 64-bit divide libcall
 * that would recurse straight back into these.  Same shape as shims.c,
 * except that __moddi3 here returns the remainder (shims.c's returns the
 * quotient -- an old bug that never bit because nothing calls it). */
#if defined(__i386__)
typedef unsigned long long u64;
typedef long long s64;

static u64
udiv64(u64 n, u64 d)
{
	u64 q = 0, r = 0;
	int i;

	if (d == 0 || d > n)
		return 0;
	for (i = 63; i >= 0; i--) {
		r = (r << 1) | ((n >> i) & 1);
		if (r >= d) {
			r -= d;
			q |= (u64)1 << i;
		}
	}
	return q;
}

static u64
mag64(s64 v)
{
	return (v < 0) ? (u64)(~v) + 1u : (u64)v;
}

extern "C" u64 __udivdi3(u64 n, u64 d) { return udiv64(n, d); }
extern "C" u64 __umoddi3(u64 n, u64 d) { return n - udiv64(n, d) * d; }

extern "C" s64
__divdi3(s64 n, s64 d)
{
	int neg = (n < 0) ^ (d < 0);
	s64 q = (s64)udiv64(mag64(n), mag64(d));

	return neg ? -q : q;
}

extern "C" s64
__moddi3(s64 n, s64 d)
{
	u64 a = mag64(n), b = mag64(d);
	s64 r = (s64)(a - udiv64(a, b) * b);

	return (n < 0) ? -r : r;
}
#endif	/* __i386__ */
