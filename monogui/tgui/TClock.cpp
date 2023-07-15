// TClock.cpp

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTClock::CTClock( CTApp* pApp )
{
	m_pApp = pApp;
	m_nStatus = 0;
	m_lLastTime = sys_clock()-1000;
	memset( m_psTime, 0x0, 128 );
	memset( m_psDate, 0x0, 128 );
	m_pcaBuffer = new unsigned char [(SCREEN_W + 7)/8*SCREEN_H];

#if defined(MOUSE_SUPPORT)
	m_iClockButtonX = -1;
	m_iClockButtonY = -1;
#endif // defined(MOUSE_SUPPORT)
}

CTClock::~CTClock()
{
	if( m_pcaBuffer != NULL )
		delete [] m_pcaBuffer;
}

void CTClock::Enable( BOOL bEnable )
{
	if( bEnable )
	{
		m_nStatus = 1;
		m_lLastTime = sys_clock()-1000;
	}
	else
	{
		m_nStatus = 0;
		_TMSG msg;
		msg.pWnd = m_pApp->m_pMainWnd;
		msg.message = TMSG_PAINT;
		msg.wParam = 0;
		msg.lParam = 0;
		m_pApp->PostMessage( &msg );
	}
}

BOOL CTClock::IsEnable()
{
	if( m_nStatus == 1 )
		return TRUE;

	return FALSE;
}

void CTClock::Check( CLCD* pLCD, CLCD* pBuf )
{
	if( m_nStatus = 1 )
	{
		ULONGLONG lNow = sys_clock();

		// ����100��������ˢ����ʾ
		if ((lNow - m_lLastTime) >= 100)
		{
			m_lLastTime = lNow;

			// �õ�ϵͳʱ��
#if defined(RUN_ENVIRONMENT_LINUX)
			struct tm  stTime;
			
			int nYear   = stTime.tm_year+1900;
			int nMonth  = stTime.tm_mon+1;
			int nDay    = stTime.tm_mday;
			int nHour   = stTime.tm_hour;
			int nMinute = stTime.tm_min;
			int nSecond = stTime.tm_sec;
#endif // defined(RUN_ENVIRONMENT_LINUX)

#if defined(RUN_ENVIRONMENT_WIN32)
			time_t time_val;
			time_t tloc;
			struct tm* pTime;
			time_val = time( &tloc );
			if( time_val != (time_t)-1 )
			{
				pTime	= gmtime( &time_val );
				stTime.nYear   = pTime->tm_year+1900;
				stTime.nMonth  = pTime->tm_mon+1;
				stTime.nDay    = pTime->tm_mday;
				stTime.nHour   = pTime->tm_hour;
				stTime.nMinute = pTime->tm_min;
				stTime.nSecond = pTime->tm_sec;
			}

			int nYear   = stTime.nYear;
			int nMonth  = stTime.nMonth;
			int nDay    = stTime.nDay;
			int nHour   = stTime.nHour;
			int nMinute = stTime.nMinute;
			int nSecond = stTime.nSecond;

			// ����ʱ��
			nHour += 8;
			if( nHour >= 24 )
				nHour -= 24;
#endif // defined(RUN_ENVIRONMENT_WIN32)

			// �����������ݿ��������Ա��������
			int w;
			int h;
			unsigned char* fb = pBuf->LCDGetFB( &w, &h );
			pBuf->LCDCopy( m_pcaBuffer, fb, w, h, 0 );

			// ���Ʊ���
			pBuf->LCDDrawImage( m_pcaBuffer,w,h, CLOCK_X, CLOCK_Y,
				w,h, LCD_MODE_NORMAL, pcaClockFace, CLOCK_W, CLOCK_H, 0, 0 );

			// �������ֵ����ں�ʱ��
#if defined(CHINESE_SUPPORT)
			sprintf( m_psDate, "%4d��%2d��%2d��",   nYear,nMonth,nDay );
			sprintf( m_psTime, " %2d ʱ%2d��%2d��", nHour,nMinute,nSecond );
			pBuf->LCDTextOut( m_pcaBuffer,w,h, CLOCK_X+82, CLOCK_Y+34,
				LCD_MODE_INVERSE, (unsigned char *)m_psDate, 14 );
			pBuf->LCDTextOut( m_pcaBuffer,w,h, CLOCK_X+82, CLOCK_Y+52,
				LCD_MODE_INVERSE, (unsigned char *)m_psTime, 14 );
#else
			sprintf( m_psDate,   "%4d/%2d/%2d", nYear,nMonth,nDay );
			sprintf( m_psTime, "  %2d:%2d:%2d", nHour,nMinute,nSecond );
			pBuf->LCDTextOut( m_pcaBuffer,w,h, CLOCK_X+96, CLOCK_Y+32,
				LCD_MODE_INVERSE, (unsigned char *)m_psDate, 14 );
			pBuf->LCDTextOut( m_pcaBuffer,w,h, CLOCK_X+96, CLOCK_Y+50,
				LCD_MODE_INVERSE, (unsigned char *)m_psTime, 14 );
#endif // defined(CHINESE_SUPPORT)

			// ������������
			  // 1��������
			    // ȷ����ʼ��
			int x1 = CLOCK_X+CENTER_X+1;
			int y1 = CLOCK_Y+CENTER_Y+1;
			if( (nSecond>0) && (nSecond<=15) )
			{
				x1 += 1;
			}
			else if( (nSecond>15) && (nSecond<=30) )
			{
				x1 += 1;
				y1 += 1;
			}
			else if( (nSecond>30) && (nSecond<=45) )
			{
				y1 += 1;
			}

			    // ȷ����ֹ��
			int x2 = CLOCK_X+MINUTE_HAND[nSecond%60][0]+1;
			int y2 = CLOCK_Y+MINUTE_HAND[nSecond%60][1]+1;
			    // ��������
			pBuf->LCDLine( m_pcaBuffer,w,h, x1,y1,x2,y2, 0 );

			  // 2���Ʒ���
			    // ȷ����ʼ��
			x1 = CLOCK_X+CENTER_X+1;
			y1 = CLOCK_Y+CENTER_Y+1;
			    // ȷ����ֹ��
			x2 = CLOCK_X+MINUTE_HAND[nMinute%60][0]+1;
			y2 = CLOCK_Y+MINUTE_HAND[nMinute%60][1]+1;
			    // ���Ʒ��루������3���أ�
			pBuf->LCDLine( m_pcaBuffer,w,h, x1,y1,x2,y2, 0 );
			x1 += 1;
			x2 += 1;
			pBuf->LCDLine( m_pcaBuffer,w,h, x1,y1,x2,y2, 0 );
			y1 += 1;
			y2 += 1;
			pBuf->LCDLine( m_pcaBuffer,w,h, x1,y1,x2,y2, 0 );
			x1 -= 1;
			x2 -= 1;
			pBuf->LCDLine( m_pcaBuffer,w,h, x1,y1,x2,y2, 0 );

			  // 3����ʱ��
			    // ȷ����ʼ��
			x1 = CLOCK_X+CENTER_X+1;
			y1 = CLOCK_Y+CENTER_Y+1;
			    // ȷ����ֹ��
			int nIndex = (nHour % 12) * 5 + nMinute / 12;
			x2 = CLOCK_X+CENTER_X+1+(MINUTE_HAND[nIndex][0]-CENTER_X)*2/3;
			y2 = CLOCK_Y+CENTER_Y+1+(MINUTE_HAND[nIndex][1]-CENTER_Y)*2/3;
			    // ����ʱ�루ʱ����3���أ�
			pBuf->LCDLine( m_pcaBuffer,w,h, x1,y1,x2,y2, 0 );
			x1 += 1;
			x2 += 1;
			pBuf->LCDLine( m_pcaBuffer,w,h, x1,y1,x2,y2, 0 );
			y1 += 1;
			y2 += 1;
			pBuf->LCDLine( m_pcaBuffer,w,h, x1,y1,x2,y2, 0 );
			x1 -= 1;
			x2 -= 1;
			pBuf->LCDLine( m_pcaBuffer,w,h, x1,y1,x2,y2, 0 );

			// �����ƺõĻ�����������Ļ
			fb = pLCD->LCDGetFB( &w, &h );
			pLCD->LCDCopy( fb, m_pcaBuffer, w, h, 0 );

			pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );

#if defined(RUN_ENVIRONMENT_WIN32)
			// ������Win32��������
			::Sleep(600);
#endif // defined(RUN_ENVIRONMENT_WIN32)
		}
	}
}

#if defined(MOUSE_SUPPORT)
// �����ʱ�Ӱ�ť�Ĵ���
BOOL CTClock::PtProc( int x, int y )
{
	if( m_nStatus == 0 )
	{
		if( (m_iClockButtonX != -1) && (m_iClockButtonY != -1) )
		{
			if( PtInRect( x, y, m_iClockButtonX, m_iClockButtonY,
				m_iClockButtonX+15, m_iClockButtonY+15 ) )
			{
				if( IsEnable() )
					Enable( FALSE );
				else
					Enable( TRUE );

				return TRUE;
			}
		}
	}

	return FALSE;
}

// ��ʾʱ�Ӱ�ť
void CTClock::ShowClockButton( CLCD* pLCD )
{
	// ����ʱ�Ӱ�ť
	if( m_nStatus == 0 )
	{
		int w;
		int h;
		unsigned char* fb;
		fb = pLCD->LCDGetFB( &w,&h );

		if( (m_iClockButtonX != -1) && (m_iClockButtonY != -1) )
		{
			pLCD->LCDDrawImage( fb,w,h,m_iClockButtonX,m_iClockButtonY,
								15,15,SCREEN_MODE,pBitmap_Clock_Button,15,15,0,0);
		}
	}
}

// ����ʱ�Ӱ�ť��λ��
BOOL CTClock::SetClockButton( int x, int y )
{
	// ������Чֵʹʱ�Ӱ�ť��ʧ
	if( x<0 || x>SCREEN_W || y<0 || y>SCREEN_H )
	{
		m_iClockButtonX = -1;
		m_iClockButtonY = -1;
		return FALSE;
	}

	m_iClockButtonX = x;
	m_iClockButtonY = y;
	return TRUE;
}
#endif // defined(MOUSE_SUPPORT)
/* END */
