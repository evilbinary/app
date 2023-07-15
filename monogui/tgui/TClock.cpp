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

		// 超过100毫秒钟则刷新显示
		if ((lNow - m_lLastTime) >= 100)
		{
			m_lLastTime = lNow;

			// 得到系统时间
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

			// 修正时区
			nHour += 8;
			if( nHour >= 24 )
				nHour -= 24;
#endif // defined(RUN_ENVIRONMENT_WIN32)

			// 将缓冲区内容拷贝入类成员缓冲区中
			int w;
			int h;
			unsigned char* fb = pBuf->LCDGetFB( &w, &h );
			pBuf->LCDCopy( m_pcaBuffer, fb, w, h, 0 );

			// 绘制背景
			pBuf->LCDDrawImage( m_pcaBuffer,w,h, CLOCK_X, CLOCK_Y,
				w,h, LCD_MODE_NORMAL, pcaClockFace, CLOCK_W, CLOCK_H, 0, 0 );

			// 绘制数字的日期和时间
#if defined(CHINESE_SUPPORT)
			sprintf( m_psDate, "%4d年%2d月%2d日",   nYear,nMonth,nDay );
			sprintf( m_psTime, " %2d 时%2d分%2d秒", nHour,nMinute,nSecond );
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

			// 绘制两条表针
			  // 1绘制秒针
			    // 确定起始点
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

			    // 确定终止点
			int x2 = CLOCK_X+MINUTE_HAND[nSecond%60][0]+1;
			int y2 = CLOCK_Y+MINUTE_HAND[nSecond%60][1]+1;
			    // 绘制秒针
			pBuf->LCDLine( m_pcaBuffer,w,h, x1,y1,x2,y2, 0 );

			  // 2绘制分针
			    // 确定起始点
			x1 = CLOCK_X+CENTER_X+1;
			y1 = CLOCK_Y+CENTER_Y+1;
			    // 确定终止点
			x2 = CLOCK_X+MINUTE_HAND[nMinute%60][0]+1;
			y2 = CLOCK_Y+MINUTE_HAND[nMinute%60][1]+1;
			    // 绘制分针（分针宽度3像素）
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

			  // 3绘制时针
			    // 确定起始点
			x1 = CLOCK_X+CENTER_X+1;
			y1 = CLOCK_Y+CENTER_Y+1;
			    // 确定终止点
			int nIndex = (nHour % 12) * 5 + nMinute / 12;
			x2 = CLOCK_X+CENTER_X+1+(MINUTE_HAND[nIndex][0]-CENTER_X)*2/3;
			y2 = CLOCK_Y+CENTER_Y+1+(MINUTE_HAND[nIndex][1]-CENTER_Y)*2/3;
			    // 绘制时针（时针宽度3像素）
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

			// 将绘制好的缓冲区送上屏幕
			fb = pLCD->LCDGetFB( &w, &h );
			pLCD->LCDCopy( fb, m_pcaBuffer, w, h, 0 );

			pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );

#if defined(RUN_ENVIRONMENT_WIN32)
			// 仅用于Win32环境仿真
			::Sleep(600);
#endif // defined(RUN_ENVIRONMENT_WIN32)
		}
	}
}

#if defined(MOUSE_SUPPORT)
// 鼠标点击时钟按钮的处理
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

// 显示时钟按钮
void CTClock::ShowClockButton( CLCD* pLCD )
{
	// 绘制时钟按钮
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

// 设置时钟按钮的位置
BOOL CTClock::SetClockButton( int x, int y )
{
	// 传入无效值使时钟按钮消失
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
