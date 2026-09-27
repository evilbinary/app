#ifndef __DINGOO_VIDEO__
#define __DINGOO_VIDEO__


#define RES_HW_SCREEN_VERTICAL 		240
#define RES_HW_SCREEN_HORIZONTAL 	320

uint32 PtoV(uint16 x, uint16 y);
bool FCEUD_ShouldDrawInputAids();
unsigned short *FCEUD_GetScreen();

#define dingoo_video_color15(R,G,B) ((((R)&0xF8)<<8)|(((G)&0xFC)<<3)|(((B)&0xF8)>>3))

extern void dingoo_clear_video(void);

#endif // __DINGOO_VIDEO__
