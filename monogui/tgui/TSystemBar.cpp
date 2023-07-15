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

// ��ʾϵͳ״̬����
void CTSystemBar::Show( CLCD* pLCD )
{
	if( m_nStatus == 0 )
		return;

	int fbw;
	int fbh;
	unsigned char* fb = pLCD->LCDGetFB (&fbw, &fbh);

	// ���Ƶ�ɫ�����
	int x = SCREEN_W - SYSTEM_BAR_W;
	int y = 0;
	int w = SYSTEM_BAR_W;
	int h = SYSTEM_BAR_H;
	pLCD->LCDFillRect (fb, fbw, fbh, x, y, w, h, 0);
	pLCD->LCDHLine (fb, fbw, fbh, x, y+h-1, w, 1);
	pLCD->LCDVLine (fb, fbw, fbh, x, y, h, 1);

	// ���Ƶ�Դ/���ͼ��
	if( m_nStatus == 1 )
	{
		pLCD->LCDDrawImage(fb, fbw, fbh, x+4, 1, 42, 13, LCD_MODE_INVERSE,
							pBitmap_Power, 42, 13, 0, 0 );
	}
	else
	{
		pLCD->LCDDrawImage(fb, fbw, fbh, x+4, 1, 42, 13, LCD_MODE_INVERSE,
							pBitmap_Battery, 42, 13, 0, 0 );
		// ���Ƶ���
		int iLen = (m_nBattery + 1) * 37 / 101;
		pLCD->LCDFillRect( fb, fbw, fbh, x+6, 3, iLen, 9, 1 );
	}

	// ���ƴ�д/Сдͼ��
	if( m_bCaps )
		pLCD->LCDDrawImage(fb, fbw, fbh, x+48, 1, 12, 13, LCD_MODE_INVERSE,
							pBitmap_Captial, 12, 13, 0, 0 );
	else
		pLCD->LCDDrawImage(fb, fbw, fbh, x+48, 1, 12, 13, LCD_MODE_INVERSE,
							pBitmap_Lowercase, 12, 13, 0, 0 );
}

#if defined(MOUSE_SUPPORT)
// ������л���Сд״̬����
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

// ����״̬��0:����ʾ��1:��ʾ��磻2:��ʾ��أ�
BOOL CTSystemBar::SetStatus( int nStatus )
{
	m_nStatus = nStatus;
	return TRUE;
}

// ���õ�ص���ֵ��0 ~ 100��
BOOL CTSystemBar::SetBattery( int nValue )
{
	if( (nValue<0) || (nValue >100) )
		return FALSE;

	m_nBattery = nValue;
	return TRUE;
}

// �õ���ǰ��ص���ֵ��
int CTSystemBar::GetBattery()
{
	return m_nBattery;
}

// ���ô�Сд״ָ̬ʾ��
BOOL CTSystemBar::SetCaps( BOOL bValue )
{
	m_bCaps = bValue;
	return TRUE;
}

// �õ���ǰ�Ĵ�Сд״̬��
BOOL CTSystemBar::GetCaps()
{
	return m_bCaps;
}
/* END */
