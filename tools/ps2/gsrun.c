/* gsrun - replay prepared GS dumps on real GS silicon (DTL-T10000).
 *
 *     dsedb -r run gsrun.elf host0:hwtest            whole suite
 *     dsedb -r run gsrun.elf host0:out/<name>/hw [hold]   one bundle
 *
 * A suite dir (tools/hwtest.py) has an index.txt naming bundle
 * subdirs; a bundle dir has hwstream.bin + vram_hw.bin directly
 * (tools/hwprep.py).  Per bundle: seed the 4 MB of GS local memory
 * (vram_hw.bin is pre-inverse-swizzled so a plain PSMCT32 1024x1024
 * LoadImage lands it byte-identical to the dump's raw layout), stream
 * the GIF records through PATH3 in order, wait a vsync per type-1
 * record, apply the condensed priv-reg snapshots (PMODE/DISPFB/
 * DISPLAY/BGCOLOR) per type-2 record so each frame is visible on the
 * video output, then StoreImage all 4 MB back and write hwvram.bin
 * into the bundle dir; tools/gscmp.py or hwtest.py compare turns that
 * back into raw layout and diffs it against the model snapshot.
 *
 * The stream is UNGATED (gsreplay's MTBA/dialect guards are model
 * workarounds) -- the point is to ask the silicon.
 *
 * "hold" as the second argument keeps redisplaying after a single
 * bundle so the frame can be eyeballed / photographed; otherwise exit
 * (the GS keeps scanning out the last state anyway).
 */
#include <eekernel.h>
#include <eeregs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sifdev.h>
#include <sifrpc.h>
#include <libgraph.h>
#include <libdma.h>

#define VMSIZE   (4 * 1024 * 1024)
#define STRIPH   64                     /* readback/upload strip: 1024x64 */
#define STRIPSZ  (1024 * STRIPH * 4)
#define STREAMMAX (32 * 1024 * 1024)
#define GSREG(off) (*(volatile u_long *)(0x12000000 + (off)))

static u_char *stream, *vbuf;

static void
die(const char *what)
{
	printf("gsrun: %s failed\n", what);
	exit(1);
}

static void *
xmalloc(int n)
{
	char *p = malloc(n + 64);
	if (p == NULL)
		die("malloc");
	return (void *)(((unsigned)p + 63) & ~63);
}

static void
readfull(int fd, void *buf, int n)
{
	char *p = buf;
	while (n > 0) {
		int r = sceRead(fd, p, n > 0x100000 ? 0x100000 : n);
		if (r <= 0)
			die("sceRead");
		p += r; n -= r;
	}
}

static void
run_one(const char *dir)
{
	char path[256];
	u_char *p, *end;
	u_int hdr[4], nrec, smode2, slen;
	int fd, y, inter, ffmd;
	long nxfer = 0, nvsync = 0, npriv = 0, nqwtot = 0;
	sceDmaChan *gif;
	sceGsLoadImage li;
	sceGsStoreImage si;

	sprintf(path, "%s/hwstream.bin", dir);
	if ((fd = sceOpen(path, SCE_RDONLY)) < 0)
		die(path);
	slen = sceLseek(fd, 0, SCE_SEEK_END);
	sceLseek(fd, 0, SCE_SEEK_SET);
	if (slen > STREAMMAX)
		die("stream larger than STREAMMAX");
	readfull(fd, stream, slen);
	sceClose(fd);
	if (memcmp(stream, "GSHW", 4) != 0)
		die("hwstream.bin magic");
	memcpy(hdr, stream, 16);
	nrec = hdr[1]; smode2 = hdr[2];
	inter = (hdr[3] & 1) ? (smode2 & 1) : 1;
	ffmd  = (hdr[3] & 1) ? (smode2 >> 1) & 1 : 0;
	printf("gsrun: %s  %u records %u bytes  inter=%d ffmd=%d\n",
	    dir, nrec, slen, inter, ffmd);

	sceGsResetGraph(0, inter ? SCE_GS_INTERLACE : SCE_GS_NOINTERLACE,
	    SCE_GS_NTSC, ffmd ? SCE_GS_FRAME : SCE_GS_FIELD);

	/* ---- seed local memory ---- */
	sprintf(path, "%s/vram_hw.bin", dir);
	if ((fd = sceOpen(path, SCE_RDONLY)) < 0)
		die(path);
	readfull(fd, vbuf, VMSIZE);
	sceClose(fd);
	for (y = 0; y < 1024; y += STRIPH) {
		sceGsSetDefLoadImage(&li, 0, 16, SCE_GS_PSMCT32,
		    0, y, 1024, STRIPH);
		/* the GIFtag packet in li is cached: flush AFTER building it,
		 * before the DMA reads it from physical memory */
		FlushCache(0);
		sceGsExecLoadImage(&li, (u_long128 *)(vbuf + y * 4096));
		sceGsSyncPath(0, 0);
	}

	/* ---- replay ---- */
	gif = sceDmaGetChan(SCE_DMA_GIF);
	p = stream + 16;
	end = stream + slen;
	while (p + 16 <= end) {
		int type = p[0];
		u_int nqw;
		memcpy(&nqw, p + 4, 4);
		p += 16;
		if (type == 0) {
			u_char *q = p;
			u_int left = nqw;
			while (left > 0) {
				u_int n = left > 16384 ? 16384 : left;
				sceDmaSendN(gif, q, n);
				sceDmaSync(gif, 0, 0);
				q += n * 16; left -= n;
			}
			nxfer++; nqwtot += nqw;
			p += nqw * 16;
		} else if (type == 1) {
			sceGsSyncV(0);
			nvsync++;
		} else if (type == 2) {
			u_long *r = (u_long *)p;
			GSREG(0x00) = r[0];             /* PMODE    */
			GSREG(0x70) = r[2];             /* DISPFB1  */
			GSREG(0x80) = r[3];             /* DISPLAY1 */
			GSREG(0x90) = r[4];             /* DISPFB2  */
			GSREG(0xa0) = r[5];             /* DISPLAY2 */
			GSREG(0xe0) = r[6];             /* BGCOLOR  */
			npriv++;
			p += nqw * 16;
		} else {
			printf("gsrun: bad record type %d\n", type);
			exit(1);
		}
	}
	sceGsSyncPath(0, 0);
	printf("gsrun: done xfer=%ld (%ld qw) vsync=%ld priv=%ld\n",
	    nxfer, nqwtot, nvsync, npriv);

	/* ---- read back all 4 MB ---- */
	SyncDCache(vbuf, vbuf + VMSIZE);
	for (y = 0; y < 1024; y += STRIPH) {
		sceGsSetDefStoreImage(&si, 0, 16, SCE_GS_PSMCT32,
		    0, y, 1024, STRIPH);
		FlushCache(0);
		sceGsExecStoreImage(&si, (u_long128 *)(vbuf + y * 4096));
		sceGsSyncPath(0, 0);
	}
	InvalidDCache(vbuf, vbuf + VMSIZE);
	sprintf(path, "%s/hwvram.bin", dir);
	if ((fd = sceOpen(path, SCE_WRONLY | SCE_CREAT | SCE_TRUNC)) < 0)
		die(path);
	for (y = 0; y < VMSIZE; y += STRIPSZ)
		if (sceWrite(fd, vbuf + y, STRIPSZ) != STRIPSZ)
			die("sceWrite");
	sceClose(fd);
	printf("gsrun: wrote %s\n", path);
}

int
main(int argc, char *argv[])
{
	const char *dir = argc > 1 ? argv[1] : "host0:hwtest";
	char path[256], sub[256];
	int fd, n = 0;

	sceSifInitRpc(0);
	sceDmaReset(1);
	stream = xmalloc(STREAMMAX);
	vbuf = xmalloc(VMSIZE);

	sprintf(path, "%s/index.txt", dir);
	if ((fd = sceOpen(path, SCE_RDONLY)) >= 0) {
		/* suite: index.txt names one bundle subdir per line */
		static char idx[65536];
		int len = sceRead(fd, idx, sizeof idx - 1), i, j = 0;
		sceClose(fd);
		if (len < 0)
			die("read index.txt");
		idx[len] = 0;
		for (i = 0; i <= len; i++) {
			if (idx[i] != '\n' && idx[i] != 0) {
				sub[j++] = idx[i];
				continue;
			}
			sub[j] = 0; j = 0;
			if (sub[0] == 0)
				continue;
			sprintf(path, "%s/%s", dir, sub);
			run_one(path);
			n++;
		}
		printf("gsrun: suite done, %d frames\n", n);
	} else {
		run_one(dir);
		if (argc > 2 && strcmp(argv[2], "hold") == 0)
			for (;;)
				sceGsSyncV(0);
	}
	return 0;
}
