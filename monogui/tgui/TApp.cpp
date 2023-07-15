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

// ���ƴ���
void CTApp::PaintWindows( CTWindow* pWnd )
{
	// ����buf
	int w;
	int h;
	unsigned char* fb;
	fb = m_pLCDBuf->LCDGetFB( &w,&h );
	m_pLCDBuf->LCDCopy( fb,fb,w,h,2 );

	// ���������ڵĻ��ƺ���
	pWnd->Paint( m_pLCDBuf );

#if defined(CHINESE_SUPPORT)

	// �������뷨���ڵĻ��ƺ���
	m_pIMEWnd->Paint (m_pLCDBuf);
#endif // defined(CHINESE_SUPPORT)

	// ����TopMost����
	if( IsWindowValid( m_pTopMostWnd ) )
		m_pTopMostWnd->Paint( m_pLCDBuf );

	// ����ϵͳ״̬��
	m_pSysBar->Show( m_pLCDBuf );

#if defined(MOUSE_SUPPORT)
	// ����ʱ�Ӱ�ť
	m_pClock->ShowClockButton( m_pLCDBuf );
#endif // defined(MOUSE_SUPPORT)

	// �����ƺõ�buf������������Ļ
	unsigned char* realfb = m_pLCD->LCDGetFB(&w,&h);
	m_pLCD->LCDCopy (realfb,fb,w,h,0);

	// �������ַ�
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

	// �����������밴��ֵ���ձ�
	if( !(m_pKeyMap->Load( KEY_MAP_FILE )) )
		exit(1);

	// ��ʾ��ʼ����
	ShowStart ();

	m_pMainWnd->UpdateView( m_pMainWnd );
	m_pMainWnd->OnInit();

#if defined(CHINESE_SUPPORT)
	// �������뷨����
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

	// runѭ���������˳�����
	return TRUE;
}

// ר���ڷ��滷��
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
			// �˳�ģ��������
			exit(1);
		}
	}

	Idle();
}

// ��Ϣ���п�״̬�µĴ���
void CTApp::Idle()
{
	GetKeyboardEvent();				// �������¼�ת��Ϊ��Ϣ

	m_pTimerQ->CheckTimer(this);		// ��鶨ʱ�����У��е�ʱ�����Ͷ�ʱ����Ϣ
	// ���CLOCK���ڴ��ڴ�״̬���򲻸������ַ�
	if( m_pClock->IsEnable() )
		m_pClock->Check( m_pLCD, m_pLCDBuf );
	else
		m_pCaret->Check( m_pLCD, m_pLCDBuf );	// ������ַ�

	// ���д���
	OnIdle();
}

// ��ʾ��������
void CTApp::ShowStart()
{
  DebugPrintf("ShowStart\n");
}

// ���д���
void CTApp::OnIdle()
{
}

// ����������ָʾ��⵽����������Ϣ
void CTApp::Beep()
{
}

// ��ȡ�����¼���������Ϣ���У�
// ֱ�ӽ����̼�����Ϊ����������Ϣ���У�
// ����ü�ֵ�ǰ������µ�ֵ(ֵС��128)
// �����IAL���getch����ʵ�֡�iMsg = TMSG_KEYDOWN��WPARAM = ��ֵ
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

       	while( ch = getch() )	// ȡ��ֵ
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
		// ��תCaps��״̬
		if( m_pSysBar->GetCaps() )
			m_pSysBar->SetCaps( FALSE );
		else
			m_pSysBar->SetCaps( TRUE );

		// �ػ洰��
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
	// ������⹦�ܼ���״̬
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

// ����������������������ݣ����ڱȽϺ�ʱ�Ĳ�������ʱ
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

// ������Ϣ
// ���ֳ�TMSG_PAINT��Ϣ��������
int CTApp::DespatchMessage( _TMSG* pMsg )
{
#if defined(MOUSE_SUPPORT)
	// ���ֳ������Ϣ��������
	if ((pMsg->message == TMSG_LBUTTONDOWN) ||
	    (pMsg->message == TMSG_LBUTTONUP)   ||
	    (pMsg->message == TMSG_MOUSEMOVE))
	{
		if( pMsg->message == TMSG_LBUTTONDOWN )
		{
			// ���ȴ�����ʱ�Ӱ�ť����Ϣ
			if( m_pClock->PtProc( pMsg->wParam, pMsg->lParam ) )
			{
				PaintWindows( m_pMainWnd );
				return 0;
			}

			// �����SystemBar�ϴ�Сдת����ť�Ĵ���
			if( m_pSysBar->PtProc( pMsg->wParam, pMsg->lParam ) )
			{
				return 0;
			}
		}

#if defined(CHINESE_SUPPORT)
		// ������뷨���ڴ��ڴ�״̬�������ȴ��ݸ����뷨���ڴ���
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

	// ���ֳ�TMSG_PAINT��Ϣ��������
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
// ģ��GetKeyboardEvent������TGUIϵͳ����Ϣ�����в���һ��������Ϣ
// �ú���������windows�·���,��Win32���滻��DespatchMessage����
void CTApp::InsertMessageToTGUI (MSG* pMSG)
{
	// ֻ���������Ϣ��������������Ϣ��WM_PAINT��Ϣ
	if (pMSG->message == WM_KEYDOWN)
	{
		if( m_pClock->IsEnable() )
		{
			m_pClock->Enable( FALSE );
		}
		else if( pMSG->wParam == TVK_CAPS_LOCK )
		{
			// ��תCaps��״̬
			if( m_pSysBar->GetCaps() )
				m_pSysBar->SetCaps( FALSE );
			else
				m_pSysBar->SetCaps( TRUE );

			// �ػ洰��
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
		// �������ر�Clock��ʾ
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

		// ������Ļ������״����������任
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

	// Clock�ر�״̬������ϵͳWM_PAINT��Ϣ
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

// ����ĺ������ó�Ա�����Ӧ����ʵ��

#if defined(CHINESE_SUPPORT)
// �����뷨����(����ʾ��������ϵ)
BOOL CTApp::OpenIME (CTWindow* pWnd)
{
	return (m_pIMEWnd->OpenIME(pWnd));
}
#endif // defined(CHINESE_SUPPORT)

#if defined(CHINESE_SUPPORT)
// �ر����뷨����(�ر���ʾ���Ͽ���ϵ)
BOOL CTApp::CloseIME (CTWindow* pWnd)
{
	return (m_pIMEWnd->CloseIME(pWnd));
}
#endif // defined(CHINESE_SUPPORT)

// ��ʾϵͳ��Ϣ
BOOL CTApp::ShowHint( int iIcon, char* psInfo )
{
  	int w;
	int h;
	unsigned char* fb = m_pLCD->LCDGetFB (&w, &h);

	// ������ʾ���ֿ��ܻ�ռ�õĿռ�
	int iWidth = 0;
	int iHeight = 0;
	unsigned char psTemp[256];

	// ��಻�ܳ��������ı�
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

	// ����ͼ���λ��
	if (iIcon > 0)
	{
		tw += 30;
		iTextLeft += 30;
	}
	if (tw < 80)
		tw = 80;

	int tx = (w - tw) / 2;
	int ty = (h - th) / 2;

	// ��ձ������򲢻��Ʊ߿�
	m_pLCD->LCDFillRect (fb, w, h, tx, ty, tw, th, 0);
	m_pLCD->LCDHLine (fb, w, h, tx, ty, tw, 1);
	m_pLCD->LCDHLine (fb, w, h, tx, ty+th-1, tw, 1);
	m_pLCD->LCDVLine (fb, w, h, tx, ty, th, 1);
	m_pLCD->LCDVLine (fb, w, h, tx+tw-1, ty, th, 1);

	// ����ͼ��
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

	// ��ֲ������ı�
	// ��಻�ܳ��������ı�
	for (i=0; i<4; i++)
	{
		memset (psTemp, 0x0, 256);
		if (GetLine (psInfo, (char *)psTemp, i))
			m_pLCD->LCDTextOut( fb,w,h,tx+iTextLeft,ty+21+i*14,LCD_MODE_NORMAL,psTemp,255);
	}

	m_pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );

	return TRUE;
}

// �ر�ϵͳ��ʾ
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

// ֱ�ӵ�����Ϣ��������
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

// ����Ϣ���������һ����Ϣ��
// �����Ϣ����������Ϣ�����ﵽ��MESSAGE_MAX ���������Ŀ�����򷵻�ʧ�ܣ�
BOOL CTApp::PostMessage (_TMSG* pMsg)
{
	return m_pMsgQ->PostMessage( pMsg );
}

// ����Ϣ���������һ���˳���Ϣ��
BOOL CTApp::PostQuitMessage()
{
	_TMSG msg;
	msg.pWnd = NULL;
	msg.message = TMSG_QUIT;
	msg.wParam = 0;
	msg.lParam = 0;
	return m_pMsgQ->PostMessage( &msg );
}

// ����Ϣ�����в���ָ�����͵���Ϣ��
// ���������Ϣ��������ָ�����͵���Ϣ���򷵻�TRUE��
// �ú�����Ҫ���ڶ�ʱ�������ϡ�CheckTimer�������ȼ����Ϣ��������û����ͬ�Ķ�ʱ����Ϣ�����û�У��ٲ����µĶ�ʱ����Ϣ
BOOL CTApp::FindMessage (_TMSG* pMsg)
{
	return m_pMsgQ->FindMessage( pMsg );
}

// ���ݴ��ڵ����ַ���Ϣ����ϵͳ���ַ��Ĳ�����
BOOL CTApp::SetCaret (_CARET* pCaret)
{
	return m_pCaret->SetCaret( pCaret );
}

// ���һ����ʱ����
// �����ǰ��ʱ���������Ѿ��ﵽTIMER_MAX���������Ŀ���򷵻�FALSE��
// �������һ��ID�뵱ǰ��ʱ����ͬ�Ķ�ʱ������ֱ���޸ĸö�ʱ�����趨��
BOOL CTApp::SetTimer (CTWindow* pWindow, int iTimerID, int interval)
{
	return m_pTimerQ->SetTimer (pWindow, iTimerID, interval);
}

// ɾ��һ����ʱ����
// ����TimerIDɾ��
BOOL CTApp::KillTimer (int iTimerID)
{
	return m_pTimerQ->KillTimer (iTimerID);
}

#if defined(CHINESE_SUPPORT)
// �����뷨�����Ƿ��ڴ�״̬
BOOL CTApp::IsIMEOpen ()
{
	return m_pIMEWnd->IsIMEOpen ();
}

// ������д�������ز���
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

// ����TopMost����
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

// ����һ������ָ���Ƿ���Ч
BOOL CTApp::IsWindowValid( CTWindow* pWindow )
{
	if( pWindow != NULL )
	{
		WORD wType = pWindow->m_wWndType;
		if( (wType > TWND_VALID_BEGIN) && (wType < TWND_VALID_END) )	// �ж�TopMost���ڵ���Ч��
		{
			return TRUE;
		}
	}
	return FALSE;
}

// ʹ��/��ֹ��ʾʱ��
// ע����ʾʱ��ʹ��ʱ��������ǰʱ�䡱����ʱ�Ӵ��ڡ�
// ����ǰʱ�䡱������Ϣ�����ܷ��͸��κ�һ�����ڡ�
// �����Լ�ʱ����Ҫ��ֹʼ�գ��Ա�֤�Լ촰�ڿ��Խ��յ�����ǰʱ�䡱�İ�����Ϣ
BOOL CTApp::ClockKeyEnable( bool bEnable )
{
	// �����ֹ��ʾʱ�ӣ������ȹر�ʱ�Ӵ���
	if( !bEnable )
	{
		if( m_pClock->IsEnable() )
			m_pClock->Enable( FALSE );
	}
	BOOL bTemp = m_bClockKeyEnable;
	m_bClockKeyEnable = bEnable;
	return bTemp;
}

// ��GUIϵͳ���ݰ�����Ϣ
BOOL CTApp::KeyMessage( int nKeyValue )
{
	if( m_pClock->IsEnable() )
	{
		m_pClock->Enable( FALSE );
	}
	else if( nKeyValue == TVK_CAPS_LOCK )
	{
		// ��תCaps��״̬
		if( m_pSysBar->GetCaps() )
			m_pSysBar->SetCaps( FALSE );
		else
			m_pSysBar->SetCaps( TRUE );

		// �ػ洰��
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
				// ֻ����nKeyValue�����һ���ֽ�
				PostKeyMessage( (unsigned char)nKeyValue );
			}
		}
		else
		{
			// ֻ����nKeyValue�����һ���ֽ�
			PostKeyMessage( (unsigned char)nKeyValue );
		}
	}
}

#if defined(MOUSE_SUPPORT)
// ��GUIϵͳ���������Ϣ
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

// ����ʱ�Ӱ�ť��λ��
BOOL CTApp::SetClockButton( int x, int y )
{
	return m_pClock->SetClockButton( x, y );
}
#endif // defined(MOUSE_SUPPORT)
/* END */
