// TStatic.cpp: implementation of the CTStatic class.
//
//////////////////////////////////////////////////////////////////////
#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTStatic::CTStatic ()
{
	m_pcaImage = NULL;
}

CTStatic::~CTStatic ()
{
	m_pcaImage = NULL;
}

// 创建静态文本
BOOL CTStatic::Create (CTWindow* pParent,WORD wWndType,WORD wStyle,WORD wStatus,int x,int y,int w,int h,int ID)
{
	if (!CTWindow::Create (pParent,TWND_TYPE_STATIC,wStyle,wStatus,x,y,w,h,ID))
	{
		DebugPrintf( "Create Static Control Failure. \n" );
		return FALSE;
	}

	//DebugPrintf( "Create Static Control Success. \n" );
	m_wStatus |= TWND_STATUS_INVALID;	// 静态文本不能获得焦点
	return TRUE;
}

// 调入图片
BOOL CTStatic::LoadImage( unsigned char* pcaImage )
{
	m_pcaImage = pcaImage;
	return TRUE;
}

void CTStatic::Paint (CLCD* pLCD)
{
	// 如果不可见，则什么也不绘制
	if( !IsWindowVisible() )
		return;

	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// 图形的绘制
	if( m_pcaImage != NULL )
	{
		// 图片宽度按字节对齐
		int iImageW = ((m_w + 7) / 8) * 8;

		pLCD->LCDBitBlt( fb,w,h,m_x,m_y,m_w,m_h,LCD_MODE_NORMAL,
				m_pcaImage,iImageW,m_h,0,0 );

		return;
	}

	int iTitleLength = 0;	// 显示字符串长度限制

	// 如果有边框，则绘制边框
	if ((m_wStyle & TWND_STYLE_NO_BORDER) == 0)
	{
		if ((m_wStyle & TWND_STYLE_ROUND_EDGE) > 0)
		{
			// 绘制圆角风格的边框
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y+7, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal,13, 13, 0, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y+7, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal,13, 13, 6, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y+m_h-6, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal,13, 13, 0, 6);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y+m_h-6, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal,13, 13, 6, 6);
			pLCD->LCDHLine (fb, w, h, m_x+6, m_y+7, m_w-12, 1);
			pLCD->LCDHLine (fb, w, h, m_x+6, m_y+m_h-1, m_w-12, 1);
			pLCD->LCDVLine (fb, w, h, m_x, m_y+13, m_h-19, 1);
			pLCD->LCDVLine (fb, w, h, m_x+m_w-1, m_y+13, m_h-19, 1);
		}
		else
		{
			// 绘制普通风格的边框
			pLCD->LCDHLine (fb, w, h, m_x, m_y+7, m_w, 1);
			pLCD->LCDHLine (fb, w, h, m_x, m_y+m_h-1, m_w, 1);
			pLCD->LCDVLine (fb, w, h, m_x, m_y+7, m_h-7, 1);
			pLCD->LCDVLine (fb, w, h, m_x+m_w-1, m_y+7, m_h-7, 1);
		}
		// 绘制文字
		if( GetTextLength() > 0 )
		{
			pLCD->LCDVLine (fb, w, h, m_x+5, m_y, 12, 0);	// 字前留空
		}

		iTitleLength = GetDisplayLimit( m_cCaption, m_w-6 );
		pLCD->LCDTextOut( fb,w,h,m_x+6,m_y+2,0,(unsigned char*)m_cCaption,iTitleLength );
	}
	else
	{
		// 无边框时文字前不留空
		iTitleLength = GetDisplayLimit( m_cCaption, m_w );
		pLCD->LCDTextOut (fb,w,h,m_x,m_y+2,0,(unsigned char*)m_cCaption,iTitleLength);
	}
}

// Static控件什么也不处理
int CTStatic::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	return 0;
}
/* END */
