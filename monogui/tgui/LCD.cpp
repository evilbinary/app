// LCD.cpp: implementation of the CLCD class.
//
//////////////////////////////////////////////////////////////////////
#include "tgui.h"

#if defined(RUN_ENVIRONMENT_LINUX)
// #include <linux/fb.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#endif // defined(RUN_ENVIRONMENT_LINUX)
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static void(* m_fnShow)();

CLCD::CLCD()
{

#if defined(RUN_ENVIRONMENT_LINUX)
	m_nFB = 0;
#endif // defined(RUN_ENVIRONMENT_LINUX)

	asc12 = NULL;

#if defined(CHINESE_SUPPORT)
	hzk12 = NULL;
#endif // defined(CHINESE_SUPPORT)

	fbmem = NULL;
	m_bDeleteFont = FALSE;
}

CLCD::~CLCD()
{
#if defined(RUN_ENVIRONMENT_LINUX)
	if( m_nFB > 0 )
		close( m_nFB );
	//ioctl( m_nFB, FBIOSTARTTIMER, 0 );
#endif // defined(RUN_ENVIRONMENT_LINUX)

	if( !m_bDeleteFont )
	{
		delete [] asc12;

#if defined(CHINESE_SUPPORT)
		delete [] hzk12;
#endif // defined(CHINESE_SUPPORT)

	}
	if( fbmem != NULL)
		delete [] fbmem;
}

/* display the trademark */
void CLCD::LCDShowBrand(unsigned char* pmem, int buf_w, int buf_h, int x, int y)
{

unsigned char pHisense[513] = 
{
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7f,0x80,0xff,0x07,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x40,0x80,0xff,0x07,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x40,0x80,0xff,0x07,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x40,0x80,0xff,0x07,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x40,0x80,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x40,0x80,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x40,0x80,0xff,0x07,0xf0,0x0f,0xf8,0x00,0x1f,0xe0,0x07,0xf1,0xf0,0x01,0xff,0x00,0x03,0xfc,0x00,
0x7f,0x80,0xff,0x07,0xf0,0x3f,0xfe,0x00,0x7f,0xf8,0x07,0xf7,0xf8,0x07,0xff,0xc0,0x0f,0xff,0x00,
0x00,0x00,0xff,0x07,0xf0,0x7f,0xff,0x00,0xff,0xfc,0x07,0xff,0xfc,0x0f,0xff,0xe0,0x1f,0xff,0x80,
0x7f,0xff,0xff,0x07,0xf0,0xff,0xff,0x81,0xff,0xfe,0x07,0xff,0xfc,0x1f,0xff,0xf0,0x3f,0xff,0xc0,
0x7f,0xff,0xff,0x07,0xf0,0xfe,0x3f,0x83,0xff,0xff,0x07,0xff,0xfe,0x1f,0xc7,0xf0,0x7f,0xff,0xe0,
0x7f,0xff,0xff,0x07,0xf0,0xfe,0x00,0x03,0xf8,0x7f,0x07,0xf9,0xfe,0x1f,0xc0,0x00,0x7f,0x0f,0xe0,
0x7f,0xff,0xff,0x07,0xf0,0xfe,0x00,0x07,0xf0,0x3f,0x87,0xf0,0xfe,0x1f,0xc0,0x00,0xfe,0x07,0xf0,
0x7f,0xff,0xff,0x07,0xf0,0xff,0xf8,0x07,0xff,0xff,0x87,0xf0,0xfe,0x1f,0xff,0x00,0xff,0xff,0xf0,
0x7f,0xff,0xff,0x07,0xf0,0x7f,0xff,0x07,0xff,0xff,0x87,0xf0,0xfe,0x0f,0xff,0xe0,0xff,0xff,0xf0,
0x7f,0x80,0xff,0x07,0xf0,0x3f,0xff,0x87,0xff,0xff,0x87,0xf0,0xfe,0x07,0xff,0xf0,0xff,0xff,0xf0,
0x7f,0x80,0xff,0x07,0xf0,0x1f,0xff,0x87,0xff,0xff,0x87,0xf0,0xfe,0x03,0xff,0xf0,0xff,0xff,0xf0,
0x7f,0x80,0xff,0x07,0xf0,0x00,0xff,0xc7,0xf0,0x00,0x07,0xf0,0xfe,0x00,0x1f,0xf8,0xfe,0x00,0x00,
0x7f,0x80,0xff,0x07,0xf0,0x00,0x1f,0xc7,0xf0,0x00,0x07,0xf0,0xfe,0x00,0x03,0xf8,0xfe,0x00,0x00,
0x7f,0x80,0xff,0x07,0xf1,0xfc,0x1f,0xc3,0xf8,0x7f,0x87,0xf0,0xfe,0x3f,0x83,0xf8,0x7f,0x0f,0xf0,
0x7f,0x80,0xff,0x07,0xf1,0xfe,0x3f,0xc3,0xff,0xff,0x07,0xf0,0xfe,0x3f,0xc7,0xf8,0x7f,0xff,0xe0,
0x7f,0x80,0xff,0x07,0xf0,0xff,0xff,0x81,0xff,0xff,0x07,0xf0,0xfe,0x1f,0xff,0xf0,0x3f,0xff,0xe0,
0x7f,0x80,0xff,0x07,0xf0,0xff,0xff,0x80,0xff,0xfe,0x07,0xf0,0xfe,0x1f,0xff,0xf0,0x1f,0xff,0xc0,
0x7f,0x80,0xff,0x07,0xf0,0x7f,0xfe,0x00,0x7f,0xf8,0x07,0xf0,0xfe,0x0f,0xff,0xc0,0x0f,0xff,0x00,
0x7f,0x80,0xff,0x07,0xf0,0x0f,0xf8,0x00,0x1f,0xe0,0x07,0xf0,0xfe,0x01,0xff,0x00,0x03,0xfc,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 
};

	LCDBitBlt (pmem,buf_w,buf_h,x,y,149,27,0,pHisense,152,27,0,0);

#if defined(CHINESE_SUPPORT)
// ������ܵ��ַ����ǣ����������������豸���޹�˾��
/* original string : (the string must be encrypted)
    unsigned char company_name[25] = 
		{0xba,0xa3, 0xd0,0xc5, 0xd6,0xc7, 0xc4,0xdc,
		 0xc9,0xcc, 0xd3,0xc3, 0xc9,0xe8, 0xb1,0xb8,
		 0xd3,0xd0, 0xcf,0xde, 0xb9,0xab, 0xcb,0xbe,
		 0x0};
    unsigned char company_name[25] = 
		{0xbb,0xa2, 0xd2,0xc3, 0xd9,0xc4, 0xc8,0xd8,
		 0xce,0xc7, 0xd9,0xbd, 0xd0,0xe1, 0xb9,0xb0,
		 0xdc,0xc7, 0xd9,0xd4, 0xc4,0xa0, 0xd7,0xb2,
		 0x0};
*/

// ������ܵ��ַ����ǣ�����ӭʹ�ú���˰���տ����
//  unsigned char company_name[23] = 
//		{0xbb,0xb6, 0xd3,0xad, 0xca,0xb9, 0xd3,0xc3,
//		 0xba,0xa3, 0xd0,0xc5, 0xcb,0xb0, 0xbf,0xd8,
//		 0xca,0xd6, 0xbf,0xee, 0xbb,0xfa, 0x00};
    unsigned char company_name[23] = 
		{0xbc,0xb5, 0xd5,0xab, 0xcd,0xb6, 0xd7,0xbf,
		 0xbf,0x9e, 0xd6,0xbf, 0xd2,0xa9, 0xc7,0xd0,
		 0xd3,0xcc, 0xc9,0xe4, 0xc6,0xef, 0x00};

    unsigned char i;
    unsigned char k1 = 1;
    unsigned char k2 = 1;
    for (i=0; i<22; i++)
    {
		if ((i % 2) == 0)
		{
			*(company_name + i) -= k1;
			k1 ++;
		}
		else
		{
			*(company_name + i) += k2;
			k2 ++;
		}
    }

	x += 1;
	for (i=0; i<11; i++)
	{
		LCDTextOut( pmem, buf_w, buf_h, x, y+28, 0, (company_name+i*2), 2 );
		x += 14;
	}
#else
	unsigned char psString[] = "   ----  HK-H60E  ----   ";
	LCDTextOut( pmem, buf_w, buf_h, x, y+28, 0, psString, 24 );
#endif // defined(CHINESE_SUPPORT)
}

#if defined(RUN_ENVIRONMENT_LINUX)
/* initilize the LCM, get the pointer of FrameBuffer */
BOOL CLCD::LCDInit (void)
{
	// struct	fb_var_screeninfo fbvar;
	// struct	fb_fix_screeninfo fbfix;
	int	fd;

	// if(( m_nFB = open("/dev/fb0",O_WRONLY)) == -1)
	// {
	// 	DebugPrintf("ERROR: open fb0 error\n");
	// 	exit(-1);
	// }

	// if(ioctl(m_nFB,FBIOGET_VSCREENINFO,&fbvar))
	// {
	// 	DebugPrintf("ERROR: can't get fb var info\n");
	// 	exit(-1);
	// }

	// if(ioctl(m_nFB,FBIOGET_FSCREENINFO,&fbfix))
	// {
	// 	DebugPrintf("ERROR: can't get fb fix info\n");
	// 	exit(-1);
	// }

	// fb_w = fbvar.xres;
	// fb_h = fbvar.yres;

	// fd = open("/dev/mem",O_RDWR);
	// if(fd<=0)
	// {
	// 	DebugPrintf("ERROR: can't open /dev/mem \n");
	// 	exit(0);
	// }
	// fbmem = (unsigned char *)mmap(0,fbfix.smem_len,PROT_READ|PROT_WRITE,MAP_SHARED,fd,fbfix.smem_start);
	// close(fd);
	
	screen_info_t* info=screen_info();

	fbmem= (unsigned char*)info->buffer;

	if( m_bDeleteFont )
	{
		if( asc12 != NULL )
			delete [] asc12;

#if defined(CHINERS_SUPPORT)
		if( hzk12 != NULL )
			delete [] asc12;
#endif // defined(CHINESE_SUPPORT)\

	}
	asc12 = new unsigned char[ LENGTH_OF_ASC + 256 ];

#if defined(CHINESE_SUPPORT)
	hzk12 = new unsigned char[ LENGTH_OF_HZK + 256 ];
#endif // defined(CHINESE_SUPPORT)

	fd = open(ASC_FILE,O_RDONLY);
	if(fd == -1)
	{
		DebugPrintf("ERROR: English font open error\n");
		delete [] asc12;
		asc12 = NULL;

#if defined(CHINESE_SUPPORT)
		delete [] hzk12;
		hzk12 = NULL;
#endif // defined(CHINESE_SUPPORT)

		exit(0);
	}
	memset(asc12, 0x0, LENGTH_OF_ASC + 256);
	int iRlt = read(fd,asc12,LENGTH_OF_ASC);
	if(iRlt != LENGTH_OF_ASC)
	{
		DebugPrintf("ERROR: English font read error\n");
	}
	close(fd);

#if defined(CHINESE_SUPPORT)
	fd = open(HZK_FILE,O_RDONLY);
	
	if(fd == -1)
	{
		DebugPrintf("ERROR: Chinese font  open error\n");
		delete [] asc12;
		asc12 = NULL;

#if defined(CHINESE_SUPPORT)
		delete [] hzk12;
		hzk12 = NULL;
#endif // defined(CHINESE_SUPPORT)

		exit(0);
	}
	memset(hzk12,0x0,LENGTH_OF_HZK + 256);
	iRlt = read(fd,hzk12,LENGTH_OF_HZK); 
	if(iRlt != LENGTH_OF_HZK)
	{
		DebugPrintf("ERROR: Chinese font read error\n");

	}
	close(fd);
#endif // defined(CHINESE_SUPPORT)

	// ������������ɾ���ֿ�
	m_bDeleteFont = TRUE;

	ioctl( m_nFB, FBIOSTOPTIMER, 0 );
}
#endif // defined(RUN_ENVIRONMENT_LINUX)

#if defined(RUN_ENVIRONMENT_WIN32)
// ��������������Win32���滷��ʹ��
BOOL CLCD::Win32Init (HWND hWnd, int w, int h)
{
	if (w < 1)
		return FALSE;
	if (h < 1)
		return FALSE;

	m_hWnd = hWnd;
	fb_w = w;
	fb_h = h;
	int iFrameBufferSize = (w * h) >> 3;
	
	fbmem = new unsigned char[iFrameBufferSize + 2];
	memset( fbmem, 0x0, iFrameBufferSize + 2 );

	if( m_bDeleteFont )
	{
		if( asc12 != NULL )
			delete [] asc12;

#if defined(CHINESE_SUPPORT)
		if( hzk12 != NULL )
			delete [] asc12;
#endif // defined(CHINESE_SUPPORT)

	}
	asc12 = new unsigned char[ LENGTH_OF_ASC + 256 ];

#if defined(CHINESE_SUPPORT)
	hzk12 = new unsigned char[ LENGTH_OF_HZK + 256 ];
#endif // defined(CHINESE_SUPPORT)

	// ���ֿ�
	HANDLE hFile;
	unsigned long ulActualSize;
	BOOL bReadSt;

#if defined(CHINESE_SUPPORT)
	hFile = CreateFile(HZK_FILE,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,NULL);
	if(hFile==INVALID_HANDLE_VALUE)
		goto release;

	memset (hzk12, 0x0, LENGTH_OF_HZK + 256);
	bReadSt = ReadFile (hFile, hzk12, LENGTH_OF_HZK, &ulActualSize, NULL);
	CloseHandle (hFile);
	
	if (!bReadSt)
		goto release;
	if (ulActualSize != LENGTH_OF_HZK)
		goto release;
#endif // defined(CHINESE_SUPPORT)

	hFile = CreateFile(ASC_FILE,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,NULL);
	if(hFile==INVALID_HANDLE_VALUE)
		goto release;

	memset (asc12, 0x0, LENGTH_OF_ASC + 256);
	bReadSt = ReadFile (hFile, asc12, LENGTH_OF_ASC, &ulActualSize, NULL);
	CloseHandle (hFile);
	
	if (!bReadSt)
		goto release;
	if (ulActualSize != LENGTH_OF_ASC)
		goto release;

	// ������������ɾ���ֿ�
	m_bDeleteFont = TRUE;
	return TRUE;

release:
	delete [] asc12;
	asc12 = NULL;

#if defined(CHINESE_SUPPORT)
	delete [] hzk12;
	hzk12 = NULL;
#endif // defined(CHINESE_SUPPORT)

	return FALSE;
}
#endif // defined(RUN_ENVIRONMENT_WIN32)

void CLCD::LCDShow( int x, int y, int w, int h )
{
#if defined(RUN_ENVIRONMENT_LINUX)
  	(* m_fnShow)();
#endif // defined(RUN_ENVIRONMENT_LINUX)

#if defined(RUN_ENVIRONMENT_WIN32)
	if (m_hWnd == NULL)
		return;

	unsigned char* pTemp = new unsigned char[(SCREEN_W + 7)/8*SCREEN_H];
	// ����Ļ��תΪ��ɫ
	for( int i=0; i<((SCREEN_W + 7)/8*SCREEN_H); i++ )
		pTemp[i] = ~fbmem[i];

	HBITMAP hBitmap = ::CreateBitmap( fb_w, fb_h, 1, 1, pTemp );

	HDC hdc = ::GetDC( m_hWnd );
	HDC hMemDC = ::CreateCompatibleDC( hdc );
	::DeleteObject( ::SelectObject( hMemDC, hBitmap ) );

	if( SCREEN_MODE == 2 )
		StretchBlt( hdc,0,0,fb_w*2,fb_h*2,hMemDC,0,0,fb_w,fb_h,SRCCOPY );
	else
		BitBlt( hdc, 0, 0, fb_w, fb_h, hMemDC, 0, 0, SRCCOPY );

	::DeleteObject( hBitmap );
	::DeleteObject( hMemDC );
	::ReleaseDC( m_hWnd, hdc );

	delete [] pTemp;
#endif // defined(RUN_ENVIRONMENT_WIN32)
}

// Init LCD Without FrameBufferSupport
BOOL CLCD::LCDInitVirtualLCD( void(* fnShow)() )
{
	m_fnShow = fnShow;

	int fd;
	fb_w = SCREEN_W;
	fb_h = SCREEN_H;
	fbmem = new unsigned char[(SCREEN_W + 7)/8*SCREEN_H];

	if( m_bDeleteFont )
	{
		if( asc12 != NULL )
			delete [] asc12;

#if defined(CHINERS_SUPPORT)
		if( hzk12 != NULL )
			delete [] asc12;
#endif // defined(CHINESE_SUPPORT)\

	}
	asc12 = new unsigned char[ LENGTH_OF_ASC + 256 ];

#if defined(CHINESE_SUPPORT)
	hzk12 = new unsigned char[ LENGTH_OF_HZK + 256 ];
#endif // defined(CHINESE_SUPPORT)

	fd = open(ASC_FILE,O_RDONLY);
	if(fd == -1)
	{
		DebugPrintf("ERROR: English font open error\n");
		delete [] asc12;
		asc12 = NULL;

#if defined(CHINESE_SUPPORT)
		delete [] hzk12;
		hzk12 = NULL;
#endif // defined(CHINESE_SUPPORT)

		exit(0);
	}
	memset(asc12, 0x0, LENGTH_OF_ASC + 256);
	int iRlt = read(fd,asc12,LENGTH_OF_ASC);
	if(iRlt != LENGTH_OF_ASC)
	{
		DebugPrintf("ERROR: English font read error\n");
	}
	close(fd);

#if defined(CHINESE_SUPPORT)
	fd = open(HZK_FILE,O_RDONLY);
	
	if(fd == -1)
	{
		DebugPrintf("ERROR: Chinese font  open error\n");
		delete [] asc12;
		asc12 = NULL;

#if defined(CHINESE_SUPPORT)
		delete [] hzk12;
		hzk12 = NULL;
#endif // defined(CHINESE_SUPPORT)

		exit(0);
	}
	memset(hzk12,0x0,LENGTH_OF_HZK + 256);
	iRlt = read(fd,hzk12,LENGTH_OF_HZK); 
	if(iRlt != LENGTH_OF_HZK)
	{
		DebugPrintf("ERROR: Chinese font read error\n");

	}
	close(fd);
#endif // defined(CHINESE_SUPPORT)

	// ������������ɾ���ֿ�
	m_bDeleteFont = TRUE;
}

/* initilize the LCM, get the pointer of FrameBuffer */
BOOL CLCD::BufferInit( unsigned char* pASC, unsigned char* pHZK )
{
	fb_w = SCREEN_W;
	fb_h = SCREEN_H;
	fbmem = new unsigned char[(SCREEN_W + 7)/8*SCREEN_H];

	if( m_bDeleteFont )
	{
		if( asc12 != NULL )
			delete [] asc12;

#if defined(CHINESE_SUPPORT)
		if( hzk12 != NULL )
			delete [] asc12;
#endif // defined(CHINESE_SUPPORT)

	}

	asc12 = pASC;

#if defined(CHINESE_SUPPORT)
	hzk12 = pHZK;
#endif // defined(CHINESE_SUPPORT)

	// ��������������Ҫɾ���ֿ�
	m_bDeleteFont = FALSE;
	return TRUE;
}

/* get the pointer of FrameBuffer, and the width, the height */
unsigned char* CLCD::LCDGetFB (int* pIW, int* pIH)
{
	*pIW = fb_w;
	*pIH = fb_h;
	return fbmem;
}

/* set a pixel color of the identified position */
void CLCD::LCDSetPixel (unsigned char* pmem, int buf_w, int buf_h, int x, int y, int color)
{
    unsigned int location = ((y * buf_w) >> 3) + (x >> 3);
    
    if (x < 0 || x >= buf_w) return;
    if (y < 0 || y >= buf_h) return;
    
    unsigned char abyte = *(pmem + location);
    if (color)
		abyte |=  (1 << (7 - (x & 7)));
    else
		abyte &= ~(1 << (7 - (x & 7)));
    *(pmem + location) = abyte;
}

/* get the color of the special pixel */
int CLCD::LCDGetPixel (unsigned char* pmem, int buf_w, int buf_h, int x, int y)
{
    unsigned int location = ((y * buf_w) >> 3) + (x >> 3);

    if (x < 0 || x >= buf_w) return 2;
    if (y < 0 || y >= buf_h) return 2;

    unsigned char abyte = *(pmem + location);
    abyte &= (1 << (7 - (x & 7)));
    if (abyte == 0)
		return 0;
    else
		return 1;
}

/* draw a vertical line on the buffer */
void CLCD::LCDVLine (unsigned char* pmem, int buf_w, int buf_h, int x, int y, int l, int color)
{
	if( x >= buf_w ) return;
	if( y >= buf_h ) return;
	if( l <= 0 )     return;
	if( x <  0 )     return;

	// ������Ļ��С�Ի��Ʋ������м���
	if( y < 0 )
	{
		l = ( l + y );
		y = 0;
	}
    if( (l + y) > buf_h )
        l = buf_h - y;
	// 

    unsigned char abyte;
    unsigned char mask;
    int i;
    unsigned int location = ((y * buf_w) >> 3) + (x >> 3);

    int buf_w_byte = buf_w >> 3;
    
    if (color)
    {
		mask = mask_set_bit [x & 7];
        for (i=0; i<l; i++)
        {
			abyte = *(pmem + location);
			abyte |= mask;
			*(pmem + location) = abyte;
			location += buf_w_byte;
		}
    }
    else
    {
		mask = mask_clr_bit [x & 7];
		for (i=0; i<l; i++)
		{
			abyte = *(pmem + location);
			abyte &= mask;
			*(pmem + location) = abyte;
			location += buf_w_byte;
		}
    }
}

/* draw a horizontal line on the buffer */
void CLCD::LCDHLine (unsigned char* pmem, int buf_w, int buf_h, int x, int y, int l, int color)
{
	if( x >= buf_w ) return;
	if( y >= buf_h ) return;
	if( l <= 0 )     return;
	if( y <  0 )     return;

	// ������Ļ��С�Ի��Ʋ������м���
	if( x < 0 )
	{
		l = ( l + x );
		x = 0;
	}
    if( (l + x) > buf_w )
        l = buf_w - x;
	//

    unsigned char abyte;
    unsigned char mask;
    int i;

    int start = ((y * buf_w) >> 3) + (x >> 3);
    int end = ((y * buf_w) >> 3) + ((x + l) >> 3);
    int offset_start = x & 7;
    int offset_end = (x + l) & 7;
    
    if (color)
    {
		if (start == end)	/* the line is in one byte */
		{
			abyte = *(pmem + start);
			mask  = mask_byte [offset_start + 8];
			mask &= mask_byte [offset_end];
			abyte |= mask;
			*(pmem + start) = abyte;
		}
		else
		{
			abyte = *(pmem + start);		/* draw the first byte */
			mask  = mask_byte [offset_start + 8];
			abyte |= mask;
			*(pmem + start) = abyte;
			for (i=1; i<(end-start); i++)	/* draw the middle bytes */
			{
				*(pmem + start + i) = 0xff;
			}
			if (offset_end != 0)			/* draw the last byte */
			{
				abyte = *(pmem + end);
				mask = mask_byte [offset_end];
				abyte |= mask;
				*(pmem + end) = abyte;
			}
		}
    }
    else
    {
		if (start == end)	/* the line is in one byte */
		{
			abyte = *(pmem + start);
			mask  = mask_byte [offset_start];
			mask |= mask_byte [offset_end + 8];
			abyte &= mask;
			*(pmem + start) = abyte;
		}
		else
		{
			abyte = *(pmem + start);		/* draw the first byte */
			mask = mask_byte [offset_start];
			abyte &= mask;
			*(pmem + start) = abyte;
			for (i=1; i<(end-start); i++)	/* draw the middle bytes */
			{
				*(pmem + start + i) = 0x0;
			}
			if (offset_end != 0)			/* draw the last byte */
			{
	    		abyte = *(pmem + end);
	    		mask  = mask_byte [offset_end + 8];
	    		abyte &= mask;
	    		*(pmem + end) = abyte;
			}
		}
    }
}

/* fill an rect area with identified color */
void CLCD::LCDFillRect (unsigned char* pmem, int buf_w, int buf_h, int x, int y, int w, int h, int color)
{
	if( x >= buf_w ) return;
	if( y >= buf_h ) return;
	if( w <= 0 )     return;
	if( h <= 0 )     return;

	// ������Ļ��С�Ի��Ʋ������м���
	if( x < 0 )
	{
		w = ( w + x );
		x = 0;
	}
    if( (w + x) > buf_w )
        w = buf_w - x;

	if( y < 0 )
	{
		h = ( h + y );
		y = 0;
	}
	if( (h + y) > buf_h )
		h = buf_h - y;
	//

    int i;
    if (color !=2)
    {
    	for (i=0; i<h; i++)
	        LCDHLine (pmem, buf_w, buf_h, x, y+i, w, color);
    }
    else	// black and white
    {
		int j;
		int cur_x = 0;
		int cur_y = 0;
		int cur_color = 0;
		
		for (i=0; i<h; i++)
		{
			for (j=0; j<w; j++)
			{
				cur_x = x + j;
				cur_y = y + i;
				if ((cur_x + cur_y) % 2 == 1)
				{
					cur_color = 1;
				}
				else
				{
					cur_color = 0;
				}
    			LCDSetPixel (pmem, buf_w, buf_h, cur_x, cur_y, cur_color);
			}
		}
    }
}

/* draw a rectangle image with identified mode */
/* LCD_MODE_NORMAL		0	*/
/* LCD_MODE_INVERSE		1	*/
/* LCD_MODE_OR			2	*/
/* LCD_MODE_AND			3	*/
/* LCD_MODE_NOR			4	*/
/* image matrix: 0: clear a bit; 1: set a bit; 2: keep background color; 3: inverse background color */
void CLCD::LCDDrawImage (unsigned char* pmem, int buf_w, int buf_h, int x, int y, int w, int h, int mode, unsigned char* buffer, int sw, int sh, int sx, int sy)
{
    if (w <= 0) return;
    if (h <= 0) return;
    if (x > buf_w) return;
    if (y > buf_h) return;
    
    int width  = w;
    if ((x+width) > buf_w)
		width = buf_w - x;
    if ((sx+width) > sw)
		width = sw - sx;
    int height = h;
    if ((y+height) > buf_h)
		height = buf_h - y;
    if ((sy+height) > sh)
		height = sh - sy;
    
    int i;
    int j;
    for (i=0; i<width; i++)
    {
        for (j=0; j<height; j++)
        {
			int c = buffer[(sy + j) * sw + sx + i];
			if (c == 2)
				continue;

			if (c == 3)
			{
				c = LCDGetPixel (pmem, buf_w, buf_h, x+i, y+j);
				c = (c==0 ? 1 : 0);
				LCDSetPixel (pmem, buf_w, buf_h, x+i, y+j, c);
				continue;
			}
			
			if (mode == 1)
			{
				c = (c==0 ? 1 : 0);
			}
			else if (mode == 2)
			{
				if (c == 0)
					continue;
			}
			else if (mode == 3)
			{
				if (c == 1)
					continue;
			}
			else if (mode == 4)
			{
				int t = LCDGetPixel (pmem, buf_w, buf_h, x+i, y+j);
				c ^= t;
			}

			LCDSetPixel (pmem, buf_w, buf_h, x+i, y+j, c);
		}
    }
}

/* draw an identified string */
void CLCD::LCDTextOut (unsigned char* pmem, int buf_w, int buf_h, int x, int y, int mode, unsigned char* buffer, int l)
{
    if (l == 0) return;
    if (x >= buf_w) return;
    if (y >= buf_h) return;
    
    int length = l;
    if (length == -1)
	length = TEXT_LENGTH_MAX;
    
    int current_x = x;
    int current_y = y;
    int current = 0;
    unsigned char cHByte;

#if defined(CHINESE_SUPPORT)
    unsigned char cLByte;
#endif // defined(CHINESE_SUPPORT)

    unsigned char character_matrix [64];	/* store the matrix of current character */
    memset (character_matrix, 0x0, 64);
        
    while (current < length)
    {
    	cHByte = *(buffer + current);
		current ++;
		if (cHByte == 0) return;		/* found the END character */

		if (cHByte < 128)			/* an ENGLISH character */
		{
			unsigned int offset = cHByte * ASC_H;
			if (offset > LENGTH_OF_ASC) continue;
			
			memcpy (character_matrix, (asc12 + offset), ASC_H);
			
			LCDBitBlt (pmem, buf_w, buf_h, current_x, current_y, ASC_W+ASC_GAP, ASC_H, mode, character_matrix, 8, ASC_H, 0, 0);
			current_x += (ASC_W + ASC_GAP);
		}
		else					/* a CHINESE character */
		{

#if defined(CHINESE_SUPPORT)
			cLByte = *(buffer + current);
			current ++;
			if (cLByte == 0) return;		/* found the END character */
			
			unsigned int offset = GetHzIndex( cHByte, cLByte, (HZK_H * 2) );
			if (offset < 0) continue;
			if (offset > LENGTH_OF_HZK) continue;

			memcpy (character_matrix, (hzk12 + offset), (HZK_H * 2));

			LCDBitBlt (pmem, buf_w, buf_h, current_x, current_y, HZK_W+HZK_GAP, HZK_H, mode, character_matrix, 16, HZK_H, 0, 0);
			current_x += (HZK_W + HZK_GAP);
#else
			return;
#endif // defined(CHINESE_SUPPORT)

		}
    }
}

// ����Ӣ�ĺ��ֶ���ģʽ
void CLCD::LCDTextOut_1(unsigned char* pmem, int buf_w, int buf_h, int x, int y, int mode, unsigned char* buffer, int l)
{
    if (l == 0) return;
    if (x >= buf_w) return;
    if (y >= buf_h) return;
    
    int length = l;
    if (length == -1)
	length = TEXT_LENGTH_MAX;
    
    int current_x = x;
    int current_y = y;
    int current = 0;
    unsigned char cHByte;

#if defined(CHINESE_SUPPORT)
    unsigned char cLByte;
#endif // defined(CHINESE_SUPPORT)

    unsigned char character_matrix [64];	/* store the matrix of current character */
    memset (character_matrix, 0x0, 64);
        
    while (current < length)
    {
    	cHByte = *(buffer + current);
		current ++;
		if (cHByte == 0) return;		/* found the END character */

		if (cHByte < 128)			/* an ENGLISH character */
		{
			unsigned int offset = cHByte * ASC_H;
			if (offset > LENGTH_OF_ASC) continue;
			
			memcpy (character_matrix, (asc12 + offset), ASC_H);
			
			LCDBitBlt (pmem, buf_w, buf_h, current_x, current_y, ASC_W+ASC_GAP, ASC_H, mode, character_matrix, 8, ASC_H, 0, 0);
			current_x += (ASC_W + ASC_GAP);
		}
		else					/* a CHINESE character */
		{

#if defined(CHINESE_SUPPORT)
			cLByte = *(buffer + current);
			current ++;
			if (cLByte == 0) return;		/* found the END character */
			
			unsigned int offset = GetHzIndex( cHByte, cLByte, (HZK_H * 2) );
			if (offset < 0) continue;
			if (offset > LENGTH_OF_HZK) continue;

			memcpy (character_matrix, (hzk12 + offset), (HZK_H * 2));

			LCDBitBlt (pmem, buf_w, buf_h, current_x, current_y, HZK_W+HZK_GAP, HZK_H, mode, character_matrix, 16, HZK_H, 0, 0);
			current_x += (ASC_W + ASC_GAP) * 2;
#else
			return;
#endif // defined(CHINESE_SUPPORT)

		}
    }
}

/* copy context from one FrameBuffer-Compatible buffer to another */
void CLCD::LCDCopy (unsigned char* obuffer, unsigned char* sbuffer, int buf_w, int buf_h, int mode)
{
    if ((obuffer == sbuffer) && (mode == 0))
	return;

    int buffer_length = (buf_w * buf_h >> 3);
    if (mode == 0)
    {
		memcpy (obuffer, sbuffer, buffer_length);
    }
    else if (mode == 1)
    {
		int i;
		for (i=0; i<buffer_length; i++)
		{
			*(obuffer + i) = ~ *(sbuffer + i);	/* inverse each byte */
		}
    }
    else if (mode == 2)
    {
		memset (obuffer, 0x0, buffer_length);
    }
    else if (mode == 3)
    {
		memset (obuffer, 0xFF, buffer_length);
    }
}

/* copy a rect area from one FrameBuffer_Compatible buffer to another */
void CLCD::LCDBitBlt (unsigned char* obuffer, int buf_w, int buf_h, int x, int y, int w, int h, int mode, unsigned char* sbuffer, int sw, int sh, int sx, int sy)
{
    if (w <= 0) return;
    if (h <= 0) return;
    if (x > buf_w) return;
    if (y > buf_h) return;
    
    int width = w;
    if ((x+width) > buf_w)
		width = buf_w - x;
    if ((sx+width) > sw)
		width = sw - sx;
    
    int height = h;
    if ((y+height) > buf_h)
		height = buf_h - y;
    if ((sy+height) > sh)
		height = sh - sy;
    
    int i;
    int j;
    for (i=0; i<width; i++)
    {
        for (j=0; j<height; j++)
        {
			int c = LCDGetPixel (sbuffer, sw, sh, sx+i, sy+j);
			switch (mode)
			{
			case 0:		/* normal */
				{
					LCDSetPixel (obuffer, buf_w, buf_h, x+i, y+j, c);
				break;
			}
			case 1:		/* inverse */
			{
				c = (c==0 ? 1 : 0);
				LCDSetPixel (obuffer, buf_w, buf_h, x+i, y+j, c);
				break;
			}
			case 2:		/* or mode */
			{
				if (c == 1)
				{
					LCDSetPixel (obuffer, buf_w, buf_h, x+i, y+j, c);
				}
				break;
			}
			case 3:		/* and mode */
			{
				if (c == 0)
				{
					LCDSetPixel (obuffer, buf_w, buf_h, x+i, y+j, c);
				}
				break;
			}
			case 4:		/* nor mode */
			{
				if (c == 1)
				{
					c = LCDGetPixel (obuffer, buf_w, buf_h, x+i, y+j);
					c = (c==0 ? 1 : 0);
					LCDSetPixel (obuffer, buf_w, buf_h, x+i, y+j, c);
				}
				break;
			}
			default:
				{
					DebugPrintf("ERR: invalid parameter: LCDBitBlt function, para 6, mode. ");
				}
			}
		}
    }
}

void CLCD::LCDLine(unsigned char* pmem, int buf_w, int buf_h, int x1, int y1, int x2, int y2, int color)
{
	// ����Ǵ�ֱ�ߣ���ֱ�ӵ��û������ߵĺ���
	if( x1 == x2 )
	{
		int y = (y1 < y2)? y1 : y2;

		int dy = y1 - y2;
		if( dy < 0 )
			dy = -dy;

		LCDVLine( pmem,buf_w,buf_h, x1, y, (dy+1), color );
		return;
	}

	// �����ˮƽֱ�ߣ���ֱ�ӵ��û��ƺ��ߵĺ���
	if( y1 == y2 )
	{
		int x = (x1 < x2)? x1 : x2;

		int dx = x1 - x2;
		if( dx < 0 )
			dx = -dx;

		LCDHLine( pmem,buf_w,buf_h, x, y1, (dx+1), color );
		return;
	}

	int dx = x1 - x2;
	if( dx < 0 )
		dx = -dx;

	int dy = y1 - y2;
	if( dy < 0 )
		dy = -dy;

	if( dx > dy )
	{
		// �������꣬ʹX1Y1��Ϊ��ߵĵ�
		if( x1 > x2 )
		{
			int x = x1;
			int y = y1;
			x1    = x2;
			x2    = x;
			y1    = y2;
			y2    = y;
		}

		if( y1 < y2 )
		{
			int offset = 0;
			int i;
			for( i=0; i<dx; i++ )
			{
				LCDSetPixel(pmem, buf_w, buf_h, x1, y1, color);
				offset += dy;
				if(offset>dx)
				{
					offset -= dx;
					y1++;
				}
				x1++;
			}
		}
		else
		{
			int offset = 0;
			int i;
			for( i=0; i<dx; i++ )
			{
				LCDSetPixel(pmem, buf_w, buf_h, x1, y1, color);
				offset += dy;
				if(offset>dx)
				{
					offset -= dx;
					y1--;
				}
				x1++;
			}
		}
	}
	else
	{
		// �������꣬ʹX1Y1��Ϊ�ϵĵ�
		if( y1 > y2 )
		{
			int x = x1;
			int y = y1;
			x1    = x2;
			x2    = x;
			y1    = y2;
			y2    = y;
		}

		if( x1 < x2 )
		{
			int offset = 0;
			int i;
			for( i=0; i<dy; i++ )
			{
				LCDSetPixel(pmem, buf_w, buf_h, x1, y1, color);
				offset += dx;
				if(offset>dy)
				{
					offset -= dy;
					x1++;
				}
				y1++;
			}
		}
		else
		{
			int offset = 0;
			int i;
			for( i=0; i<dy; i++ )
			{
				LCDSetPixel(pmem, buf_w, buf_h, x1, y1, color);
				offset += dx;
				if(offset>dy)
				{
					offset -= dy;
					x1--;
				}
				y1++;
			}
		}
	}
}

/* end */
