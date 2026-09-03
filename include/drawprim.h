/* drawprim - immediate-mode vertex helpers for Sony's host "grfw"
 * (graphics framework) rasterizer plug-in interface.
 *
 * _grfwVertex is grfw's vertex record (host-defined; layout pinned by
 * drawprim.o's field accesses).  z is a DOUBLE (fldl at +0x08) - the
 * only 8-byte field - and r/g/b/a are 0..255 floats (a is rescaled to
 * the GS 0..128 range by PutVertex).
 *
 * grfwSwitchVertex()/grfwSwitchTriangleRasterlizer() (sic - Sony's
 * spelling) let the tool install these callbacks so grfw feeds its
 * vertices straight into the GS model via pGPU2Reg->Put():
 *
 *	Vertex0	first vertex of a primitive: kicks PRIM (tristrip when
 *		type==1, else trifan) with IIP/TME/FGE/AA1 taken from
 *		bits 0-3 of `flag', then sends the vertex
 *	Vertex1/2  further vertices (flag bit 1 = TME again)
 *
 * Each vertex becomes RGBAQ, ST (only when textured) and XYZF2 writes.
 * DrawLine is an unimplemented stub (prints once); DrawTriangle is
 * empty - triangles are rasterized by the model itself through the
 * vertex-kick path, the grfw plug-in slot is just parked.
 */

typedef struct _grfwVertex {
	float x;	/* 0x00 window x */
	float y;	/* 0x04 window y */
	double z;	/* 0x08 depth */
	float r;	/* 0x10 0..255 */
	float g;	/* 0x14 */
	float b;	/* 0x18 */
	float a;	/* 0x1c 0..255 (GS A = a*128/255) */
	float s;	/* 0x20 */
	float t;	/* 0x24 */
	float q;	/* 0x28 */
	float f;	/* 0x2c fog */
} _grfwVertex;

void DrawLine(int n, _grfwVertex *v0, _grfwVertex *v1);
void Vertex0(int type, int flag, _grfwVertex *v);
void Vertex1(int type, int flag, _grfwVertex *v);
void Vertex2(int type, int flag, _grfwVertex *v);
void DrawTriangle(int n, _grfwVertex *v0, _grfwVertex *v1, _grfwVertex *v2);

extern "C" {
void grfwSwitchTriangleRasterlizer(void (*fn)(int, _grfwVertex *,
    _grfwVertex *, _grfwVertex *));
void grfwSwitchVertex(void (*fn0)(int, int, _grfwVertex *),
    void (*fn1)(int, int, _grfwVertex *),
    void (*fn2)(int, int, _grfwVertex *));
}
