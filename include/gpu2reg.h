/* gpu2reg - the jtcl register-console surface of the GS model.
 *
 * GPU2Reg is an ABSTRACT interface class: all three virtuals are pure
 * (gpu2reg.o's local `_vt.7GPU2Reg' carries three __pure_virtual
 * entries), and neither gpu2.o nor gpu2vec.o emits a GPU2Reg vtable, so
 * the concrete subclass lived in Sony's HOST TOOL (gpu2/gpu2vec
 * application), not in the archive: the tool wrapped its GPU2/GPU2VEC
 * in a GPU2Reg-derived adapter and constructed it, which registered the
 * 82 jtcl commands and the grfw vertex callbacks (see the constructor
 * in src/gpu2reg.c).  Everything in this layer reaches the model
 * through pGPU2Reg's three virtuals:
 *
 *	Put(addr, data)		a register write (GS_PutPort equivalent;
 *				addr 0x100/0x101 are pseudo-registers, see
 *				doc/notes/gpu2reg.md)
 *	Get()			one 64-bit Local->Host transfer word
 *				(GPU2::Get / BitBLT::ReadPixel path)
 *	GetCRT()		one displayed-CRT pixel (GPU2::GetCRT)
 *
 * vtable order = declaration order: Put +0x08, Get +0x10, GetCRT +0x18.
 *
 * jtcl_data_t is the parsed-argument record of Sony's tool-side Tcl
 * ("jtcl") binding: 8 bytes per argument, the value union at +4 (only
 * +4 offsets are ever read; the first word is presumed the type tag).
 * The command table entry (our jtcl_cmd_t; real Sony name unknown) is
 * {name, handler, help, 0} - the help string is the argument signature
 * ("9I", "4IF", "S2I", ...).  RegisterCommands() and the interpreter
 * handle tcl_ip come from the host framework; tcl_ip->result (offset 0,
 * as in real Tcl_Interp) receives error messages via sprintf.
 *
 * ImageData is the host tool's image-file record (OpenImageFile/
 * SaveImageFile/FreeImage/ConvImage24to8/ConvImage8to24 operate on it);
 * layout is pinned by gpu2reg.o at sizeof 0x224 (the `*img = tmp'
 * struct copy is a 0x89-dword rep movsl).
 */

typedef struct {
	int type;
	union {
		int i;
		float f;
		char *s;
	} d;
} jtcl_data_t;

typedef struct {
	char *name;
	int (*func)(int, char *, jtcl_data_t *);
	char *help;
	char *pad;
} jtcl_cmd_t;

typedef struct {
	char *result;
} jtcl_interp_t;

struct ImageData {
	char name[512];		/* 0x000 file name */
	int type;		/* 0x200 always written 2 */
	int format;		/* 0x204 1=RGB24, 2=RGBA32(ARGB), 3=IDX8 */
	int width;		/* 0x208 */
	int height;		/* 0x20c */
	int ncolor;		/* 0x210 (never touched here) */
	unsigned char *r;	/* 0x214 palette red */
	unsigned char *g;	/* 0x218 palette green */
	unsigned char *b;	/* 0x21c palette blue */
	unsigned char *pixel;	/* 0x220 pixel data */
};				/* sizeof 0x224 */

extern "C" {
extern jtcl_interp_t *tcl_ip;
void RegisterCommands(jtcl_cmd_t *tbl);
int OpenImageFile(ImageData *img);
int SaveImageFile(ImageData *img);
void FreeImage(ImageData *img);
void ConvImage24to8(ImageData *img, ImageData *idx, int ncolor, int mode);
void ConvImage8to24(ImageData *idx, ImageData *img);
}

class GPU2Reg {
public:
	GPU2Reg(void);
	virtual void Put(int addr, long long data) = 0;
	virtual long long Get(void) = 0;
	virtual int GetCRT(void) = 0;
};

extern GPU2Reg *pGPU2Reg;

/* float -> raw GS register bits; fully inlined everywhere (no symbol in
 * either object).  The address-taken parameter is what pins the pun to
 * one stack slot per call. */
static inline int
FtoI(float f)
{
	return *(int *)&f;
}
