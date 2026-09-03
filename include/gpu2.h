/* GPU2 - the whole GS model behind one object.
 *
 * The full definition (the pipeline stage types, the constructor and the
 * method bodies) is src/gpu2.c; this declaration is what libgpu2.c (the
 * public API layer) compiles against, so only the size (0x18, the
 * `__builtin_new' argument in GS_OpenSim), the member offsets and the
 * mangled names matter here:
 *
 *	__4GPU2Pciii		GPU2::GPU2(char*, int, int, int)
 *	Put__4GPU2ix		GPU2::Put(int, long long)
 *	Get__4GPU2		GPU2::Get()
 *	GetCRT__4GPU2		GPU2::GetCRT()
 *	ResizeWindow__4GPU2ii	GPU2::ResizeWindow(int, int)
 *
 * Layout verified against GPU2::GPU2/Put/Get in orig/lib/gpu2.o
 * (doc/STRUCTS.md, doc/notes/gpu2.md).  Put returns int (the 1998 code
 * always sets %eax to 1); GetCRT returns one captured display pixel per
 * call and 0 when the frame is exhausted.
 */

class PP;		/* the 0x10 front-end block: Pre1/Pre3/PCalc/PPOut */
class DDA;
class TXM;
class MemIF;
class Memory;		/* 0x4001c8: 4 MB VRAM + configs + trailing BitBLT */
class PCRTC;

class GPU2 {
public:
	PP *pp;		/* +0x00 */
	DDA *dda;	/* +0x04 */
	TXM *txm;	/* +0x08 */
	MemIF *memif;	/* +0x0c */
	Memory *mem;	/* +0x10 */
	PCRTC *pcrtc;	/* +0x14 PCRTCdmy (0), PCRTCxif (1, 2) */

	GPU2(char *title, int width, int height, int disp_on);
	int Put(int addr, long long data);
	long long Get();
	unsigned int GetCRT();
	void ResizeWindow(int w, int h);
};
