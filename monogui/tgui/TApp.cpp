// TApp.cpp: implementation of the CTApp class.
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTApp::CTApp()
{
#if defined(RUN_ENVIRONMENT_LINUX)
	m_pLCD        = new CLCD();
#endif // defined(RUN_ENVIRONMENT_LINUX)

	m_pLCDBuf     = new CLCD();
	m_pMsgQ       = new CTMessageQueue();
	m_pMainWnd    = NULL;
	m_pCaret      = new CTCaret();
	m_pTimerQ     = new CTTimerQueue();
	m_pTopMostWnd = NULL;
	m_pKeyMap     = new CTKeyMapMgt();
	m_pClock      = new CTClock( this );
	m_pSysBar     = new CTSystemBar();

#if defined(CHINESE_SUPPORT)
	m_pIMEWnd     = new CIMEWindow();
#endif // defined(CHINESE_SUPPORT)

#if defined(RESMGT_SUPPORT)
	m_pDtmMgt     = new CTDtmMgt();
	m_pImgMgt     = new CTImgMgt();
#endif // defined(RESMGT_SUPPORT)

	m_bClockKeyEnable      = TRUE;

}

CTApp::~CTApp()
{
#if defined(RUN_ENVIRONMENT_LINUX)
	if( m_pLCD != NULL )
		delete m_pLCD;
#endif // defined(RUN_ENVIRONMENT_LINUX)

	if( m_pLCDBuf != NULL )
		delete m_pLCDBuf;
	if( m_pMsgQ != NULL )
		delete m_pMsgQ;
	m_pMainWnd = NULL;
	if( m_pCaret != NULL )
		delete m_pCaret;
	if( m_pTimerQ != NULL )
		delete m_pTimerQ;
	m_pTopMostWnd = NULL;
	if( m_pKeyMap != NULL )
		delete m_pKeyMap;
	if( m_pClock != NULL )
		delete m_pClock;
	if( m_pSysBar != NULL )
		delete m_pSysBar;

#if defined(CHINESE_SUPPORT)
	if( m_pIMEWnd != NULL )
		delete m_pIMEWnd;
#endif // defined(CHINESE_SUPPORT)

#if defined(RESMGT_SUPPORT)
	if( m_pDtmMgt != NULL )
		delete m_pDtmMgt;

	if( m_pImgMgt != NULL )
		delete m_pImgMgt;
#endif // defined(RESMGT_SUPPORT)
}

// 绘制窗口
void CTApp::PaintWindows( CTWindow* pWnd )
{
	// 擦除buf
	int w;
	int h;
	unsigned char* fb;
	fb = m_pLCDBuf->LCDGetFB( &w,&h );
	m_pLCDBuf->LCDCopy( fb,fb,w,h,2 );

	// 调用主窗口的绘制函数
	pWnd->Paint( m_pLCDBuf );

#if defined(CHINESE_SUPPORT)

	// 调用输入法窗口的绘制函数
	m_pIMEWnd->Paint (m_pLCDBuf);
#endif // defined(CHINESE_SUPPORT)

	// 绘制TopMost窗口
	if( IsWindowValid( m_pTopMostWnd ) )
		m_pTopMostWnd->Paint( m_pLCDBuf );

	// 绘制系统状态条
	m_pSysBar->Show( m_pLCDBuf );

#if defined(MOUSE_SUPPORT)
	// 绘制时钟按钮
	m_pClock->ShowClockButton( m_pLCDBuf );
#endif // defined(MOUSE_SUPPORT)

	// 将绘制好的buf传给真正的屏幕
	unsigned char* realfb = m_pLCD->LCDGetFB(&w,&h);
	m_pLCD->LCDCopy (realfb,fb,w,h,0);

	// 绘制脱字符
	m_pCaret->DrawCaret (m_pLCD, m_pLCDBuf);

	m_pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );
}

BOOL CTApp::Create( CLCD* pLCD, CTWindow* pMainWnd )
{
	if( pLCD == NULL )
		return FALSE;

	if( pMainWnd == NULL )
		return FALSE;

	m_pLCD = pLCD;

#if defined(CHINESE_SUPPORT)
	m_pLCDBuf->BufferInit( pLCD->asc12, pLCD->hzk12 );
#else
	m_pLCDBuf->BufferInit( pLCD->asc12, NULL );
#endif // defined(CHINESE_SUPPORT)

	m_pMainWnd = pMainWnd;
	m_pMainWnd->m_pApp = this;

	// 创建按键宏与按键值对照表
	if( !(m_pKeyMap->Load( KEY_MAP_FILE )) )
		exit(1);

	// 显示开始画面
	ShowStart ();

	m_pMainWnd->UpdateView( m_pMainWnd );
	m_pMainWnd->OnInit();

#if defined(CHINESE_SUPPORT)
	// 创建输入法窗口
	if( !m_pIMEWnd->Create(this) )
		return FALSE;
#endif // defined(CHINESE_SUPPORT)

#if defined(RESMGT_SUPPORT)
	if( !m_pDtmMgt->Init( DIRECTORY_DTM ) )
		return FALSE;

	if( !m_pImgMgt->Init( DIRECTORY_IMG ) )
		return FALSE;
#endif // defined(RESMGT_SUPPORT)

	return TRUE;
}

BOOL CTApp::Run ()
{
	int iMsgInfo = 1;
	_TMSG msg;

	while( iMsgInfo != 0 )
	{
		int iMsgInfo = m_pMsgQ->GetMessage( &msg );

		if( iMsgInfo == 1 )
			DespatchMessage(&msg);
		else if(iMsgInfo == -1)
			Idle();
	}

	// run循环结束，退出程序
	return TRUE;
}

// 专用于仿真环境
void CTApp::RunInEmulator()
{
	int iMsgInfo = 1;
	_TMSG msg;

	while (iMsgInfo != -1)
	{
		iMsgInfo = m_pMsgQ->GetMessage (&msg);
		if (iMsgInfo == 1)
		{
			DespatchMessage(&msg);
		}
		else if (iMsgInfo == 0)
		{
			// 退出模拟器程序
			exit(1);
		}
	}

	Idle();
}

// 消息队列空状态下的处理
void CTApp::Idle()
{
	GetKeyboardEvent();				// 将按键事件转换为消息

	m_pTimerQ->CheckTimer(this);		// 检查定时器队列，有到时的则发送定时器消息
	// 如果CLOCK窗口处于打开状态，则不更新脱字符
	if( m_pClock->IsEnable() )
		m_pClock->Check( m_pLCD, m_pLCDBuf );
	else
		m_pCaret->Check( m_pLCD, m_pLCDBuf );	// 检查脱字符

	// 空闲处理
	OnIdle();
}

// 显示开机画面
void CTApp::ShowStart()
{
  DebugPrintf("ShowStart\n");
}

// 空闲处理
void CTApp::OnIdle()
{
}

// 蜂鸣器鸣叫指示检测到按键按键消息
void CTApp::Beep()
{
}

// 获取键盘事件，插入消息队列；
// 直接将键盘键码作为参数插入消息队列；
// 如果该键值是按键按下的值(值小于128)
// 则调用IAL层的getch函数实现。iMsg = TMSG_KEYDOWN；WPARAM = 键值
#if defined(RUN_ENVIRONMENT_WIN32)
BOOL CTApp::GetKeyboardEvent ()
{
	return FALSE;
}
#endif // defined(RUN_ENVIRONMENT_WIN32)

#if defined(RUN_ENVIRONMENT_LINUX)
unsigned char CTApp::getch()
{
  return 0;
}

BOOL CTApp::GetKeyboardEvent ()
{
	unsigned char ch = 0;
	unsigned char cKeyValue = 0;

       	while( ch = getch() )	// 取键值
	{
		if( ch < 128)
			cKeyValue = ch;
		if( ch == 0 )
			break;
	}

	if( cKeyValue == 0 )
		return TRUE;

	if( m_pClock->IsEnable() )
	{
		m_pClock->Enable( FALSE );
	}
	else if( cKeyValue == TVK_CAPS_LOCK )
	{
		// 翻转Caps的状态
		if( m_pSysBar->GetCaps() )
			m_pSysBar->SetCaps( FALSE );
		else
			m_pSysBar->SetCaps( TRUE );

		// 重绘窗口
		PaintWindows( m_pMainWnd );
	}
	else
	{
		if( m_bClockKeyEnable )
		{
			if( cKeyValue == TVK_CLOCK )
			{
				if( m_pClock->IsEnable() )
					m_pClock->Enable( FALSE );
				else
					m_pClock->Enable( TRUE );
			}
			else
			{
				PostKeyMessage( cKeyValue );
			}
		}
		else
		{
			PostKeyMessage( cKeyValue );
		}
	}

	Beep();
	return TRUE;
}
#endif // defined(RUN_ENVIRONMENT_LINUX)

void CTApp::PostKeyMessage( unsigned char cKeyValue )
{
	// 获得特殊功能键的状态
	int nKeyMask = 0x00000000;
	if( m_pSysBar->GetCaps() )
		nKeyMask |= CAPSLOCK_MASK;

	_TMSG msg;
	msg.pWnd    = m_pMainWnd;
	msg.message = TMSG_KEYDOWN;
	msg.wParam  = cKeyValue;
	msg.lParam  = nKeyMask;
	m_pMsgQ->PostMessage (&msg);
}

// 清除按键缓冲区残留的内容，用于比较耗时的操作结束时
#if defined(RUN_ENVIRONMENT_WIN32)
BOOL CTApp::CleanKeyBuffer()
{
	return TRUE;
}
#endif // defined(RUN_ENVIRONMENT_WIN32)

#if defined(RUN_ENVIRONMENT_LINUX)
BOOL CTApp::CleanKeyBuffer()
{
	int i;
	for( i=0; i<1000; i++ )
	{
		if( getch() == 0 )
			return TRUE;
	}
	return FALSE;
}
#endif // defined(RUN_ENVIRONMENT_LINUX)

// 发送消息
// 区分出TMSG_PAINT消息单独处理
int CTApp::DespatchMessage( _TMSG* pMsg )
{
#if defined(MOUSE_SUPPORT)
	// 区分出鼠标消息单独处理
	if ((pMsg->message == TMSG_LBUTTONDOWN) ||
	    (pMsg->message == TMSG_LBUTTONUP)   ||
	    (pMsg->message == TMSG_MOUSEMOVE))
	{
		if( pMsg->message == TMSG_LBUTTONDOWN )
		{
			// 首先处理点击时钟按钮的消息
			if( m_pClock->PtProc( pMsg->wParam, pMsg->lParam ) )
			{
				PaintWindows( m_pMainWnd );
				return 0;
			}

			// 鼠标点击SystemBar上大小写转换按钮的处理
			if( m_pSysBar->PtProc( pMsg->wParam, pMsg->lParam ) )
			{
				return 0;
			}
		}

#if defined(CHINESE_SUPPORT)
		// 如果输入法窗口处于打开状态，则首先传递给输入法窗口处理
		if (IsIMEOpen())
		{
			if (m_pIMEWnd->PtInWindow (pMsg->wParam, pMsg->lParam))
			{
				m_pIMEWnd->PtProc( pMsg->pWnd, pMsg->message, pMsg->wParam, pMsg->lParam );
				return 0;
			}
		}
#endif // defined(CHINESE_SUPPORT)

		if( IsWindowValid( pMsg->pWnd ) )
		{
			return pMsg->pWnd->PtProc (pMsg->pWnd, pMsg->message, pMsg->wParam, pMsg->lParam);
		}
	}

	// 区分出TMSG_PAINT消息单独处理
	if ( pMsg->message == TMSG_PAINT )
	{
		PaintWindows( pMsg->pWnd );
		return 0;
	}

	return( SendMessage (pMsg) );
#else	
	if ( pMsg->message == TMSG_PAINT )
	{
	  PaintWindows ( pMsg->pWnd );
	  return 0;
	}

	return ( SendMessage (pMsg) );
#endif // defined(MOUSE_SUPPORT)

}

#if defined(RUN_ENVIRONMENT_WIN32)
// 模仿GetKeyboardEvent函数向TGUI系统的消息队列中插入一个键盘消息
// 该函数仅用于windows下仿真,在Win32下替换掉DespatchMessage函数
void CTApp::InsertMessageToTGUI (MSG* pMSG)
{
	// 只插入键盘消息、鼠标左键按下消息和WM_PAINT消息
	if (pMSG->message == WM_KEYDOWN)
	{
		if( m_pClock->IsEnable() )
		{
			m_pClock->Enable( FALSE );
		}
		else if( pMSG->wParam == TVK_CAPS_LOCK )
		{
			// 翻转Caps的状态
			if( m_pSysBar->GetCaps() )
				m_pSysBar->SetCaps( FALSE );
			else
				m_pSysBar->SetCaps( TRUE );

			// 重绘窗口
			PaintWindows( m_pMainWnd );
		}
		else
		{
			if( m_bClockKeyEnable )
			{
				if( pMSG->wParam == TVK_CLOCK )
				{
					if( m_pClock->IsEnable() )
						m_pClock->Enable( FALSE );
					else
						m_pClock->Enable( TRUE );
				}
				else
				{
					PostKeyMessage( (unsigned char)pMSG->wParam );
				}
			}
			else
			{
				PostKeyMessage( (unsigned char)pMSG->wParam );
			}
		}

		//Beep();
	}

#if defined(MOUSE_SUPPORT)
	if ((pMSG->message == WM_LBUTTONDOWN) ||
		(pMSG->message == WM_LBUTTONUP) ||
		(pMSG->message == WM_MOUSEMOVE))
	{
		// 鼠标点击则关闭Clock显示
		if( pMSG->message == WM_LBUTTONDOWN )
		{
			if( m_pClock->IsEnable() )
			{
				m_pClock->Enable( FALSE );
				return;
			}
		}

		int x = (unsigned short)(pMSG->lParam);
		int y = (unsigned short)(((unsigned int)(pMSG->lParam) >> 16) & 0xFFFF);

		// 根据屏幕的缩放状况进行坐标变换
		x = x / SCREEN_MODE;
		y = y / SCREEN_MODE;

		_TMSG msg;
		msg.pWnd	= m_pMainWnd;
		msg.wParam  = x;
		msg.lParam  = y;

		switch (pMSG->message)
		{
		case WM_LBUTTONDOWN:
			msg.message = TMSG_LBUTTONDOWN;
			break;
		case WM_LBUTTONUP:
			msg.message = TMSG_LBUTTONUP;
			break;
		case WM_MOUSEMOVE:
			msg.message = TMSG_MOUSEMOVE;
			break;
		}

		m_pMsgQ->PostMessage (&msg);
	}
#endif // defined(MOUSE_SUPPORT)

	if( m_pClock->IsEnable() )
		return;

	// Clock关闭状态下允许系统WM_PAINT消息
	if (pMSG->message == WM_PAINT)
	{
		_TMSG msg;
		msg.pWnd	= m_pMainWnd;
		msg.message = TMSG_PAINT;
		msg.wParam  = pMSG->wParam;
		msg.lParam  = pMSG->lParam;
		m_pMsgQ->PostMessage (&msg);
	}
}
#endif // defined(RUN_ENVIRONMENT_WIN32)

// 下面的函数调用成员类的相应函数实现

#if defined(CHINESE_SUPPORT)
// 打开输入法窗口(打开显示，创建联系)
BOOL CTApp::OpenIME (CTWindow* pWnd)
{
	return (m_pIMEWnd->OpenIME(pWnd));
}
#endif // defined(CHINESE_SUPPORT)

#if defined(CHINESE_SUPPORT)
// 关闭输入法窗口(关闭显示，断开联系)
BOOL CTApp::CloseIME (CTWindow* pWnd)
{
	return (m_pIMEWnd->CloseIME(pWnd));
}
#endif // defined(CHINESE_SUPPORT)

// 显示系统信息
BOOL CTApp::ShowHint( int iIcon, char* psInfo )
{
  	int w;
	int h;
	unsigned char* fb = m_pLCD->LCDGetFB (&w, &h);

	// 计算显示文字可能会占用的空间
	int iWidth = 0;
	int iHeight = 0;
	unsigned char psTemp[256];

	// 最多不能超过四行文本
	int i;
	for (i=0; i<4; i++)
	{
		memset (psTemp, 0x0, 256);
		if (GetLine (psInfo, (char *)psTemp, i))
		{
			int iDesplayLength = GetDisplayLength ((char *)psTemp, 255);
			if (iDesplayLength > iWidth)
				iWidth = iDesplayLength;

			iHeight += 14;
		}
	}

	if (iWidth > (SCREEN_W - 40))
		iWidth = SCREEN_W - 40;

	int iTextLeft = 10;
	int tw = iWidth + 24;
	int th = iHeight + 42;

	// 留出图标的位置
	if (iIcon > 0)
	{
		tw += 30;
		iTextLeft += 30;
	}
	if (tw < 80)
		tw = 80;

	int tx = (w - tw) / 2;
	int ty = (h - th) / 2;

	// 清空背景区域并绘制边框
	m_pLCD->LCDFillRect (fb, w, h, tx, ty, tw, th, 0);
	m_pLCD->LCDHLine (fb, w, h, tx, ty, tw, 1);
	m_pLCD->LCDHLine (fb, w, h, tx, ty+th-1, tw, 1);
	m_pLCD->LCDVLine (fb, w, h, tx, ty, th, 1);
	m_pLCD->LCDVLine (fb, w, h, tx+tw-1, ty, th, 1);

	// 绘制图标
	unsigned char* pIcon = NULL;
	switch (iIcon)
	{
	case TB_ERROR:
		pIcon = pBitmap_Icon_Error;
		break;
	case TB_EXCLAMATION:
		pIcon = pBitmap_Icon_Exclamation;
		break;
	case TB_QUESTION:
		pIcon = pBitmap_Icon_Question;
		break;
	case TB_INFORMATION:
		pIcon = pBitmap_Icon_Information;
		break;
	case TB_BUSY:
		pIcon = pBitmap_Icon_Busy;
		break;
	case TB_PRINT:
		pIcon = pBitmap_Icon_Printer;
		break;
	}
	m_pLCD->LCDDrawImage(fb,w,h,tx+10,ty+18,23,23,0,pIcon,23,23,0,0);

	// 拆分并绘制文本
	// 最多不能超过四行文本
	for (i=0; i<4; i++)
	{
		memset (psTemp, 0x0, 256);
		if (GetLine (psInfo, (char *)psTemp, i))
			m_pLCD->LCDTextOut( fb,w,h,tx+iTextLeft,ty+21+i*14,LCD_MODE_NORMAL,psTemp,255);
	}

	m_pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );

	return TRUE;
}

// 关闭系统提示
BOOL CTApp::CloseHint()
{
  int w;
  int h;
  unsigned char* fb = m_pLCDBuf->LCDGetFB( &w,&h );
  unsigned char* realfb = m_pLCD->LCDGetFB( &w,&h );
  m_pLCD->LCDCopy (realfb,fb,w,h,0);
  CleanKeyBuffer();
  return TRUE;
}

// 直接调用消息处理函数；
int CTApp::SendMessage (_TMSG* pMsg)
{
	if( IsWindowValid( pMsg->pWnd ) )
	{
	     return pMsg->pWnd->Proc (pMsg->pWnd, pMsg->message, pMsg->wParam, pMsg->lParam);
	}
	else
	     printf("SendMessage:Invalid Window !\n");

	return 0;
}

// 向消息队列中添加一条消息；
// 如果消息队列满（消息数量达到了MESSAGE_MAX 所定义的数目），则返回失败；
BOOL CTApp::PostMessage (_TMSG* pMsg)
{
	return m_pMsgQ->PostMessage( pMsg );
}

// 向消息队列中添加一条退出消息；
BOOL CTApp::PostQuitMessage()
{
	_TMSG msg;
	msg.pWnd = NULL;
	msg.message = TMSG_QUIT;
	msg.wParam = 0;
	msg.lParam = 0;
	return m_pMsgQ->PostMessage( &msg );
}

// 在消息队列中查找指定类型的消息；
// 如果发现消息队列中有指定类型的消息，则返回TRUE；
// 该函数主要用在定时器处理上。CheckTimer函数首先检查消息队列中有没有相同的定时器消息，如果没有，再插入新的定时器消息
BOOL CTApp::FindMessage (_TMSG* pMsg)
{
	return m_pMsgQ->FindMessage( pMsg );
}

// 根据窗口的脱字符信息设置系统脱字符的参数；
BOOL CTApp::SetCaret (_CARET* pCaret)
{
	return m_pCaret->SetCaret( pCaret );
}

// 添加一个定时器；
// 如果当前定时器的数量已经达到TIMER_MAX所定义的数目，则返回FALSE；
// 如果发现一个ID与当前定时器相同的定时器，则直接修改该定时器的设定；
BOOL CTApp::SetTimer (CTWindow* pWindow, int iTimerID, int interval)
{
	return m_pTimerQ->SetTimer (pWindow, iTimerID, interval);
}

// 删除一个定时器；
// 根据TimerID删除
BOOL CTApp::KillTimer (int iTimerID)
{
	return m_pTimerQ->KillTimer (iTimerID);
}

#if defined(CHINESE_SUPPORT)
// 看输入法窗口是否处于打开状态
BOOL CTApp::IsIMEOpen ()
{
	return m_pIMEWnd->IsIMEOpen ();
}

// 设置手写输入的相关参数
BOOL CTApp::EnableGraffiti( int nSerialPort )
{
	return m_pIMEWnd->EnableGraffiti( nSerialPort );
}
#endif // defined(CHINESE_SUPPORT)

#if defined(RESMGT_SUPPORT)
struct _DLGTEMPLET* CTApp::GetDlgTemplet( int nID )
{
	return m_pDtmMgt->GetDlgTemplet( nID );
}

unsigned char* CTApp::GetImage( int nID )
{
	return m_pImgMgt->GetImage( nID );
}
#endif // defined(RESMGT_SUPPORT)

// 设置TopMost窗口
BOOL CTApp::SetTopMost( CTWindow* pWindow )
{
	if( IsWindowValid(pWindow) )
	{
		m_pTopMostWnd = pWindow;
		return TRUE;
	}
	else
	{
		m_pTopMostWnd = NULL;
		return FALSE;
	}
}

// 检验一个窗口指针是否有效
BOOL CTApp::IsWindowValid( CTWindow* pWindow )
{
	if( pWindow != NULL )
	{
		WORD wType = pWindow->m_wWndType;
		if( (wType > TWND_VALID_BEGIN) && (wType < TWND_VALID_END) )	// 判断TopMost窗口的有效性
		{
			return TRUE;
		}
	}
	return FALSE;
}

// 使能/禁止显示时钟
// 注：显示时钟使能时，按“当前时间”键打开时钟窗口。
// “当前时间”按键消息将不能发送给任何一个窗口。
// 机器自检时，需要禁止始终，以保证自检窗口可以接收到“当前时间”的按键消息
BOOL CTApp::ClockKeyEnable( bool bEnable )
{
	// 如果禁止显示时钟，则首先关闭时钟窗口
	if( !bEnable )
	{
		if( m_pClock->IsEnable() )
			m_pClock->Enable( FALSE );
	}
	BOOL bTemp = m_bClockKeyEnable;
	m_bClockKeyEnable = bEnable;
	return bTemp;
}

// 向GUI系统传递按键消息
BOOL CTApp::KeyMessage( int nKeyValue )
{
	if( m_pClock->IsEnable() )
	{
		m_pClock->Enable( FALSE );
	}
	else if( nKeyValue == TVK_CAPS_LOCK )
	{
		// 翻转Caps的状态
		if( m_pSysBar->GetCaps() )
			m_pSysBar->SetCaps( FALSE );
		else
			m_pSysBar->SetCaps( TRUE );

		// 重绘窗口
		PaintWindows( m_pMainWnd );
	}
	else
	{
		if( m_bClockKeyEnable )
		{
			if( nKeyValue == TVK_CLOCK )
			{
				if( m_pClock->IsEnable() )
					m_pClock->Enable( FALSE );
				else
					m_pClock->Enable( TRUE );
			}
			else
			{
				// 只保留nKeyValue的最后一个字节
				PostKeyMessage( (unsigned char)nKeyValue );
			}
		}
		else
		{
			// 只保留nKeyValue的最后一个字节
			PostKeyMessage( (unsigned char)nKeyValue );
		}
	}
}

#if defined(MOUSE_SUPPORT)
// 向GUI系统传递鼠标消息
BOOL CTApp::MouseMessage( int nType, int x, int y )
{
	if( (nType!=TMSG_LBUTTONDOWN) &&
	    (nType!=TMSG_LBUTTONUP  ) &&
	    (nType!=TMSG_MOUSEMOVE  ) )
		return FALSE;

	_TMSG msg;
	msg.pWnd    = m_pMainWnd;
	msg.message = nType;
	msg.wParam  = x;
	msg.lParam  = y;

	return m_pMsgQ->PostMessage (&msg);
}

// 设置时钟按钮的位置
BOOL CTApp::SetClockButton( int x, int y )
{
	return m_pClock->SetClockButton( x, y );
}
#endif // defined(MOUSE_SUPPORT)
/* END */
