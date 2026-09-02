/* GPU2 - the whole GS model behind one object.
 *
 * libgpu2.c (the public API layer) only ever holds a GPU2* and calls three
 * of its members, so this declaration is deliberately minimal: it fixes
 * sizeof(GPU2) (0x18, the `__builtin_new' argument in GS_OpenSim) and the
 * mangled names of the members libgpu2.o references
 *
 *	__4GPU2Pciii	GPU2::GPU2(char*, int, int, int)
 *	Put__4GPU2ix	GPU2::Put(int, long long)
 *	Get__4GPU2	GPU2::Get()
 *
 * The member layout below is the one doc/STRUCTS.md pins from GPU2::GPU2
 * in gpu2.o; libgpu2.c never touches a field, so only the total size is
 * load-bearing here.  The real class (with the rest of its interface,
 * GetCRT/ResizeWindow, and the pipeline stage types) belongs to gpu2.c.
 */

class Pre1;		/* really the unnamed 0x10-byte front-end block */
class DDA;
class TXM;
class MemIF;
class Memory;
class PCRTC;

class GPU2 {
	Pre1 *pp;	/* +0x00 front-end block (Pre1/Pre3/PCalc/PPOut) */
	DDA *dda;	/* +0x04 */
	TXM *txm;	/* +0x08 */
	MemIF *memif;	/* +0x0c */
	Memory *mem;	/* +0x10 Memory + trailing BitBLT */
	PCRTC *pcrtc;	/* +0x14 PCRTCdmy or PCRTCxif */
public:
	GPU2(char *title, int width, int height, int disp_on);
	void Put(int addr, long long data);
	long long Get();
};
