// TSystemBar.cpp
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTSystemBar::CTSystemBar()
{
	m_nStatus  = 0;
	m_nBattery = 0;
	m_bCaps    = FALSE;
}

CTSystemBar::~CTSystemBar()
{
}

// 显示系统状态条；
void CTSystemBar::Show( CLCD* pLCD )
{
	if( m_nStatus == 0 )
		return;

	int fbw;
	int fbh;
	unsigned char* fb = pLCD->LCDGetFB (&fbw, &fbh);

	// 绘制底色和外框
	int x = SCREEN_W - SYSTEM_BAR_W;
	int y = 0;
	int w = SYSTEM_BAR_W;
	int h = SYSTEM_BAR_H;
	pLCD->LCDFillRect (fb, fbw, fbh, x, y, w, h, 0);
	pLCD->LCDHLine (fb, fbw, fbh, x, y+h-1, w, 1);
	pLCD->LCDVLine (fb, fbw, fbh, x, y, h, 1);

	// 绘制电源/电池图标
	if( m_nStatus == 1 )
	{
		pLCD->LCDDrawImage(fb, fbw, fbh, x+4, 1, 42, 13, LCD_MODE_INVERSE,
							pBitmap_Power, 42, 13, 0, 0 );
	}
	else
	{
		pLCD->LCDDrawImage(fb, fbw, fbh, x+4, 1, 42, 13, LCD_MODE_INVERSE,
							pBitmap_Battery, 42, 13, 0, 0 );
		// 绘制电量
		int iLen = (m_nBattery + 1) * 37 / 101;
		pLCD->LCDFillRect( fb, fbw, fbh, x+6, 3, iLen, 9, 1 );
	}

	// 绘制大写/小写图标
	if( m_bCaps )
		pLCD->LCDDrawImage(fb, fbw, fbh, x+48, 1, 12, 13, LCD_MODE_INVERSE,
							pBitmap_Captial, 12, 13, 0, 0 );
	else
		pLCD->LCDDrawImage(fb, fbw, fbh, x+48, 1, 12, 13, LCD_MODE_INVERSE,
							pBitmap_Lowercase, 12, 13, 0, 0 );
}

#if defined(MOUSE_SUPPORT)
// 鼠标点击切换大小写状态处理
BOOL CTSystemBar::PtProc( int x, int y )
{
	if( m_nStatus == 0 )
		return FALSE;

	int left   = SCREEN_W - SYSTEM_BAR_W + 48;
	int top    = 1;
	int right  = left + 12;
	int bottom = top  + 13;
	if( PtInRect( x, y, left, top, right, bottom ) )
	{
		if( m_bCaps )
			m_bCaps = FALSE;
		else
			m_bCaps = TRUE;

		return TRUE;
	}

	return FALSE;
}
#endif // defined(MOUSE_SUPPORT)

// 设置状态：0:不显示；1:显示充电；2:显示电池；
BOOL CTSystemBar::SetStatus( int nStatus )
{
	m_nStatus = nStatus;
	return TRUE;
}

// 设置电池电量值，0 ~ 100；
BOOL CTSystemBar::SetBattery( int nValue )
{
	if( (nValue<0) || (nValue >100) )
		return FALSE;

	m_nBattery = nValue;
	return TRUE;
}

// 得到当前电池电量值；
int CTSystemBar::GetBattery()
{
	return m_nBattery;
}

// 设置大小写状态指示；
BOOL CTSystemBar::SetCaps( BOOL bValue )
{
	m_bCaps = bValue;
	return TRUE;
}

// 得到当前的大小写状态；
BOOL CTSystemBar::GetCaps()
{
	return m_bCaps;
}
/* END */
