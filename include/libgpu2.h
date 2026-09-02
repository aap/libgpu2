
#define LIBGPU2_VERSION "Ver1.12.0"

typedef struct {
	int fbp;
	int fbw;
	int psm;
	int posx, posy;
	int width, height;
} FRAME_BUFFER;

void GS_InitSim(void);
void GS_OpenSim(char *title, int width, int height, int disp_on, int field);
void GS_CloseSim(void);
void GS_PutPort(int addr, long long data);
int GS_PutCtlPort(int addr, long long data);
int GS_SaveImage(char *filename);
void GS_SetSaveImageArea(FRAME_BUFFER *fbinfo);
void GS_GetSaveImageArea(FRAME_BUFFER *fbinfo);
