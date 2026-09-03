/* modern.h - compatibility shim for building the 1998 GS model with a
 * modern compiler.  Force-included (-include include/modern.h) by
 * tools/build-modern.sh; the era compiler NEVER sees this file, so
 * nothing in it can perturb the byte-matching build.
 *
 * It has exactly one job: the string functions.  The era's libc5
 * <stdio.h>/<stdlib.h> leaked memcpy/strcpy, today's do not, and
 * include/xif.h (Frame2d::Copy) and src/gpu2reg.c rely on that.  Adding
 * an #include to those files would move the assert __LINE__s that xif.h,
 * pcrtc.h, txm.h and gpu2vec.h bake into their objects, so the
 * declaration is injected from the command line instead.
 *
 * The model needed no ILP32 shim: a `long' audit found only Xlib's own
 * `unsigned long' masks in xif, and no pointer-to-integer casts at all.
 * Where the 1998 layout does leak into a modern build - per-TU views of
 * a neighbouring class, written as 1998 byte offsets - it is fixed at
 * the declaration.  See doc/notes/modern-port.md.
 */
#ifndef LIBGPU2_MODERN_H
#define LIBGPU2_MODERN_H

#if __GNUC__ >= 3 || defined(__clang__)
#include <string.h>
#endif

#endif	/* LIBGPU2_MODERN_H */
