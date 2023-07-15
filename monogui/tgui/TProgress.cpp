// TProgress.cpp: implementation of the CTProgress class.
//
//////////////////////////////////////////////////////////////////////
#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTProgress::CTProgress ()
{
	m_iRange = 1;
}

CTProgress::~CTProgress ()
{
}

// 创建进度条
BOOL CTProgress::Create (CTWindow* pParent,WORD wWndType,WORD wStyle,WORD wStatus,int x,int y,int w,int h,int ID)
{
	if (!CTWindow::Create (pParent,wWndType,wStyle,wStatus,x,y,w,h,ID))
	{
		return FALSE;
	}

	m_wStatus |= TWND_STATUS_INVALID;	// 进度条不能获得焦点
	return TRUE;
}

void CTProgress::Paint (CLCD* pLCD)
{
	// 如果不可见，则什么也不绘制
	if( !IsWindowVisible() )
		return;

	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// 绘制边框，填充背景
	pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 0);
	if ((m_wStyle & TWND_STYLE_NO_BORDER) == 0)
	{
		pLCD->LCDHLine (fb, w, h, m_x, m_y, m_w, 1);
		pLCD->LCDHLine (fb, w, h, m_x, m_y+m_h-1, m_w, 1);
		pLCD->LCDVLine (fb, w, h, m_x, m_y, m_h, 1);
		pLCD->LCDVLine (fb, w, h, m_x+m_w-1, m_y, m_h, 1);
	}

	// 绘制背景条
	pLCD->LCDFillRect (fb, w, h, m_x+2, m_y+2, m_w-4, m_h-4, 2);

	// 绘制前景条
	int iBarWidth = (int)((m_w-4) * m_iPosition / m_iRange);
	pLCD->LCDFillRect (fb, w, h, m_x+2, m_y+2, iBarWidth, m_h-4, 1);
}

int CTProgress::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	return 0;
}

// 设置整体范围
BOOL CTProgress::SetRange (int iRange)
{
	if (iRange > 1)
	{
		m_iRange = iRange;
		if (m_iPosition > m_iRange)
			m_iPosition = m_iRange;
		// 进度条的修改应当立即被更新显示
		Paint( m_pApp->m_pLCD );

		m_pApp->m_pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );

		return TRUE;
	}
	return FALSE;
}

// 设置当前进度
BOOL CTProgress::SetPos (int iPos)
{
	if (iPos >= 0)
	{
		m_iPosition = iPos;
		if (m_iPosition > m_iRange)
			m_iPosition = m_iRange;
		// 进度条的修改应当立即被更新显示
		Paint(m_pApp->m_pLCD);

		m_pApp->m_pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );

		return TRUE;
	}
	return FALSE;
}
/* END */
