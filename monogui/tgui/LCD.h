// LCD.h: interface for the CLCD class.
//
//////////////////////////////////////////////////////////////////////
#include "TSystemCommon.h"
#if !defined(__LCD_H__)
#define __LCD_H__

// 定义绘制模式
#define	LCD_MODE_NORMAL         0
#define	LCD_MODE_INVERSE        1
#define	LCD_MODE_OR             2
#define	LCD_MODE_AND            3
#define	LCD_MODE_NOR            4

#define FBIOREFRESH    0x4682
#define FBIOSTOPTIMER  0x4680
#define FBIOSTARTTIMER 0x4681


#define  ASC_W	5                       /* width of an ASCI character       */
#define  ASC_H  12                      /* height of an ASCI character      */
#define  HZK_W  11                      /* width of a CHINESE character     */
#define  HZK_H  12                      /* height of a CHINESE character    */

#define  TEXT_LENGTH_MAX 128            /* the max length of a string           */

#define  LENGTH_OF_ASC 3072             /* the number of bytes of the asc font  */
#define  LENGTH_OF_HZK   694752         /* the number of bytes of the hzk font  */
#define  ASC_FILE    "asc12.bin"        /* file name of the asc font            */
#define  HZK_FILE    "gb12song"         /* file name of the hzk font            */

#define  ASC_GAP 1                      /* gap betweent two asc character       */
#define  HZK_GAP 1                      /* gap betweent two hzk character       */

const unsigned char mask_set_bit[8]	= {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
const unsigned char mask_clr_bit[8]	= {0x7F,0xBF,0xDF,0xEF,0xF7,0xFB,0xFD,0xFE};
const unsigned char mask_byte[16]	= {0x00,0x80,0xC0,0xE0,0xF0,0xF8,0xFC,0xFE,0xFF,0x7F,0x3F,0x1F,0x0F,0x07,0x03,0x01};

class CLCD  
{
private:

#if defined(RUN_ENVIRONMENT_LINUX)
	int m_nFB;							/* framebuffer fd*/
#endif // defined(RUN_ENVIRONMENT_LINUX)

	int fb_w;//  = 240;					/* width of FrameBuffer  		*/
	int fb_h;//  = 128;					/* height of FrameBuffer 		*/
	unsigned char* fbmem;				/* FrameBuffer 					*/
	BOOL m_bDeleteFont;
public:
	unsigned char* asc12; // [ LENGTH_OF_ASC + 256 ] = {};	/* ENGLISH FONT	*/

#if defined(CHINESE_SUPPORT)
	unsigned char* hzk12; // [ LENGTH_OF_HZK + 256 ] = {};	/* CHINESE FONT	*/
#endif // defined(CHINESE_SUPPORT)

	CLCD();
	virtual ~CLCD();

	void LCDShowBrand(unsigned char* pmem, int buf_w, int buf_h, int x, int y);

#if defined(RUN_ENVIRONMENT_LINUX)
	// Init LCD in FrameBuffer mode
	BOOL LCDInit();
#endif // defined(RUN_ENVIRONMENT_LINUX)
#if defined(RUN_ENVIRONMENT_WIN32)
	HWND m_hWnd;
	BOOL Win32Init( HWND hWnd, int w, int h );
#endif // defined(RUN_ENVIRONMENT_WIN32)

	// Init LCD Without FrameBufferSupport
	BOOL LCDInitVirtualLCD( void(* fnShow)() );
	BOOL BufferInit( unsigned char* pASC, unsigned char* pHZK );
	void LCDShow( int x, int y, int w, int h );

	unsigned char* LCDGetFB(int* pIW,int* pIH);
	void LCDSetPixel(unsigned char* pmem, int buf_w, int buf_h, int x, int y, int color);
	int  LCDGetPixel(unsigned char* pmem, int buf_w, int buf_h, int x, int y);
	void LCDVLine(unsigned char* pmem, int buf_w, int buf_h, int x, int y, int l, int color);
	void LCDHLine(unsigned char* pmem, int buf_w, int buf_h, int x, int y, int l, int color);
	void LCDFillRect(unsigned char* pmem, int buf_w, int buf_h, int x, int y, int w, int h, int color);
	void LCDDrawImage(unsigned char* pmem, int buf_w, int buf_h, int x, int y, int w, int h, int mode, unsigned char* buffer, int sw, int sh, int sx, int sy);
	void LCDTextOut(unsigned char* pmem, int buf_w, int buf_h, int x, int y, int mode, unsigned char* buffer, int l);
	void LCDTextOut_1(unsigned char* pmem, int buf_w, int buf_h, int x, int y, int mode, unsigned char* buffer, int l);  // 用于英文汉字对齐模式
	void LCDCopy(unsigned char* obuffer, unsigned char* sbuffer, int buf_w, int buf_h, int mode);
	void LCDBitBlt(unsigned char* obuffer, int buf_w, int buf_h, int x, int y, int w, int h, int mode, unsigned char* sbuffer, int sw, int sh, int sx, int sy);
	void LCDLine(unsigned char* pmem, int buf_w, int buf_h, int x1, int y1, int x2, int y2, int color);
};

#endif // !defined(__LCD_H__)
