// TButton.cpp: implementation of the CTButton class.
//
//////////////////////////////////////////////////////////////////////
#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTButton::CTButton ()
{
	m_pcaImage = NULL;
}

CTButton::~CTButton ()
{
	m_pcaImage = NULL;
}

BOOL CTButton::Create( CTWindow* pParent,WORD wWndType,WORD wStyle,WORD wStatus,int x,int y,int w,int h,int ID)
{
	if( !CTWindow::Create(pParent,TWND_TYPE_BUTTON,wStyle,wStatus,x,y,w,h,ID))
		return FALSE;

	return TRUE;
}

// 调入图片
BOOL CTButton::LoadImage( unsigned char* pcaImage )
{
	m_pcaImage = pcaImage;
	return TRUE;
}

// 虚函数，绘制按钮
void CTButton::Paint( CLCD* pLCD )
{
	// 如果不可见，则什么也不绘制
	if( !IsWindowVisible() )
		return;

	if( m_pcaImage == NULL )
		DrawCommonButton( pLCD );
	else
		DrawImageButton( pLCD );
}

// 绘制图像按钮
void CTButton::DrawImageButton( CLCD* pLCD )
{
	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// 计算文字的居中显示位置
	int tx = m_x + (m_w - 6 * GetTextLength()) / 2;
	int ty = m_y + (m_h - 12) / 2;

	// 根据按钮不同的状态绘制
	// 图片从上到下分别是：
	// 1>Normal样式的图片
	// 2>Focus样式的图片
	// 3>PushDown样式的图片

	// 图片宽度按字节对齐
	int iImageW = ((m_w + 7) / 8) * 8;

	// 绘制按下状态的按钮
	if ((m_wStatus & TWND_STATUS_PUSHED) > 0)
	{
		// 绘制底图
		pLCD->LCDBitBlt( fb,w,h,m_x,m_y,m_w,m_h,LCD_MODE_NORMAL,
						m_pcaImage,iImageW,m_h*3,0,m_h*2 );
		// 绘制反白的文字
		// pLCD->LCDTextOut (fb,w,h,tx,ty,1,(unsigned char*)m_cCaption,255);
		return;
	}

	// 绘焦点状态的按钮
	if ((m_wStatus & TWND_STATUS_FOCUSED) > 0)
	{
		// 绘制底图
		pLCD->LCDBitBlt( fb,w,h,m_x,m_y,m_w,m_h,LCD_MODE_NORMAL,
						m_pcaImage,iImageW,m_h*3,0,m_h );
		// 绘制正常的文字
		// pLCD->LCDTextOut (fb,w,h,tx,ty,0,(unsigned char*)m_cCaption,255);
		return;
	}

	// 绘制普通样式的按钮
	// 绘制底图
	pLCD->LCDBitBlt( fb,w,h,m_x,m_y,m_w,m_h,LCD_MODE_NORMAL,
					m_pcaImage,iImageW,m_h*3,0,0 );
	// 绘制正常的文字
	// pLCD->LCDTextOut (fb,w,h,tx,ty,0,(unsigned char*)m_cCaption,255);
}

// 绘制普通风格的按钮
void CTButton::DrawCommonButton( CLCD* pLCD )
{
	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// 计算文字的居中显示位置
	int tx = m_x + (m_w - 6 * GetTextLength()) / 2;
	int ty = m_y + (m_h - 12) / 2;

	// 根据按钮不同的状态绘制
	// 如果是按下状态，则按照按下状态绘制
	if ((m_wStatus & TWND_STATUS_PUSHED) > 0)
	{
		if ((m_wStyle & TWND_STYLE_ROUND_EDGE) > 0)
		{
			// 绘制圆角风格的按钮
			pLCD->LCDFillRect (fb, w, h, m_x+6, m_y, m_w-12, m_h, 1);
			pLCD->LCDFillRect (fb, w, h, m_x, m_y+6, 6, m_h-12, 1);
			pLCD->LCDFillRect (fb, w, h, m_x+m_w-6, m_y+6, 6, m_h-12, 1);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y, 6, 6, LCD_MODE_INVERSE, pBitmap_Button_Normal,13, 13, 0, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_INVERSE, pBitmap_Button_Normal,13, 13, 6, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y+m_h-6, 6, 6, LCD_MODE_INVERSE, pBitmap_Button_Normal,13, 13, 0, 6);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y+m_h-6, 6, 6, LCD_MODE_INVERSE, pBitmap_Button_Normal,13, 13, 6, 6);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y, 6, 6, LCD_MODE_OR, pBitmap_Button_Normal,13, 13, 0, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_OR, pBitmap_Button_Normal,13, 13, 6, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y+m_h-6, 6, 6, LCD_MODE_OR, pBitmap_Button_Normal,13, 13, 0, 6);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y+m_h-6, 6, 6, LCD_MODE_OR, pBitmap_Button_Normal,13, 13, 6, 6);
		}
		else
		{
			// 绘制普通风格的按钮
			pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 1);
		}
		// 绘制反白的文字
		pLCD->LCDTextOut (fb,w,h,tx,ty,1,(unsigned char*)m_cCaption,255);
		return;
	}

	// 如果是默认的，则绘制宽边按钮。如果不是默认，则绘制普通按钮
	if ((m_wStatus & TWND_STATUS_DEFAULT) > 0)
	{
		// 绘制默认风格的按钮
		if ((m_wStyle & TWND_STYLE_ROUND_EDGE) > 0)
		{
			// 绘制圆角风格的按钮外框
			pLCD->LCDFillRect (fb, w, h, m_x+6, m_y+1, m_w-12, m_h-2, 0);
			pLCD->LCDFillRect (fb, w, h, m_x+1, m_y+6, 5, m_h-12, 0);
			pLCD->LCDFillRect (fb, w, h, m_x+m_w-6, m_y+6, 5, m_h-12, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Default,13, 13, 0, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y, 7, 6, LCD_MODE_NORMAL, pBitmap_Button_Default,13, 13, 6, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y+m_h-6, 6, 7, LCD_MODE_NORMAL, pBitmap_Button_Default,13, 13, 0, 6);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y+m_h-6, 7, 7, LCD_MODE_NORMAL, pBitmap_Button_Default,13, 13, 6, 6);
			pLCD->LCDHLine (fb, w, h, m_x+6, m_y, m_w-12, 1);
			pLCD->LCDVLine (fb, w, h, m_x, m_y+6, m_h-12, 1);
			pLCD->LCDFillRect(fb, w, h, m_x+6, m_y+m_h-1, m_w-12, 2, 1);
			pLCD->LCDFillRect(fb, w, h, m_x+m_w-1, m_y+6, 2, m_h-12, 1);
		}
		else
		{
			// 绘制普通风格的按钮外框
			pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 0);
			pLCD->LCDHLine (fb, w, h, m_x, m_y, m_w, 1);
			pLCD->LCDHLine (fb, w, h, m_x, m_y+m_h-1, m_w, 1);
			pLCD->LCDVLine (fb, w, h, m_x, m_y, m_h, 1);
			pLCD->LCDVLine (fb, w, h, m_x+m_w-1, m_y, m_h, 1);
			pLCD->LCDHLine (fb, w, h, m_x+1, m_y+m_h, m_w, 1);
			pLCD->LCDVLine (fb, w, h, m_x+m_w, m_y+1, m_h, 1);
		}
	}
	else
	{
		// 绘制普通风格的按钮
		if ((m_wStyle & TWND_STYLE_ROUND_EDGE) > 0)
		{
			// 绘制圆角风格的按钮外框
			pLCD->LCDFillRect (fb, w, h, m_x+6, m_y+1, m_w-12, m_h-2, 0);
			pLCD->LCDFillRect (fb, w, h, m_x+1, m_y+6, 5, m_h-12, 0);
			pLCD->LCDFillRect (fb, w, h, m_x+m_w-6, m_y+6, 5, m_h-12, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal,13, 13, 0, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal,13, 13, 6, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y+m_h-6, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal,13, 13, 0, 6);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y+m_h-6, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal,13, 13, 6, 6);
			pLCD->LCDHLine (fb, w, h, m_x+6, m_y, m_w-12, 1);
			pLCD->LCDVLine (fb, w, h, m_x, m_y+6, m_h-12, 1);
			pLCD->LCDHLine(fb, w, h, m_x+6, m_y+m_h-1, m_w-12, 1);
			pLCD->LCDVLine(fb, w, h, m_x+m_w-1, m_y+6, m_h-12, 1);
		}
		else
		{
			// 绘制普通风格的按钮外框
			pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 0);
			pLCD->LCDHLine (fb, w, h, m_x, m_y, m_w, 1);
			pLCD->LCDHLine (fb, w, h, m_x, m_y+m_h-1, m_w, 1);
			pLCD->LCDVLine (fb, w, h, m_x, m_y, m_h, 1);
			pLCD->LCDVLine (fb, w, h, m_x+m_w-1, m_y, m_h, 1);
		}
	}
	// 如果处于焦点，再绘制虚线框
	if ((m_wStatus & TWND_STATUS_FOCUSED) > 0)
	{
		if ((m_wStyle & TWND_STYLE_ROUND_EDGE) > 0)
		{
			// 圆角按钮，虚线框要避开四角的圆弧
			pLCD->LCDFillRect (fb,w,h,m_x+3,m_y+2,m_w-7,1,2);
			pLCD->LCDFillRect (fb,w,h,m_x+2,m_y+3,1,m_h-7,2);
			pLCD->LCDFillRect (fb,w,h,m_x+3,m_y+m_h-3,m_w-7,1,2);
			pLCD->LCDFillRect (fb,w,h,m_x+m_w-3,m_y+3,1,m_h-7,2);
		}
		else
		{
			pLCD->LCDFillRect (fb,w,h,m_x+2,m_y+2,m_w-5,1,2);
			pLCD->LCDFillRect (fb,w,h,m_x+2,m_y+2,1,m_h-5,2);
			pLCD->LCDFillRect (fb,w,h,m_x+2,m_y+m_h-3,m_w-5,1,2);
			pLCD->LCDFillRect (fb,w,h,m_x+m_w-3,m_y+2,1,m_h-5,2);
		}
	}
	// 绘制正常的文字
	pLCD->LCDTextOut (fb,w,h,tx,ty,0,(unsigned char*)m_cCaption,255);
}

// 虚函数，消息处理
// 消息处理过了，返回1，未处理返回0
int CTButton::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = 0;

	// 按钮只处理三种消息：1、空格键；2、回车键(只对处于默认状态的按钮有效)；3、HM_PUSHDOWN消息
	if (iMsg == TMSG_KEYDOWN)
	{
		if (wParam == TVK_SPACE)
		{
			if ((m_wStatus & TWND_STATUS_FOCUSED) > 0)
			{
				OnPushDown ();
			}
			iReturn = 1;
		}
		else if (wParam == TVK_ENTER)
		{
			if ((m_wStatus & TWND_STATUS_DEFAULT) > 0)
			{
				OnPushDown ();
			}
			iReturn = 1;
		}
	}
	else if (iMsg == TMSG_PUSHDOWN)
	{
		if (pWnd == this)
		{
			OnPushDown ();
		}
		iReturn = 1;
	}

	return iReturn;
}

#if defined(MOUSE_SUPPORT)
// 坐标设备消息处理
int CTButton::PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = CTWindow::PtProc (pWnd, iMsg, wParam, lParam);

	if (iMsg == TMSG_LBUTTONDOWN)
	{
		if (PtInWindow (wParam, lParam) )
		{
			OnPushDown();
			iReturn = 1;
		}
	}

	return iReturn;
}
#endif // defined(MOUSE_SUPPORT)

// 按键被按下的处理
void CTButton::OnPushDown ()
{
	// 刷新显示，向父窗口发出通知消息，再次刷新显示
	m_wStatus |= TWND_STATUS_PUSHED;
	Paint(m_pApp->m_pLCD);

	// 强制显示按钮更新的效果
	m_pApp->m_pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );

	// 延时，使按钮按下的状态可以被看清楚
	Delay( 200 );

	// 向父窗口发送消息值为本ID的消息
	_TMSG msg;
	msg.pWnd    = m_pParent;
	msg.message = TMSG_NOTIFY_PARENT;
	msg.wParam  = m_ID;
	msg.lParam  = 0;
	m_pApp->PostMessage (&msg);

	m_wStatus &= ~TWND_STATUS_PUSHED;
	Paint(m_pApp->m_pLCD);

	// 强制显示按钮更新的效果
	m_pApp->m_pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );
}
/* END */
