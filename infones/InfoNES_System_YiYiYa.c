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
#include "time.h"  /* clock_gettime / nanosleep：帧节流用 */

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
  screen_put_pixel(x, y, c);
  return 0;
}
screen_info_t *screen = NULL;

/* 输出像素格式：可用命令行 -argb / -rgb555 切换 */
enum { PIX_ARGB8888 = 0, PIX_RGB555 = 1 };
static int g_pix_fmt = -1; /* -1=按屏幕 bpp 自动选择 */

static int lcd_fb_init() {
  screen_init();
  screen = screen_info();
  if (screen == NULL || screen->buffer == NULL) {
    fb_fd = -1;
    return -1;
  }
  /* XWIN 下 screen->fd 常为 -1；用 buffer 是否就绪作可用标志 */
  fb_mem = (unsigned char *)screen->buffer;
  fb_fd = 1;
  /* 未强制指定时：16bpp 屏默认 rgb555，否则 argb */
  if (g_pix_fmt < 0) {
    g_pix_fmt =
        (screen->bpp == 16 || screen->fb.bpp == 16) ? PIX_RGB555 : PIX_ARGB8888;
  }
  if (g_pix_fmt == PIX_RGB555 || screen->fb.bpp == 16) {
    lcd_height = NES_DISP_HEIGHT * 2;
    lcd_width = NES_DISP_WIDTH * 2;
  } else {
    lcd_height = NES_DISP_HEIGHT;
    lcd_width = NES_DISP_WIDTH;
  }
  printf("infones pixel format: %s\n",
         g_pix_fmt == PIX_RGB555 ? "rgb555" : "argb8888");
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
/* 【缓冲必须 ≥ samples*4】samples 默认 735（44100/60）⇒ 立体声 16bit 需要
 * 2940 字节。原来只给 2048 字节，却按 samples*4 写出 ⇒ 越界 892 字节；
 * 且循环只填了前 samples*2 字节 ⇒ 后半截是上一帧的陈旧数据 ⇒ 听感"嘈杂"。 */
BYTE final_wave[8192];
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
    0xff707070, 0xff201888, 0xff0000a8, 0xff400098, 0xff880070, 0xffa80010,
    0xffa00000, 0xff780800, 0xff402800, 0xff004000, 0xff005000, 0xff003810,
    0xff183858, 0xff000000, 0xff000000, 0xff000000, 0xffb8b8b8, 0xff0070e8,
    0xff2038e8, 0xff8000f0, 0xffb800b8, 0xffe00058, 0xffd82800, 0xffc84808,
    0xff887000, 0xff009000, 0xff00a800, 0xff009038, 0xff008088, 0xff000000,
    0xff000000, 0xff000000, 0xfff8f8f8, 0xff38b8f8, 0xff5890f8, 0xff4088f8,
    0xfff078f8, 0xfff870b0, 0xfff87060, 0xfff89838, 0xfff0b838, 0xff80d010,
    0xff48d848, 0xff58f898, 0xff00e8d8, 0xff000000, 0xff000000, 0xff000000,
    0xfff8f8f8, 0xffa8e0f8, 0xffc0d0f8, 0xffd0c8f8, 0xfff8c0f8, 0xfff8c0d8,
    0xfff8b8b0, 0xfff8d8a8, 0xfff8e0a0, 0xffe0f8a0, 0xffa8f0b8, 0xffb0f8c8,
    0xff98f8f0, 0xff000000, 0xff000000, 0xff000000,
};

// DWORD RGBPalette[64] = {
//     0xff707070, 0xff201888, 0xff0000a8, 0xff400098, 0xff880070, 0xffa80010,
//     0xffa00000, 0xff780800, 0xff402800, 0xff004000, 0xff005000, 0xff003810,
//     0xff183858, 0xff000000, 0xff000000, 0xff000000, 0xffb8b8b8, 0xff0070e8,
//     0xff2038e8, 0xff8000f0, 0xffb800b8, 0xffe00058, 0xffd82800, 0xffc84808,
//     0xff887000, 0xff009000, 0xff00a800, 0xff009038, 0xff008088, 0xff000000,
//     0xff000000, 0xff000000, 0xfff8f8f8, 0xff38b8f8, 0xff5890f8, 0xff4088f8,
//     0xfff078f8, 0xfff870b0, 0xfff87060, 0xfff89838, 0xfff0b838, 0xff80d010,
//     0xff48d848, 0xff58f898, 0xff00e8d8, 0xff000000, 0xff000000, 0xff000000,
//     0xfff8f8f8, 0xffa8e0f8, 0xffc0d0f8, 0xffd0c8f8, 0xfff8c0f8, 0xfff8c0d8,
//     0xfff8b8b0, 0xfff8d8a8, 0xfff8e0a0, 0xffe0f8a0, 0xffa8f0b8, 0xffb0f8c8,
//     0xff98f8f0, 0xff000000, 0xff000000, 0xff000000,
// };

// DWORD RGBPalette[64] = {
//     0xff707070,0xff201888,0xff0000a8,0xff400098,0xff880070,0xffa80010,0xffa00000,0xff780800,
//     0xff402800,0xff004000,0xff005000,0xff003810,0xff183858,0xff000000,0xff000000,0xff000000,
//     0xffb8b8b8,0xff0070e8,0xff2038e8,0xff8000f0,0xffb800b8,0xffe00058,0xffd82800,0xffc84808,
//     0xff887000,0xff009000,0xff00a800,0xff009038,0xff008088,0xff000000,0xff000000,0xff000000,
//     0xfff8f8f8,0xff38b8f8,0xff5890f8,0xff4088f8,0xfff078f8,0xff00ffff,0xff00ffff,0xff00ffff,
//     0xfff0b838,0xff80d010,0xff48d848,0xff58f898,0xff00e8d8,0xff000000,0xff000000,0xff000000,
//     0xfff8f8f8,0xffa8e0f8,0xffc0d0f8,0xffd0c8f8,0xfff8c0f8,0xfff8c0d8,0xfff8b8b0,0xfff8d8a8,
//     0xfff8e0a0,0xffe0f8a0,0xffa8f0b8,0xffb0f8c8,0xff98f8f0,0xff000000,0xff000000,0xff000000,
// };
// DWORD RGBPalette[64] = {
//     0xff707070,0xff201800,0xff0000ff,0xff400098,0xff880070,0xffa80010,0xffa00000,0xff780800,
//     0xff402800,0xff004000,0xff005000,0xff003810,0xff183858,0xff000000,0xff000000,0xff000000,
//     0xffb8b8b8,0xff0070e8,0xff2038e8,0xffff0000,0xffb800b8,0xffe00058,0xffd82800,0xffc84808,
//     0xff887000,0xff009000,0xff00a800,0xff009038,0xff008088,0xff000000,0xff000000,0xff000000,
//     0xfff8f8f8,0xff38b8f8,0xff5890f8,0xff4088f8,0xfff078f8,0xfff870b0,0xfff87060,0xfff89838,
//     0xfff0b838,0xff80d010,0xff48d848,0xff58f898,0xff00e8d8,0xff000000,0xff000000,0xff000000,
//     0xfff8f8f8,0xffa8e0f8,0xffc0d0f8,0xffd0c8f8,0xfff8c0f8,0xfff8c0d8,0xfff8b8b0,0xfff8d8a8,
//     0xfff8e0a0,0xffe0f8a0,0xffa8f0b8,0xffb0f8c8,0xff98f8f0,0xff000000,0xff000000,0xff000000,
// };

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
    0x7f94, 0x73f4, 0x57d7, 0x5bf9, 0x4ffe, 0x0000, 0x0000, 0x0000};

/*===================================================================*/
/*                                                                   */
/*                main() : Application main                          */
/*                                                                   */
/*===================================================================*/

/* Application main */
int main(int argc, char **argv) {
  const char *rom = NULL;
  int i;

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

  /* 默认未指定：lcd_fb_init 里按屏幕 bpp 自动选 */
  g_pix_fmt = -1;

  for (i = 1; i < argc; i++) {
    if (argv[i] == NULL) {
      continue;
    }
    if (argv[i][0] == '-') {
      if (strcmp(argv[i], "-rgb555") == 0 || strcmp(argv[i], "-555") == 0) {
        g_pix_fmt = PIX_RGB555;
      } else if (strcmp(argv[i], "-argb") == 0 ||
                 strcmp(argv[i], "-rgb888") == 0 ||
                 strcmp(argv[i], "-888") == 0) {
        g_pix_fmt = PIX_ARGB8888;
      } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
        printf("usage: infones [-rgb555|-argb] <rom.nes>\n");
        return 0;
      } else {
        printf("unknown option: %s\n", argv[i]);
        printf("usage: infones [-rgb555|-argb] <rom.nes>\n");
        return 1;
      }
    } else if (rom == NULL) {
      rom = argv[i];
    }
  }

  InitJoypadInput();

  lcd_fb_init();

  // 初始化 zoom 缩放表
  make_zoom_tab();

  /* If a rom name specified, start it */
  if (rom != NULL) {
    start_application((char *)rom);
  } else {
    printf("usage: infones [-rgb555|-argb] <rom.nes>\n");
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

/* WorkFrame 存的是 NesPalette 的 RGB555（非索引） */
static inline u32 rgb555_to_argb(WORD c) {
  u32 r = (c >> 10) & 0x1f;
  u32 g = (c >> 5) & 0x1f;
  u32 b = c & 0x1f;
  r = (r << 3) | (r >> 2);
  g = (g << 3) | (g >> 2);
  b = (b << 3) | (b >> 2);
  return 0xff000000u | (r << 16) | (g << 8) | b;
}

static inline WORD work_rgb555(WORD c) { return (WORD)(c & 0x7fff); }

/* ---- ARGB8888（32bpp buffer） ---- */
static inline void InfoNes_LoadLineScale2_argb(uint32_t *fb, WORD *frame,
                                               int width) {
  while (width-- > 0) {
    u32 c = rgb555_to_argb(*frame++);
    *fb++ = c;
    *fb++ = c;
  }
}

static inline void InfoNes_LoadLineScale1_argb(uint32_t *fb, WORD *frame,
                                               int width) {
  while (width-- > 0) {
    *fb++ = rgb555_to_argb(*frame++);
  }
}

/* ---- RGB555（16bpp buffer，每像素 2 字节） ---- */
static inline void InfoNes_LoadLineScale2_rgb555(WORD *fb, WORD *frame,
                                                 int width) {
  while (width-- > 0) {
    WORD c = work_rgb555(*frame++);
    *fb++ = c;
    *fb++ = c;
  }
}

static inline void InfoNes_LoadLineScale1_rgb555(WORD *fb, WORD *frame,
                                                 int width) {
  while (width-- > 0) {
    *fb++ = work_rgb555(*frame++);
  }
}

static inline void InfoNES_LoadFrameScale2(void) {
  int offX = (screen->width - NES_DISP_WIDTH * 2) / 2;
  int offY = (screen->height - NES_DISP_HEIGHT * 2) / 2;
  WORD *s = WorkFrame;
  int y;

  if (g_pix_fmt == PIX_RGB555) {
    WORD *d = (WORD *)screen->buffer + offY * screen->width + offX;
    for (y = 0; y < NES_DISP_HEIGHT; y++) {
      InfoNes_LoadLineScale2_rgb555(d, s, NES_DISP_WIDTH);
      d += screen->width;
      InfoNes_LoadLineScale2_rgb555(d, s, NES_DISP_WIDTH);
      d += screen->width;
      s += NES_DISP_WIDTH;
    }
  } else {
    uint32_t *d =
        (uint32_t *)screen->buffer + offY * screen->width + offX;
    for (y = 0; y < NES_DISP_HEIGHT; y++) {
      InfoNes_LoadLineScale2_argb(d, s, NES_DISP_WIDTH);
      d += screen->width;
      InfoNes_LoadLineScale2_argb(d, s, NES_DISP_WIDTH);
      d += screen->width;
      s += NES_DISP_WIDTH;
    }
  }
}

static inline void InfoNES_LoadFrameScale1(void) {
  int offX = (screen->width - NES_DISP_WIDTH) / 2;
  int offY = (screen->height - NES_DISP_HEIGHT) / 2;
  WORD *s = WorkFrame;
  int y;

  if (g_pix_fmt == PIX_RGB555) {
    WORD *d = (WORD *)screen->buffer + offY * screen->width + offX;
    for (y = 0; y < NES_DISP_HEIGHT; y++) {
      InfoNes_LoadLineScale1_rgb555(d, s, NES_DISP_WIDTH);
      d += screen->width;
      s += NES_DISP_WIDTH;
    }
  } else {
    uint32_t *d =
        (uint32_t *)screen->buffer + offY * screen->width + offX;
    for (y = 0; y < NES_DISP_HEIGHT; y++) {
      InfoNes_LoadLineScale1_argb(d, s, NES_DISP_WIDTH);
      d += screen->width;
      s += NES_DISP_WIDTH;
    }
  }
}

void InfoNES_LoadFrame2() {
  if (screen == NULL || screen->buffer == NULL) {
    return;
  }
  if (screen->width >= NES_DISP_WIDTH * 2 &&
      screen->height >= NES_DISP_HEIGHT * 2)
    InfoNES_LoadFrameScale2();
  else
    InfoNES_LoadFrameScale1();
  screen_flush();
}

/*===================================================================*/
/*       InfoNES_FramePace() : 帧节流（绝对时基，60.00Hz）           */
/*===================================================================*/
/* 【为什么必须节流】InfoNES_Wait() 在本移植里是【空实现】⇒ 模拟器完全不受
 * 时间约束，主循环 for(;;) 全速运行 ⇒ 实测 app_fps=81（NES 应为 60fps，
 * 快 1.35 倍），后果有两个：
 *   ① 游戏速度偏快（动作/音乐节拍都偏快）；
 *   ② 音频按 81 帧/秒生产（81×735×4 = 238KB/s），而编解码器只按 44.1kHz
 *      消费（176.4KB/s）⇒ 环形缓冲持续净增 ⇒ 溢出 ⇒ 周期性爆响/断续。
 * 【为什么放这里】本函数在 InfoNES_LoadFrame() 里被调用，而 FrameSkip 恒为 0
 * （InfoNES.c 初始化后不再修改）⇒ LoadFrame 每帧恰好一次；且 APU 的音频写出
 * （InfoNES_SoundOutput）就发生在本函数之前 ⇒ 音频生产间隔天然均匀。
 * 【周期为什么是 16667us】NES 每帧产 735 个采样（samples_per_sync=735），
 * 44100 / 735 = 60.00Hz ⇒ 只有帧率恰好 60.00 时音高才正确（用 60.0988 会让
 * 音高系统性偏高 0.16%，听不出来，但与 735 样本严格配对更简单）。
 * 【绝对排程】累加"目标时刻"而不是睡固定时长：nanosleep 的唤醒延迟会在下一帧
 * 自动被扣回 ⇒ 平均帧率精确；若落后超过一帧（被抢占/阻塞）则重同步，避免
 * 追赶风暴导致连续爆音。 */
static void InfoNES_FramePace(void) {
  static unsigned int next_us;
  unsigned int now;
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  now = (unsigned int)((u32)ts.tv_sec * 1000000u + (u32)ts.tv_nsec / 1000u);

  if (next_us == 0) { /* 首帧：只建立基准 */
    next_us = now + 16667u;
    printf("frame pace: target 60.00Hz (16667us/frame)\n");
    return;
  }
  if ((int)(now - next_us) < 0) {
    return; /* 还没到下一帧的目标时刻：直接返回，不睡 */
  }

  next_us += 16667u; /* 推进到下一帧的目标时刻 */
  {
    int wait = (int)(next_us - now);
    if (wait > 0) {
      ts.tv_sec = 0;
      ts.tv_nsec = (long)wait * 1000L;
      nanosleep(&ts, NULL);
    } else {
      /* 已落后超过一帧（被抢占/阻塞）⇒ 重同步，防止追赶风暴 */
      next_us = now + 16667u;
    }
  }
}

void InfoNES_LoadFrame() {
  /* 帧节流：每帧恰好一次，详见上面 InfoNES_FramePace() 的说明 */
  InfoNES_FramePace();
  /* XWIN/DIRECT：fd 为 -1，旧逻辑 if (fb_fd>0) 会整帧跳过 → 黑屏。 */
  if (fb_fd <= 0 || screen == NULL || screen->buffer == NULL) {
    return;
  }
  InfoNES_LoadFrame2();
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
/* YiYiYa OSS ioctl 常量（duck/modules/sound/sound.h），自行声明避免依赖内核头 */
#ifndef AFMT_S16_LE
#define AFMT_S16_LE 16
#endif
#ifndef SNDCTL_DSP_SETFMT
#define SNDCTL_DSP_SETFMT 11
#endif
#ifndef SNDCTL_DSP_CHANNELS
#define SNDCTL_DSP_CHANNELS 33
#endif
#ifndef SNDCTL_DSP_SPEED
#define SNDCTL_DSP_SPEED 44
#endif

void InfoNES_SoundInit(void) {}

/*===================================================================*/
/*                                                                   */
/*        InfoNES_SoundOpen() : Sound Open                           */
/*                                                                   */
/*===================================================================*/
int InfoNES_SoundOpen(int samples_per_sync, int sample_rate) {
  // sample_rate 采样率 44100
  // samples_per_sync  735
  int fmt = AFMT_S16_LE;
  int ch = 2;
  int hz = sample_rate;
  printf("InfoNES_SoundOpen: samples_per_sync=%d, sample_rate=%d\n",
         samples_per_sync, sample_rate);
  /* O_WRONLY：原来写的是 0（=O_RDONLY），本内核虽不检查，但语义是错的 */
  sound_fd = open(SOUND_DEVICE, O_WRONLY);
  if (sound_fd < 0) {
    sound_fd = -1;
    return 0;
  }
  /* 【必须显式设定格式】原来只 open 不设参数 ⇒ 设备会沿用"上一个应用"留下的
   * 配置（例如先跑 loopwave 播过 22.05k 单声道文件，设备就停在 mono/22050）
   * ⇒ 同样的数据时而能听、时而怪。这里固定成 16bit 小端 / 立体声 / 44100，
   * 与本文件写出的数据布局严格一致。 */
  ioctl(sound_fd, SNDCTL_DSP_SETFMT, &fmt);
  ioctl(sound_fd, SNDCTL_DSP_CHANNELS, &ch);
  ioctl(sound_fd, SNDCTL_DSP_SPEED, &hz);
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
  int mn = (1 << 20), mx = -1;

  if (sound_fd > 0 && samples > 0) {
    int maxs = (int)(sizeof(final_wave) / 4); /* 缓冲能容纳的立体声帧数 */
    if (samples > maxs) {
      samples = maxs;
    }
    /* 【数据映射·实查 InfoNES_pAPU.c 得到】各通道值域：
     *   脉冲1/2 = pulse_xx[] * 音量 ⇒ 0..255（表元值 0x11 × 音量 0..15）
     *   三角     = triangle_50[]   ⇒ 0..255
     *   噪声     = ApuC4Vol/Env    ⇒ 0..15
     *   DMC      = 1 + (dpcm << 1) ⇒ 很小
     * ⇒ 求和 0..~1020，【静音 = 0】（不是 128）。
     * 因此：静音必须输出 0；按 sum*32 映射到 0..32640（≈满幅、不溢出），
     * 等价于原 8bit 写法 (sum/5) 的 160 倍。原代码 (sum*50) 存进 BYTE 会回绕
     * ⇒ 失真"嘈杂"；上一版按"中心 128"偏移 ⇒ 压成 −32k 附近的窄摆幅 ⇒ 几乎无声。
     * 布局：设备为 16bit 小端 / 2 声道 / 44100 ⇒ 一帧 4 字节（L 低高、R 低高）。 */
    /* 【去直流·关键的一步】NES 的合成输出是【单极性】的（0..~1020，静音 = 0）。
     * 直接线性映射到 16bit 会带一个接近半幅的【直流分量】；而真实的 NES 是交流
     * 耦合输出，本 codec 的 DAC→耳放 通路未必能过直流 —— 实测现象正是"数据在流、
     * DMA 在跑、却完全没声音"（直流把后级顶死/被削掉）。原代码的 BYTE 回绕恰好
     * 制造了巨大交流分量，所以它"有声但嘈杂"。
     * 这里加一级一阶高通（去直流），并保证静音仍输出 0：
     *   y[n] = x[n] - x[n-1] + a*y[n-1],  a = 32700/32768 (≈0.998 ⇒ 截止 ~11Hz@44.1k)
     * 状态跨帧保持，保证滤波连续。 */
    {
      static int xp, yp;
      for (i = 0; i < samples; i++) {
        int sum = wave1[i] + wave2[i] + wave3[i] + wave4[i] + wave5[i];
        int x = sum * 32;
        int y = x - xp + ((yp * 32700) >> 15);
        short s;
        if (sum < mn) {
          mn = sum;
        }
        if (sum > mx) {
          mx = sum;
        }
        xp = x;
        if (y > 32767) {
          y = 32767;
        } else if (y < -32768) {
          y = -32768;
        }
        yp = y;
        s = (short)y;
        final_wave[i * 4 + 0] = (BYTE)(s & 0xff);
        final_wave[i * 4 + 1] = (BYTE)((s >> 8) & 0xff);
        final_wave[i * 4 + 2] = final_wave[i * 4 + 0];
        final_wave[i * 4 + 3] = final_wave[i * 4 + 1];
      }
    }

    /* 【诊断·可删】前 3 帧 + 之后每 256 帧打一行：
     *   sum = 五路求和原始范围（游戏中有音乐时应明显起伏，>0）
     * 若长期 sum=0..0 ⇒ APU 没产生波形（问题在模拟侧）；若 sum 明显 >0 而仍无声
     * ⇒ 问题在设备/后级（codec 增益、直流、DMA 数据）。 */
    {
      static int dbg;
      dbg++;
      if (dbg <= 3 || (dbg & 255) == 0) {
        printf("apu out: n=%d sum=%d..%d\n", samples, mn, mx);
      }
    }

    if (write(sound_fd, final_wave, samples * 4) < samples * 4) {
      printf("wrote less than %d bytes\n", samples * 4);
    }
  }
  return;
}

/*===================================================================*/
/*                                                                   */
/*            InfoNES_Wait() : Wait Emulation if required            */
/*                                                                   */
/*===================================================================*/
void InfoNES_Wait() {}

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
