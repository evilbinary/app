/*===================================================================*/
/*                                                                   */
/*  InfoNES_System_Linux.cpp : Linux specific File                   */
/*                                                                   */
/*  2001/05/18  InfoNES Project ( Sound is based on DarcNES )        */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "InfoNES.h"
#include "InfoNES_System.h"
#include "InfoNES_pAPU.h"
#include "screen.h"

// bool define
#define TRUE 1
#define FALSE 0

/* lcd 操作相关 头文件 */
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

static int fb_fd = -1;
static unsigned char *fb_mem;
static int px_width;
static int line_width;
static int screen_width;
static int lcd_width;
static int lcd_height;

static int *zoom_x_tab;
static int *zoom_y_tab;

extern int InitJoypadInput(void);
extern int GetJoypadInput(void);

static int lcd_fb_display_px(WORD color, int x, int y) {
  // 565 -> 888
  u32 c;
  u8 R, G, B;
  R = (color >> 11 & 0xff);
  G = (color >> 5 & 0x3f);
  B = (color & 0x1f);
  c = (R << 16) | (G << 8) | B;

  // u8 r,g,b;
  // r = (((color & 0xF800) >> 11) << 3);
  // g = (((color & 0x7E0) >> 5) << 2);
  // b = (((color & 0x1F)) << 3);
  
  // u8 *dst=&c;
  // dst[0]=b;
  // dst[1]=g;
  // dst[2]=r;
  // dst[3]=0xff;
  screen_put_pixel(x, y,c);
  return 0;
}
screen_info_t *screen=NULL;
static int lcd_fb_init() {
  screen_init();
  screen = screen_info();
  fb_mem = (unsigned char *)screen->fb.frambuffer;
  fb_fd = screen->fd;
  lcd_height = NES_DISP_HEIGHT;
  lcd_width = NES_DISP_WIDTH;
  return 0;
}

/**
 * 生成zoom 缩放表
 */
int make_zoom_tab() {
  int i;
  zoom_x_tab = (int *)malloc(sizeof(int) * lcd_width);

  if (NULL == zoom_x_tab) {
    printf("make zoom_x_tab error\n");
    return -1;
  }
  for (i = 0; i < lcd_width; i++) {
    zoom_x_tab[i] = i * NES_DISP_WIDTH / lcd_width;
  }
  zoom_y_tab = (int *)malloc(sizeof(int) * lcd_height);
  if (NULL == zoom_y_tab) {
    printf("make zoom_y_tab error\n");
    return -1;
  }
  for (i = 0; i < lcd_height; i++) {
    zoom_y_tab[i] = i * NES_DISP_HEIGHT / lcd_height;
  }
  return 1;
}

/*-------------------------------------------------------------------*/
/*  ROM image file information                                       */
/*-------------------------------------------------------------------*/

char szRomName[256];
char szSaveName[256];
int nSRAM_SaveFlag;

/*-------------------------------------------------------------------*/
/*  Constants ( Linux specific )                                     */
/*-------------------------------------------------------------------*/

#define VBOX_SIZE 7
#define SOUND_DEVICE "/dev/dsp"
#define VERSION "InfoNES v0.91J"

/*-------------------------------------------------------------------*/
/*  Global Variables ( Linux specific )                              */
/*-------------------------------------------------------------------*/

/* Emulation thread */
pthread_t emulation_tid;
int bThread;

/* Pad state */
DWORD dwKeyPad1;
DWORD dwKeyPad2;
DWORD dwKeySystem;

/* For Sound Emulation */
BYTE final_wave[2048];
int waveptr;
int wavflag;
int sound_fd;

/*-------------------------------------------------------------------*/
/*  Function prototypes ( Linux specific )                           */
/*-------------------------------------------------------------------*/

void *emulation_thread(void *args);

void start_application(char *filename);

int LoadSRAM();

int SaveSRAM();

DWORD RGBPalette[64] = {
	0xff707070,0xff201888,0xff0000a8,0xff400098,0xff880070,0xffa80010,0xffa00000,0xff780800,
	0xff402800,0xff004000,0xff005000,0xff003810,0xff183858,0xff000000,0xff000000,0xff000000,
	0xffb8b8b8,0xff0070e8,0xff2038e8,0xff8000f0,0xffb800b8,0xffe00058,0xffd82800,0xffc84808,
	0xff887000,0xff009000,0xff00a800,0xff009038,0xff008088,0xff000000,0xff000000,0xff000000,
	0xfff8f8f8,0xff38b8f8,0xff5890f8,0xff4088f8,0xfff078f8,0xfff870b0,0xfff87060,0xfff89838,
	0xfff0b838,0xff80d010,0xff48d848,0xff58f898,0xff00e8d8,0xff000000,0xff000000,0xff000000,
	0xfff8f8f8,0xffa8e0f8,0xffc0d0f8,0xffd0c8f8,0xfff8c0f8,0xfff8c0d8,0xfff8b8b0,0xfff8d8a8,
	0xfff8e0a0,0xffe0f8a0,0xffa8f0b8,0xffb0f8c8,0xff98f8f0,0xff000000,0xff000000,0xff000000,
};

/* Palette data */
WORD NesPalette[64] = {
    // 0x738E,0x88C4,0xA800,0x9808,0x7011,0x1015,0x0014,0x004F,
    // 0x0148,0x0200,0x0280,0x11C0,0x59C3,0x0000,0x0000,0x0000,
    // 0xBDD7,0xEB80,0xE9C4,0xF010,0xB817,0x581C,0x015B,0x0A59,
    // 0x0391,0x0480,0x0540,0x3C80,0x8C00,0x0000,0x0000,0x0000,
    // 0xFFDF,0xFDC7,0xFC8B,0xFC48,0xFBDE,0xB39F,0x639F,0x3CDF,
    // 0x3DDE,0x1690,0x4EC9,0x9FCB,0xDF40,0x0000,0x0000,0x0000,
    // 0xFFDF,0xFF15,0xFE98,0xFE5A,0xFE1F,0xDE1F,0xB5DF,0xAEDF,
    // 0xA71F,0xA7DC,0xBF95,0xCFD6,0xF7D3,0x0000,0x0000,0x0000,

    0x39ce, 0x1071, 0x0015, 0x2013, 0x440e, 0x5402, 0x5000, 0x3c20,
    0x20a0, 0x0100, 0x0140, 0x00e2, 0x0ceb, 0x0000, 0x0000, 0x0000,
    0x5ef7, 0x01dd, 0x10fd, 0x401e, 0x5c17, 0x700b, 0x6ca0, 0x6521,
    0x45c0, 0x0240, 0x02a0, 0x0247, 0x0211, 0x0000, 0x0000, 0x0000,
    0x7fff, 0x1eff, 0x2e5f, 0x223f, 0x79ff, 0x7dd6, 0x7dcc, 0x7e67,
    0x7ae7, 0x4342, 0x2769, 0x2ff3, 0x03bb, 0x0000, 0x0000, 0x0000,
    0x7fff, 0x579f, 0x635f, 0x6b3f, 0x7f1f, 0x7f1b, 0x7ef6, 0x7f75,
    0x7f94, 0x73f4, 0x57d7, 0x5bf9, 0x4ffe, 0x0000, 0x0000, 0x0000
    };

/*===================================================================*/
/*                                                                   */
/*                main() : Application main                          */
/*                                                                   */
/*===================================================================*/

/* Application main */
int main(int argc, char **argv) {
  char cmd;

  /*-------------------------------------------------------------------*/
  /*  Pad Control                                                      */
  /*-------------------------------------------------------------------*/

  /* Initialize a pad state */
  dwKeyPad1 = 0;
  dwKeyPad2 = 0;
  dwKeySystem = 0;

  /*-------------------------------------------------------------------*/
  /*  Load Cassette & Create Thread                                    */
  /*-------------------------------------------------------------------*/

  /* Initialize thread state */
  bThread = FALSE;

  int i;

  InitJoypadInput();

  lcd_fb_init();

  // 初始化 zoom 缩放表
  make_zoom_tab();

  /* If a rom name specified, start it */
  if (argc >= 2) {
    start_application(argv[1]);
  }
  printf("exit\n");
  return (0);
}

/*===================================================================*/
/*                                                                   */
/*           emulation_thread() : Thread Hooking Routine             */
/*                                                                   */
/*===================================================================*/

void *emulation_thread(void *args) { InfoNES_Main(); }

/*===================================================================*/
/*                                                                   */
/*     start_application() : Start NES Hardware                      */
/*                                                                   */
/*===================================================================*/
void start_application(char *filename) {
  /* Set a ROM image name */
  strcpy(szRomName, filename);
  /* Load cassette */
  int ret = InfoNES_Load(szRomName);
  if (ret == 0) {
    /* Load SRAM */
    LoadSRAM();

    /* Create Emulation Thread */
    bThread = TRUE;
    // pthread_create( &emulation_tid, NULL, emulation_thread, NULL );

    InfoNES_Main();
  } else {
    printf("load nes file %s faild %d\n", szRomName, ret);
  }
}

/*===================================================================*/
/*                                                                   */
/*           LoadSRAM() : Load a SRAM                                */
/*                                                                   */
/*===================================================================*/
int LoadSRAM() {
  /*
   *  Load a SRAM
   *
   *  Return values
   *     0 : Normally
   *    -1 : SRAM data couldn't be read
   */

  FILE *fp;
  unsigned char pSrcBuf[SRAM_SIZE];
  unsigned char chData;
  unsigned char chTag;
  int nRunLen;
  int nDecoded;
  int nDecLen;
  int nIdx;

  /* It doesn't need to save it */
  nSRAM_SaveFlag = 0;

  /* It is finished if the ROM doesn't have SRAM */
  if (!ROM_SRAM) return (0);

  /* There is necessity to save it */
  nSRAM_SaveFlag = 1;

  /* The preparation of the SRAM file name */
  strcpy(szSaveName, szRomName);
  strcpy(strrchr(szSaveName, '.') + 1, "srm");

  /*-------------------------------------------------------------------*/
  /*  Read a SRAM data                                                 */
  /*-------------------------------------------------------------------*/

  /* Open SRAM file */
  fp = fopen(szSaveName, "rb");
  if (fp == NULL) return (-1);

  /* Read SRAM data */
  fread(pSrcBuf, SRAM_SIZE, 1, fp);

  /* Close SRAM file */
  fclose(fp);

  /*-------------------------------------------------------------------*/
  /*  Extract a SRAM data                                              */
  /*-------------------------------------------------------------------*/

  nDecoded = 0;
  nDecLen = 0;

  chTag = pSrcBuf[nDecoded++];

  while (nDecLen < 8192) {
    chData = pSrcBuf[nDecoded++];

    if (chData == chTag) {
      chData = pSrcBuf[nDecoded++];
      nRunLen = pSrcBuf[nDecoded++];
      for (nIdx = 0; nIdx < nRunLen + 1; ++nIdx) {
        SRAM[nDecLen++] = chData;
      }
    } else {
      SRAM[nDecLen++] = chData;
    }
  }

  /* Successful */
  return (0);
}

/*===================================================================*/
/*                                                                   */
/*           SaveSRAM() : Save a SRAM                                */
/*                                                                   */
/*===================================================================*/
int SaveSRAM() {
  /*
   *  Save a SRAM
   *
   *  Return values
   *     0 : Normally
   *    -1 : SRAM data couldn't be written
   */

  FILE *fp;
  int nUsedTable[256];
  unsigned char chData;
  unsigned char chPrevData;
  unsigned char chTag;
  int nIdx;
  int nEncoded;
  int nEncLen;
  int nRunLen;
  unsigned char pDstBuf[SRAM_SIZE];

  if (!nSRAM_SaveFlag) return (0); /* It doesn't need to save it */

  /*-------------------------------------------------------------------*/
  /*  Compress a SRAM data                                             */
  /*-------------------------------------------------------------------*/

  memset(nUsedTable, 0, sizeof nUsedTable);

  for (nIdx = 0; nIdx < SRAM_SIZE; ++nIdx) {
    ++nUsedTable[SRAM[nIdx++]];
  }
  for (nIdx = 1, chTag = 0; nIdx < 256; ++nIdx) {
    if (nUsedTable[nIdx] < nUsedTable[chTag]) chTag = nIdx;
  }

  nEncoded = 0;
  nEncLen = 0;
  nRunLen = 1;

  pDstBuf[nEncLen++] = chTag;

  chPrevData = SRAM[nEncoded++];

  while (nEncoded < SRAM_SIZE && nEncLen < SRAM_SIZE - 133) {
    chData = SRAM[nEncoded++];

    if (chPrevData == chData && nRunLen < 256)
      ++nRunLen;
    else {
      if (nRunLen >= 4 || chPrevData == chTag) {
        pDstBuf[nEncLen++] = chTag;
        pDstBuf[nEncLen++] = chPrevData;
        pDstBuf[nEncLen++] = nRunLen - 1;
      } else {
        for (nIdx = 0; nIdx < nRunLen; ++nIdx) pDstBuf[nEncLen++] = chPrevData;
      }

      chPrevData = chData;
      nRunLen = 1;
    }
  }
  if (nRunLen >= 4 || chPrevData == chTag) {
    pDstBuf[nEncLen++] = chTag;
    pDstBuf[nEncLen++] = chPrevData;
    pDstBuf[nEncLen++] = nRunLen - 1;
  } else {
    for (nIdx = 0; nIdx < nRunLen; ++nIdx) pDstBuf[nEncLen++] = chPrevData;
  }

  /*-------------------------------------------------------------------*/
  /*  Write a SRAM data                                                */
  /*-------------------------------------------------------------------*/

  /* Open SRAM file */
  fp = fopen(szSaveName, "wb");
  if (fp == NULL) return (-1);

  /* Write SRAM data */
  fwrite(pDstBuf, nEncLen, 1, fp);

  /* Close SRAM file */
  fclose(fp);

  /* Successful */
  return (0);
}

/*===================================================================*/
/*                                                                   */
/*                  InfoNES_Menu() : Menu screen                     */
/*                                                                   */
/*===================================================================*/
int InfoNES_Menu() {
  /*
   *  Menu screen
   *
   *  Return values
   *     0 : Normally
   *    -1 : Exit InfoNES
   */
  // printf("bThread %d\n",bThread);

  /* If terminated */
  if (bThread == FALSE) {
    return (-1);
  }

  /* Nothing to do here */
  return (0);
}

/*===================================================================*/
/*                                                                   */
/*               InfoNES_ReadRom() : Read ROM image file             */
/*                                                                   */
/*===================================================================*/
int InfoNES_ReadRom(const char *pszFileName) {
  /*
   *  Read ROM image file
   *
   *  Parameters
   *    const char *pszFileName          (Read)
   *
   *  Return values
   *     0 : Normally
   *    -1 : Error
   */

  FILE *fp;

  /* Open ROM file */
  fp = fopen(pszFileName, "rb");
  if (fp == NULL) {
    printf("open nes faild\n");
    return (-1);
  }

  /* Read ROM Header */
  fread(&NesHeader, sizeof NesHeader, 1, fp);
  if (memcmp(NesHeader.byID, "NES\x1a", 4) != 0) {
    /* not .nes file */
    fclose(fp);
    printf("read nes faild %s\n", NesHeader.byID);
    return (-1);
  }

  /* Clear SRAM */
  memset(SRAM, 0, SRAM_SIZE);

  /* If trainer presents Read Triner at 0x7000-0x71ff */
  if (NesHeader.byInfo1 & 4) {
    fread(&SRAM[0x1000], 512, 1, fp);
  }

  /* Allocate Memory for ROM Image */
  ROM = (BYTE *)malloc(NesHeader.byRomSize * 0x4000);

  /* Read ROM Image */
  fread(ROM, 0x4000, NesHeader.byRomSize, fp);

  if (NesHeader.byVRomSize > 0) {
    /* Allocate Memory for VROM Image */
    VROM = (BYTE *)malloc(NesHeader.byVRomSize * 0x2000);

    /* Read VROM Image */
    fread(VROM, 0x2000, NesHeader.byVRomSize, fp);
  }

  /* File close */
  fclose(fp);

  /* Successful */
  return (0);
}

/*===================================================================*/
/*                                                                   */
/*           InfoNES_ReleaseRom() : Release a memory for ROM         */
/*                                                                   */
/*===================================================================*/
void InfoNES_ReleaseRom() {
  /*
   *  Release a memory for ROM
   *
   */

  if (ROM) {
    free(ROM);
    ROM = (unsigned char *)NULL;
  }

  if (VROM) {
    free(VROM);
    VROM = (unsigned char *)NULL;
  }
}

/*===================================================================*/
/*                                                                   */
/*             InfoNES_MemoryCopy() : memcpy                         */
/*                                                                   */
/*===================================================================*/
void *InfoNES_MemoryCopy(void *dest, const void *src, int count) {
  /*
   *  memcpy
   *
   *  Parameters
   *    void *dest                       (Write)
   *      Points to the starting address of the copied block's destination
   *
   *    const void *src                  (Read)
   *      Points to the starting address of the block of memory to copy
   *
   *    int count                        (Read)
   *      Specifies the size, in bytes, of the block of memory to copy
   *
   *  Return values
   *    Pointer of destination
   */

  memcpy(dest, src, count);
  return (dest);
}

/*===================================================================*/
/*                                                                   */
/*             InfoNES_MemorySet() : memset                          */
/*                                                                   */
/*===================================================================*/
void *InfoNES_MemorySet(void *dest, int c, int count) {
  /*
   *  memset
   *
   *  Parameters
   *    void *dest                       (Write)
   *      Points to the starting address of the block of memory to fill
   *
   *    int c                            (Read)
   *      Specifies the byte value with which to fill the memory block
   *
   *    int count                        (Read)
   *      Specifies the size, in bytes, of the block of memory to fill
   *
   *  Return values
   *    Pointer of destination
   */

  memset(dest, c, count);
  return (dest);
}

/*===================================================================*/
/*                                                                   */
/*      InfoNES_LoadFrame() :                                        */
/*           Transfer the contents of work frame on the screen       */
/*                                                                   */
/*===================================================================*/


/* Transfer the contents of work frame on the screen */
static inline void InfoNes_LoadLineScale2(uint32_t *fb, WORD* frame, int width){
	do{
		int i = *frame++ % 64;
		*fb++ = *fb++ = RGBPalette[i];
	}while(width--);
}


static inline void InfoNES_LoadFrameScale2(void){
  int offX = (screen->width - NES_DISP_WIDTH*2)/2;
  int offY =(screen->height - NES_DISP_HEIGHT*2)/2; 
  uint32_t* d=(uint32_t *)screen->buffer + offY*screen->width + offX; 
  WORD* s = WorkFrame;
  for(int y=0; y<NES_DISP_HEIGHT; y++){
	InfoNes_LoadLineScale2(d, s, NES_DISP_WIDTH);
	d+=screen->width;
	InfoNes_LoadLineScale2(d, s, NES_DISP_WIDTH);
    d+=screen->width; 
	s+=NES_DISP_WIDTH;
  }
}

static inline void InfoNes_LoadLineScale1(uint32_t *fb, WORD* frame, int width){
	do{
		int i = *frame++ % 64;
		*fb++ = RGBPalette[i];
	}while(width--);
}

static inline  InfoNES_LoadFrameScale1(void){
  int offX = (screen->width - NES_DISP_WIDTH)/2;
  int offY =(screen->height - NES_DISP_HEIGHT)/2; 
  uint32_t* d=(uint32_t *)screen->buffer + offY*screen->width + offX; 
  WORD* s = WorkFrame;
  for(int y=0; y<NES_DISP_HEIGHT; y++){
	InfoNes_LoadLineScale1(d, s, NES_DISP_WIDTH);
	d+=screen->width;
	s+=NES_DISP_WIDTH;
  }
}


// void InfoNES_LoadFrame(){
// 	if(screen->width >= NES_DISP_WIDTH * 2 && screen->height >= NES_DISP_HEIGHT * 2)
// 		InfoNES_LoadFrameScale2();
// 	else
// 		InfoNES_LoadFrameScale1();	
//   screen_flush();
// }

void InfoNES_LoadFrame() {
  int x, y;
  int line_width;
  WORD wColor;

  if (0 < fb_fd) {
    for (y = 0; y < lcd_height; y++) {
      line_width = zoom_y_tab[y] * NES_DISP_WIDTH;
      for (x = 0; x < lcd_width; x++) {
        wColor = WorkFrame[line_width + zoom_x_tab[x]];
        /* 16-bit to 24-bit  RGB565 to RGB888*/
        WORD color = ((wColor & 0x7c00) << 9) | ((wColor & 0x03e0) << 6) |
                     ((wColor & 0x001f) << 3) | (0xff << 24);
        screen_put_pixel(x, y, color);
        // lcd_fb_display_px(wColor, x, y);
      }
    }
    screen_flush();
  }
}

void InfoNES_ReadJoypad() {
  int ret = GetJoypadInput();
  if (ret >= 0) {
    dwKeyPad1 = ret;
    // dwKeyPad2=ret;
    // dwKeySystem=ret;
  }
}

/*===================================================================*/
/*                                                                   */
/*             InfoNES_PadState() : Get a joypad state               */
/*                                                                   */
/*===================================================================*/
void InfoNES_PadState(DWORD *pdwPad1, DWORD *pdwPad2, DWORD *pdwSystem) {
  /*
   *  Get a joypad state
   *
   *  Parameters
   *    DWORD *pdwPad1                   (Write)
   *      Joypad 1 State
   *
   *    DWORD *pdwPad2                   (Write)
   *      Joypad 2 State
   *
   *    DWORD *pdwSystem                 (Write)
   *      Input for InfoNES
   *
   */

  InfoNES_ReadJoypad();

  /* Transfer joypad state */
  *pdwPad1 = dwKeyPad1;
  *pdwPad2 = dwKeyPad2;
  *pdwSystem = bThread == FALSE ? PAD_SYS_QUIT : 0;

  // 取消重置手柄 在 输入函数中自行处理
  //  dwKeyPad1 = 0;
}

/*===================================================================*/
/*                                                                   */
/*        InfoNES_SoundInit() : Sound Emulation Initialize           */
/*                                                                   */
/*===================================================================*/
void InfoNES_SoundInit(void) {}

/*===================================================================*/
/*                                                                   */
/*        InfoNES_SoundOpen() : Sound Open                           */
/*                                                                   */
/*===================================================================*/
int InfoNES_SoundOpen(int samples_per_sync, int sample_rate) {
  // sample_rate 采样率 44100
  // samples_per_sync  735
  printf("InfoNES_SoundOpen: samples_per_sync=%d, sample_rate=%d\n",
         samples_per_sync, sample_rate);
  sound_fd = open(SOUND_DEVICE, 0);
  if (sound_fd < 0) {
    sound_fd = -1;
    return 0;
  }
  return 1;
}

/*===================================================================*/
/*                                                                   */
/*        InfoNES_SoundClose() : Sound Close                         */
/*                                                                   */
/*===================================================================*/
void InfoNES_SoundClose(void) {}

/*===================================================================*/
/*                                                                   */
/*            InfoNES_SoundOutput() : Sound Output 5 Waves           */
/*                                                                   */
/*===================================================================*/
void InfoNES_SoundOutput(int samples, BYTE *wave1, BYTE *wave2, BYTE *wave3,
                         BYTE *wave4, BYTE *wave5) {
  int i;
  int ret;
  unsigned char wav;

  if (sound_fd > 0) {
    for (int i = 0; i < samples; i++) {
      final_wave[i * 2 + 1] = final_wave[i * 2] =
          (wave1[i] + wave2[i] + wave3[i] + wave4[i] + wave5[i]) * 50;
    }

    if (write(sound_fd, final_wave, samples * 4) < samples * 4) {
      printf("wrote less than 1024 bytes\n");
    }
  }
  return;
}

/*===================================================================*/
/*                                                                   */
/*            InfoNES_Wait() : Wait Emulation if required            */
/*                                                                   */
/*===================================================================*/
void InfoNES_Wait() { usleep(5000); }

/*===================================================================*/
/*                                                                   */
/*            InfoNES_MessageBox() : Print System Message            */
/*                                                                   */
/*===================================================================*/
void InfoNES_MessageBox(const char *pszMsg, ...) {
  printf("MessageBox: %s \n", pszMsg);
}

/*
 * End of InfoNES_System_Linux.cpp
 */
