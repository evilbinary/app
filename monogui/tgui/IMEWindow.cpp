// IMEWindow.cpp: implementation of the CIMEWindow class.
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"

//////////////////////////////////////////////////////////////////////
// Ϊ��д���뷨ʹ�õģ��Ӷ˿ڶ������ݵ��߳�
//////////////////////////////////////////////////////////////////////
void* GraffitiPortProc( void* pWindow )
{
  CIMEWindow* pWnd = (CIMEWindow *)pWindow;

  // �򿪶�Ӧ���ڣ�������Ϊ9600�����ʣ�8λ����λ��1λֹͣλ����Ӳ�����ء�
  int com_fd = 0;

  // ���ݶ˿����ô���Ӧ����(ָ����������ʽ)
  if( pWnd->m_iGraffitiPort == 1 )
    com_fd = open( "/dev/ttyS0", O_RDWR|O_NONBLOCK );
  else if( pWnd->m_iGraffitiPort == 2 )
    com_fd = open( "/dev/ttyS1", O_RDWR|O_NONBLOCK );

  // ������ڴ�ʧ�ܣ���֪ͨ���뷨������ʾ�����Ϣ
  if( com_fd <= 0 )
    {
      pWnd->m_bGraffitiPortStatus = FALSE;
      printf("Debug: Graffiti Port open failure\n");
      pthread_exit( NULL );
    }

  // ��ȡ��ǰ��������
  struct termios tty;
  tcgetattr( com_fd, &tty );

  // ����ǰ������Ϊ�Ǽӹ�ģʽ
  tty.c_iflag &= ~(IGNBRK | IGNCR | INLCR | ICRNL | IUCLC | 
                   IXANY | IXON | IXOFF | INPCK | ISTRIP);
  tty.c_iflag |= (BRKINT | IGNPAR);
  tty.c_oflag &= ~OPOST;
  tty.c_lflag &=~(XCASE | ECHONL | NOFLSH);
  tty.c_lflag &=~( ICANON | ISIG | ECHO | ECHOE );
  tty.c_cflag |= CREAD;
  tty.c_cflag &= ~CRTSCTS; //����Ӳ��������
  tty.c_cc[VTIME] = 0;
  tty.c_cc[VMIN]  = 0;

  tcflush( com_fd, TCIOFLUSH );

  cfsetispeed( &tty, B9600 );
  cfsetospeed( &tty, B9600 );

  // 8λ����λ
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;

  // ����żУ��λ
  tty.c_cflag &= ~PARENB;
  tty.c_iflag &= ~INPCK;

  // 1λֹͣλ
  tty.c_cflag &= ~CSTOPB;

  // ���ô���
  tcsetattr( com_fd, TCSANOW ,&tty );

  pWnd->m_bGraffitiPortStatus = TRUE;

  // ��ʼ�����ڵ�ѭ��
  while( pWnd->m_bGraffitiProcRun )
    {
      // ����������߳�mutex�Ѿ�����,��������������������
      pthread_mutex_lock( &(pWnd->m_mutexReadCom) );
      pthread_mutex_unlock( &(pWnd->m_mutexReadCom) );

      // �����������������ݵı�־,����һ��ѭ�����մ��ڻ�����
      if( pWnd->m_bCleanReadCom )
	{
	  char psTemp[256];
	  while( read( com_fd, psTemp, 255 ) );
	  pWnd->m_bCleanReadCom = FALSE;
	}
      
      // �Ӵ��ڶ�����
      char psBuffer[256];
      int nReceiveLen = read( com_fd, psBuffer, 255 );
      psBuffer[ nReceiveLen ] = 0;
      
      if( nReceiveLen > 0 )
	printf( "%s\n", psBuffer );
    }

  // �رմ����豸
  close( com_fd );

  // �˳��߳�
  pWnd->m_bGraffitiPortStatus = FALSE;
  printf("Debug: Graffiti Thread Exit!\n");
  pthread_exit( NULL );
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CIMEWindow::CIMEWindow ()
{
	m_iCurIME = 0;				// �������뷨
	m_iCurInputLine	= IME_CURRENT_UPPER;	// Ĭ������
	m_iGraffitiPort = 0;
	m_bGraffitiPortStatus = TRUE;
	m_bGraffitiProcRun = FALSE;
	m_bCleanReadCom = TRUE;
	m_pTargetWnd = NULL;
	m_pIMEDB = NULL;
	m_iSyLength = 0;
	m_iSyValidCount = 0;
	m_iSyCount = 0;
	m_iCurSy = 0;
	m_iHzCount = 0;
	m_iCurPage = 0;
	m_iOutStrLength = 0;
	memset (m_cUpperLine, 0x0, 33);
	memset (m_cLowerLine, 0x0, 37);
}

CIMEWindow::~CIMEWindow ()
{
	m_pTargetWnd = NULL;
	pthread_mutex_destroy( &m_mutexReadCom );
}

// �������뷨����
BOOL CIMEWindow::Create(CTApp* pApp)
{
	if (pApp == NULL)
		return FALSE;

	m_pApp = pApp;
	m_wWndType = TWND_TYPE_IME;

	m_x = 0;
	m_y = 93;
	m_w = 240;
	m_h = 35;
	m_iCurIME = 0;

	m_pIMEDB = new CIMEDataBase ();
	return m_pIMEDB->Init ();

	// ��ʼ�����ڿ��ƶ������߳���ͣ��mutex
	pthread_mutex_init( &m_mutexReadCom, NULL );

	// �������߳�mutex����
	pthread_mutex_lock( &m_mutexReadCom );
}

// �������뷨����
void CIMEWindow::Paint (CLCD* pLCD)
{
	// ������ɼ�����ʲôҲ������
	if( !IsWindowVisible() )
		return;

	if (m_iCurIME == 0)
		return;

	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// ���Ʊ���
	pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 0);
//	pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 2);	// ����ɫ���ӵ�Ч��
//	pLCD->LCDFillRect (fb, w, h, m_x+7, m_y+2, m_w-10, m_h-4, 0);

	// �������д����״̬������������ʾ��ʾ����
	if( m_iCurIME == 6 )
	{
		if( m_bGraffitiPortStatus )
			pLCD->LCDTextOut (fb, w, h, m_x+9, m_y+4, 
					  LCD_MODE_NORMAL, (unsigned char *)"                  ", 255);
		else
			pLCD->LCDTextOut (fb, w, h, m_x+9, m_y+4, 
					  LCD_MODE_NORMAL, (unsigned char *)"����д��ͨѶʧ�ܣ�", 255);
	}
	else
	{
		// �������е�����(���Ի���32��Ӣ����ĸ)
		pLCD->LCDTextOut (fb, w, h, m_x+9, m_y+2, LCD_MODE_NORMAL, m_cUpperLine, 255);
	}

	// �����Ӣ�����뷨����û�г�ʱ�������һ����ĸ������ʾ��
	// �����ʱ�ˣ������һ����ĸ�ĺ������һ��С����
	if ((m_iCurIME == 3) || (m_iCurIME == 4))
	{
		if (m_iTimeOut == 0)
		{
			// δ��ʱ�����һ����ĸ������ʾ
			if (m_iOutStrLength > 0)
			{
				int iLength = GetDisplayLength ((char*)m_cUpperLine, m_iOutStrLength - 1);
				unsigned char cc = m_cUpperLine[m_iOutStrLength - 1];
				pLCD->LCDTextOut (fb, w, h, m_x+9+iLength, m_y+2, LCD_MODE_INVERSE, &cc, 1);
			}
		}
		else
		{
			// ��ʱ������������������������ʾһ��С����
			if (m_iOutStrLength > 0)
			{
				int iLength = GetDisplayLength ((char*)m_cUpperLine, m_iOutStrLength);
				pLCD->LCDVLine (fb, w, h, m_x+9+iLength, m_y+2, 14, 1);
			}
		}
	}
	// �������е�����
	pLCD->LCDTextOut (fb, w, h, m_x+9, m_y+19, LCD_MODE_NORMAL, m_cLowerLine, 255);

	// �������
	pLCD->LCDHLine (fb, w, h, m_x, m_y-1, m_w, 0);
	pLCD->LCDHLine (fb, w, h, m_x, m_y+m_h, m_w, 0);
	pLCD->LCDHLine (fb, w, h, m_x, m_y, m_w, 1);
	pLCD->LCDHLine (fb, w, h, m_x, m_y+m_h-1, m_w, 1);
	pLCD->LCDVLine (fb, w, h, m_x, m_y, m_h, 1);
	pLCD->LCDVLine (fb, w, h, m_x+m_w-1, m_y, m_h, 1);
	pLCD->LCDHLine (fb, w, h, m_x+7, m_y+2, m_w-10, 1);
	pLCD->LCDHLine (fb, w, h, m_x+7, m_y+m_h-3, m_w-10, 1);
	pLCD->LCDVLine (fb, w, h, m_x+7, m_y+2, m_h-4, 1);
	pLCD->LCDVLine (fb, w, h, m_x+m_w-3, m_y+2, m_h-4, 1);
	pLCD->LCDHLine (fb, w, h, m_x+7, m_y+17, m_w-10, 1);
	pLCD->LCDVLine (fb, w, h, m_x+209, m_y+2, 16, 1);

	// ���Ƶ�ǰ������ָʾ
	int iYPos = 0;
	if (m_iCurInputLine == IME_CURRENT_UPPER)
		iYPos = m_y+6;
	else
		iYPos = m_y+21;

	pLCD->LCDDrawImage (fb, w, h, m_x, iYPos, 7, 7, LCD_MODE_OR,
				pBitmap_Arror_Right, 7, 7, 0, 0);

	// ��������ָʾ��ǰ���뷨
	unsigned char cText[5];
	memset (cText, 0x0, 5);
	memcpy (cText, &cIMEName[(m_iCurIME-1)*4], 4);
	pLCD->LCDTextOut (fb, w, h, m_x+211, m_y+4, LCD_MODE_NORMAL, cText, 4);

	if (!m_bAllSyInvalid)
	{
		// ��������������ѡ��ָʾ��ͷ
		if ((m_iSyLength > 0) && (m_iSyValidCount > 1))
		{
			if (m_iCurSy > 0)	// ��������ļ�ͷ
			{
				pLCD->LCDDrawImage (fb, w, h, m_x+199, m_y+7, 5, 5, LCD_MODE_NORMAL,
					pBitmap_Little_Arror_Left, 5, 5, 0, 0);
			}
			if (m_iCurSy < (m_iSyValidCount-1))	// �������ҵļ�ͷ
			{
				pLCD->LCDDrawImage (fb, w, h, m_x+204, m_y+7, 5, 5, LCD_MODE_OR,
					pBitmap_Little_Arror_Right, 5, 5, 0, 0);
			}
		}

		// ���Ƶ�ǰƴ��ѡ����»���
		int ix = m_x + 9 + (ASC_W + ASC_GAP) * (m_iSyLength + 1) * m_iCurSy;
		int iw = (ASC_W + ASC_GAP) * m_iSyLength;
		pLCD->LCDHLine (fb, w, h, ix, m_y+15, iw, 1);
	}

	// �������������·�ҳָʾ��ͷ
	if (m_iHzCount > 9)
	{
		if (m_iCurPage > 0)		// �������ϼ�ͷ
		{
			pLCD->LCDDrawImage (fb, w, h, m_x+230, m_y+20, 5, 5, LCD_MODE_NORMAL,
				pBitmap_Little_Arror_Up, 5, 5, 0, 0);
		}
		if (m_iCurPage < ((m_iHzCount+8)/9-1))	// �������¼�ͷ
		{
			pLCD->LCDDrawImage (fb, w, h, m_x+230, m_y+25, 5, 5, LCD_MODE_NORMAL,
				pBitmap_Little_Arror_Down, 5, 5, 0, 0);
		}
	}
}

// ��Ϣ��������1
int CIMEWindow::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	if (pWnd == this)
	{
		if ((iMsg == TMSG_KEYDOWN) && (wParam == TVK_F12))
		{
			m_pApp->KillTimer (IME_TIMER_ID);
			m_iTimeOut = 0;

			// �л����뷨
			if (m_iGraffitiPort == 0)
			{
				if (m_iCurIME < 5)
					m_iCurIME ++;
				else
					m_iCurIME = 1;
			}
			else
			{
				if (m_iCurIME < 6)
				{
					m_iCurIME ++;
					if( m_iCurIME == 6 )
					{
						// ����������߳�mutex������
						pthread_mutex_unlock( &m_mutexReadCom );
						m_bCleanReadCom = TRUE;
					}
				}
				else
				{
					m_iCurIME = 1;
					// �������߳�mutex����
					pthread_mutex_lock( &m_mutexReadCom );
				}
			}

			// �������뷨�Ĳ�ͬ���в�ͬ�ĳ�ʼ��
			m_pIMEDB->SyRemoveAll ();
			memset (m_cUpperLine, 0x0, 33);
			memset (m_cLowerLine, 0x0, 37);
			m_iOutStrLength = 0;
			m_iCurInputLine = IME_CURRENT_UPPER;
			m_iSyLength = 0;
			m_iSyValidCount = 0;
			m_iSyCount = 0;
			m_iCurSy = 0;
			m_iHzCount = 0;
			m_iCurPage = 0;
			m_iOutStrLength = 0;

			if (m_iCurIME == 5)
			{
				// �������뷨������ǰ���������������У�����ʼ�����е���ʾ����
				m_iCurInputLine = IME_CURRENT_LOWER;
				m_cHzReturn = (unsigned char*)strPunctuation;
				m_iHzCount = IME_PUNCTUATION_NUM;
				RenewLowerLine ();		// ����m_cHzReturn�͵�ǰ�е���Ϣ��������
			}

			if (m_iCurIME == 6)
			{
				m_iHzCount = 0;
				m_iCurInputLine = IME_CURRENT_LOWER;
			}

			if (m_iCurIME == 1)                     // ��׼ƴ�����뷨
				m_pIMEDB->SetCurIME (1);
			else if (m_iCurIME == 2)                // ������չ���뷨
				m_pIMEDB->SetCurIME (2);
		}
		else
		{
			switch (m_iCurIME)
			{
			case 1:		// ƴ�����뷨
			case 2:
				{
					PinYinProc (pWnd, iMsg, wParam, lParam);
				}
				break;
			case 3:		// Ӣ�����뷨
			case 4:
				{
					YingWenProc (pWnd, iMsg, wParam, lParam);
				}
				break;
			case 5:		// ��������
				{
					PunctuationProc (pWnd, iMsg, wParam, lParam);
				}
				break;
			case 6:		// ��д����
				{
					GraffitiProc (pWnd, iMsg, wParam, lParam);
				}
			}
		}
	}

	return CTWindow::Proc (pWnd, iMsg, wParam, lParam);
//	UpdateView (this);	// �������ˢ�²��ã��⿪�����
}

// �Ƿ���Դ�����ư���(������봰�ǿյģ��򲻴�����ư���������CTEdit����ȥ����)
BOOL CIMEWindow::CanHandleControlKey()
{
	if ((m_iCurIME == 1) && (m_iCurInputLine == IME_CURRENT_UPPER) && (m_iSyValidCount == 0))
		return FALSE;
	if ((m_iCurIME == 2) && (m_iCurInputLine == IME_CURRENT_UPPER) && (m_iSyValidCount == 0))
		return FALSE;
	if ((m_iCurIME == 3) && (m_iOutStrLength == 0))
		return FALSE;
	if ((m_iCurIME == 4) && (m_iOutStrLength == 0))
		return FALSE;
	if ((m_iCurIME == 5) || (m_iCurIME == 6))
		return FALSE;

	return TRUE;
}

// �����뷨����(����ʾ��������ϵ)
BOOL CIMEWindow::OpenIME (CTWindow* pWnd)
{
	if (pWnd == NULL)
		return FALSE;

	m_pTargetWnd = pWnd;

	// ������뷨���ڵ�ס��Ŀ��ؼ�������ʾ��Ŀ��ؼ����Ϸ�
	if ((pWnd->m_y + pWnd->m_h) > 93)
		m_y = pWnd->m_y - 37;
	else
		m_y = 93;

	// �����д�򿪣���Ĭ��ʹ����д���뷨
	if( m_iGraffitiPort > 0 )
	{
		m_iCurIME = 6;

		// �����������߳�
		m_bGraffitiProcRun = TRUE;
		int ret = pthread_create( &m_threadReadCom, NULL, GraffitiPortProc, (void*)this );
		if( ret != 0 )
			m_bGraffitiPortStatus = FALSE;

		// ����������߳�mutex������
		pthread_mutex_unlock( &m_mutexReadCom );
	}
	else
	{
		m_iCurIME = 1;	// Ĭ��ƴ�����뷨
		m_pIMEDB->SetCurIME (1);
	}

	return TRUE;
}

// �ر����뷨����(�ر���ʾ���Ͽ���ϵ)
BOOL CIMEWindow::CloseIME (CTWindow* pWnd)
{
	if (pWnd != m_pTargetWnd)
		return FALSE;

	// �����д������Ч����رն���д�崮�ڵ��߳�
	if( m_iGraffitiPort > 0 )
	{
		if( m_bGraffitiPortStatus )
		{
			// ����������߳�mutex������
			pthread_mutex_unlock( &m_mutexReadCom );

			// ʹ������ѭ���˳�
			m_bGraffitiProcRun = FALSE;

			// �ȴ��߳��˳�
			pthread_join( m_threadReadCom, NULL );
		}
	}

	m_iCurIME = 0;	// �ر����뷨����
	m_pIMEDB->SyRemoveAll ();
	m_iCurInputLine = IME_CURRENT_UPPER;
	m_pTargetWnd = NULL;
	m_iSyLength = 0;
	m_iSyValidCount = 0;
	m_iSyCount = 0;
	m_iCurSy = 0;
	m_iHzCount = 0;
	m_iCurPage = 0;
	m_iOutStrLength = 0;
	memset (m_cUpperLine, 0x0, 33);
	memset (m_cLowerLine, 0x0, 37);
	return TRUE;
}

// �����뷨�����Ƿ��ڴ�״̬
BOOL CIMEWindow::IsIMEOpen ()
{
	if (m_iCurIME == 0)
		return FALSE;
	else
		return TRUE;
}

// ������д�������ز���
BOOL CIMEWindow::EnableGraffiti( int nSerialPort )
{
	// ���������뷨���ڴ򿪵�״̬�¸ı�����
	if( m_iCurIME > 0 )
		return FALSE;

	if( (nSerialPort < 0) || (nSerialPort > 2) )
	{
		m_iGraffitiPort = 0;
		return FALSE;
	}

	m_iGraffitiPort = nSerialPort;
	return TRUE;
}

// ����ʱ����Ϣ(ֻ����Ӣ�����뷨)
void CIMEWindow::OnTimer (int iTimerID, int iInterval)
{
	m_pApp->KillTimer (IME_TIMER_ID);
	m_iTimeOut = 1;
	UpdateView(this);
}

// ��Ŀ�괰�ڷ����ַ�(�����Ӣ�ģ���ֻ��c1��c2Ϊ0)
void CIMEWindow::SendChar (unsigned char c1, unsigned char c2)
{
	_TMSG msg;
	msg.pWnd = m_pTargetWnd;
	msg.message = TMSG_CHAR;
	msg.wParam = c1;
	msg.lParam = c2;
	m_pApp->SendMessage (&msg);
}

// �������ּ�1~9ѡ���ַ�������
unsigned char* CIMEWindow::LowerLineGetChar (int iKeyValue, int iLength)
{
	int iSelect = 0;

	switch (iKeyValue)
	{
	case TVK_1:	iSelect=0; break;
	case TVK_2:	iSelect=1; break;
	case TVK_3:	iSelect=2; break;
	case TVK_4:	iSelect=3; break;
	case TVK_5:	iSelect=4; break;
	case TVK_6:	iSelect=5; break;
	case TVK_7:	iSelect=6; break;
	case TVK_8:	iSelect=7; break;
	case TVK_9:	iSelect=8; break;
	default:	return NULL;
	}

	int iOffset = m_iCurPage * 9 + iSelect;
	if (iOffset < m_iHzCount)
	{
		iOffset *= 2;
		if (iLength == 1)
		{
			SendChar (m_cHzReturn[iOffset], 0x0);
		}
		else if (iLength == 2)
		{
			SendChar (m_cHzReturn[iOffset], m_cHzReturn[iOffset+1]);
		}
		return &m_cHzReturn[iOffset];
	}
	return NULL;
}

// ���ֲ�ͬ���뷨�Ĵ�����
// ƴ�����뷨
void CIMEWindow::PinYinProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if (iMsg == TMSG_KEYDOWN)
	{
		switch (wParam)
		{
		case TVK_UP:
			{
				// ����(��������ǰ��ҳ)
				if (m_iCurInputLine == IME_CURRENT_LOWER)
				{
					LowerLinePageUp ();
				}
			}
			break;
		case TVK_DOWN:
			{
				// ����(���������ҳ)
				if (m_iCurInputLine == IME_CURRENT_LOWER)
				{
					LowerLinePageDown ();
				}
			}
			break;
		case TVK_LEFT:
			{
				// ����(������ѡ��ǰ���һ��ƴ��)(��������ǰ��ҳ)
				if (m_iCurInputLine == IME_CURRENT_UPPER)
				{
					if (m_iCurSy > 0)
						m_iCurSy --;
					// �޸�ƴ���ĺ�������
					RenewSy ();
				}
				else if (m_iCurInputLine == IME_CURRENT_LOWER)
				{
					LowerLinePageUp ();
				}
			}
			break;
		case TVK_RIGHT:
			{
				// ����(������ѡ������һ��ƴ��)(���������ҳ)
				if (m_iCurInputLine == IME_CURRENT_UPPER)
				{
					if (m_iCurSy < (m_iSyValidCount - 1))
						m_iCurSy ++;
					// �޸�ƴ���ĺ�������
					RenewSy ();
				}
				else if (m_iCurInputLine == IME_CURRENT_LOWER)
				{
					LowerLinePageDown ();
				}
			}
			break;
		case TVK_BACK_SPACE:
			{
				// ɾ��(������ɾ��һ���ַ�)
				if (m_iCurInputLine == IME_CURRENT_UPPER)
				{
					if (m_iSyLength > 0)
					{
						// ɾ�����һ���ַ�
						m_pIMEDB->SyRemove ();
						// ���»��ƴ������
						m_iSyLength = m_pIMEDB->GetSyLength ();
						// ��ѯƴ�����
						m_iSyCount = m_pIMEDB->GetSy (&m_cSyReturn);
						// �޸�ƴ���ĺ�������
						m_iCurSy = 0;
						RenewSy ();
					}
				}
			}
			break;
		case TVK_SPACE:
			{
				// �ո��
				SendChar (0x20, 0x0);
			}
			break;
		case TVK_PERIOD:
			{
				// ����С����
				SendChar ('.', 0x0);
			}
			break;
		case TVK_ENTER:
			{
				// ȷ�ϼ�(�л�������)
				if (m_iCurInputLine == IME_CURRENT_UPPER)
				{
					// ����
					// ��������ж������򽫵�ǰ��������Ϊ����
					if (m_iHzCount > 0)
					{
						m_iCurInputLine = IME_CURRENT_LOWER;
					}
				}
			}
			break;
		case TVK_ESCAPE:
			{
				// ESCAPE��(�л�������)
				if (m_iCurInputLine == IME_CURRENT_LOWER)
				{
					m_iCurInputLine = IME_CURRENT_UPPER;
				}
			}
			break;
		case TVK_0:
		case TVK_1:
		case TVK_2:
		case TVK_3:
		case TVK_4:
		case TVK_5:
		case TVK_6:
		case TVK_7:
		case TVK_8:
		case TVK_9:
			{
				// ���ּ�(��������ƴ��������ѡ����)
				if (m_iCurInputLine == IME_CURRENT_UPPER)
				{
					// ����
					// ������ת����ASC�ַ�������ƴ������
					// ������ؼ��ǿյģ���ѸղŲ����ɾ��
					// ���Ƚ�����ת�����ַ�
					char cKey = MessageToKey (wParam);
					// ��ƴ��������ַ�
					m_pIMEDB->SyAdd (cKey);
					// ��ѯƴ�����
					m_iSyCount = m_pIMEDB->GetSy (&m_cSyReturn);
					// ���ƴ������ǿյģ���ɾ���ղ�������ַ�
					if (m_iSyCount == 0)
					{
						m_pIMEDB->SyRemove ();
						// ��ѯƴ�����
						m_iSyCount = m_pIMEDB->GetSy (&m_cSyReturn);
					}
					else
					{
						// ���»��ƴ������
						m_iSyLength = m_pIMEDB->GetSyLength ();
						// ����ǰѡ���趨Ϊ��һ��
						m_iCurSy = 0;
						// �޸�ƴ���ĺ�������
						RenewSy ();
					}
				}
				else if (m_iCurInputLine == IME_CURRENT_LOWER)
				{
					// ����
					// ѡ��һ�����֣�Ȼ���������ʿ⣬���ݷ��ؼ���������
					// Ȼ�󽫵�ǰ�����л�������
					unsigned char* cHz = LowerLineGetChar (wParam, 2);
					if (cHz != NULL)
					{
						unsigned char cc [3];
						memset (cc, 0x0, 3);
						memcpy (cc, cHz, 2);
						m_iHzCount = m_pIMEDB->GetLx (cc, &m_cHzReturn);
						m_iCurPage = 0;
						RenewLowerLine ();
						// �������������ǰ�����л�������
						m_pIMEDB->SyRemoveAll ();
						memset (m_cUpperLine, 0x0, 33);
						m_iSyLength = 0;
						m_iSyCount = 0;
						m_iSyValidCount = 0;
						m_iCurSy = 0;
						m_iCurInputLine = IME_CURRENT_UPPER;
					}
				}
			}
			break;
		}
	}
}

// Ӣ�����뷨
void CIMEWindow::YingWenProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if (iMsg == TMSG_KEYDOWN)
	{
		switch (wParam)
		{
		case TVK_SPACE:
			{
				// �ո��
				int i;
				for (i=0; i<m_iOutStrLength; i++)
				{
					char cc = m_cUpperLine[i];
					SendChar (cc, 0x0);
				}
				m_iOutStrLength = 0;
				memset (m_cUpperLine, 0x0, 33);
				// �رճ�ʱ��ʱ��
				m_pApp->KillTimer (IME_TIMER_ID);
				m_iTimeOut = 0;
				// ���һ���ո�
				SendChar (0x20, 0x0);
			}
			break;
		case TVK_DOWN:
		case TVK_RIGHT:
			{
				// ���Һ����£���ǰ��ֹ��ʱ��ʱ��
				m_pApp->KillTimer (IME_TIMER_ID);
				m_iTimeOut = 1;
			}
			break;
		case TVK_BACK_SPACE:
			{
				// ɾ��
				if (m_iOutStrLength > 0)
				{
					m_cUpperLine[m_iOutStrLength - 1] = 0x0;
					m_iOutStrLength --;
				}
				// �رճ�ʱ��ʱ��
				m_pApp->KillTimer (IME_TIMER_ID);
				m_iTimeOut = 1;
			}
			break;
		case TVK_ENTER:
			{
				// ȷ�ϼ�
				int i;
				for (i=0; i<m_iOutStrLength; i++)
				{
					char cc = m_cUpperLine[i];
					SendChar (cc, 0x0);
				}
				m_iOutStrLength = 0;
				memset (m_cUpperLine, 0x0, 33);
				// �رճ�ʱ��ʱ��
				m_pApp->KillTimer (IME_TIMER_ID);
				m_iTimeOut = 0;
			}
			break;
		case TVK_0:
		case TVK_1:
		case TVK_2:
		case TVK_3:
		case TVK_4:
		case TVK_5:
		case TVK_6:
		case TVK_7:
		case TVK_8:
		case TVK_9:
			{
				// ���ּ�
				if (m_iOutStrLength < IME_OUT_STRING_LIMIT)
				{
					// �����ּ�ת����ASC�ַ�
					char cKey = MessageToKey (wParam);

					// ���ȼ�ⳬʱ��־�����
					if (m_iTimeOut == 1)
					{
						// �Ѿ���ʱ���������µ��ַ�
						// ���ݴ�Сд״̬���в�ͬ�Ĵ���
						char cc = KeyToChar (cKey);	// Ĭ��Сд��ĸ
						if (cc != 0x0)
						{
							if (m_iCurIME == 4)	// Ӣ�Ĵ�д
								cc -= 0x20;
							m_cUpperLine[m_iOutStrLength] = cc;
							m_iOutStrLength ++;
						}
					}
					else
					{
						// û�г�ʱ������������ַ���ʲô�ַ�
						// ������ϴ�����Ĳ���ͬһ���ַ����������µ��ַ�
						// ������ϴ��������ͬһ���ַ������޸����һ���ַ�
						// ���ݴ�Сд״̬���в�ͬ�Ĵ���
						char cc = 0x0;
						if (m_iOutStrLength > 0)
							cc = CharToKey (m_cUpperLine[m_iOutStrLength-1]);

						if (cc == cKey)		// ͬһ���������޸����һ���ַ�
						{
							cc = KeyNextChar (m_cUpperLine[m_iOutStrLength-1]);
							if (m_iCurIME == 4)		// Ӣ�Ĵ�д
								cc -= 0x20;
							m_cUpperLine[m_iOutStrLength-1] = cc;
						}
						else				// �����µ���ĸ
						{
							cc = KeyToChar (cKey);	// Ĭ��Сд��ĸ
							if (cc != 0x0)
							{
								if (m_iCurIME == 4)		// Ӣ�Ĵ�д
									cc -= 0x20;
								m_cUpperLine[m_iOutStrLength] = cc;
								m_iOutStrLength ++;
							}
						}
					}
					// ���ó�ʱ��ʱ��
					m_pApp->SetTimer (this, IME_TIMER_ID, IME_KEY_PRESS_GAP);
					m_iTimeOut = 0;
				}
			}
			break;
		}
	}
}

// �������뷨
void CIMEWindow::PunctuationProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if (iMsg == TMSG_KEYDOWN)
	{
		switch (wParam)
		{
		case TVK_UP:
			{
				// ����
				LowerLinePageUp ();
			}
			break;
		case TVK_DOWN:
			{
				// ����
				LowerLinePageDown ();
			}
			break;
		case TVK_0:
		case TVK_1:
		case TVK_2:
		case TVK_3:
		case TVK_4:
		case TVK_5:
		case TVK_6:
		case TVK_7:
		case TVK_8:
		case TVK_9:
			{
				// ���ּ�
				LowerLineGetChar (wParam, 1);
			}
			break;
		case TVK_PERIOD:
			{
				// С����
				SendChar ('.', 0x0);
			}
			break;
		case TVK_SPACE:
			{
				// �ո��
				SendChar (0x20, 0x0);
			}
			break;
		}
	}
}

// ��д���뷨(�ⲿ�豸ͨ���˿����뱸ѡ����)
void CIMEWindow::GraffitiProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if (iMsg == TMSG_KEYDOWN)
	{
		switch (wParam)
		{
		case TVK_UP:
			{
				// ����
				LowerLinePageUp ();
			}
			break;
		case TVK_DOWN:
			{
				// ����
				LowerLinePageDown ();
			}
			break;
		case TVK_0:
		case TVK_1:
		case TVK_2:
		case TVK_3:
		case TVK_4:
		case TVK_5:
		case TVK_6:
		case TVK_7:
		case TVK_8:
		case TVK_9:
			{
				// ���ּ�
				LowerLineGetChar (wParam, 1);
			}
			break;
		case TVK_PERIOD:
			{
				// С����
				SendChar ('.', 0x0);
			}
			break;
		case TVK_SPACE:
			{
				// �ո��
				SendChar (0x20, 0x0);
			}
			break;
		}
	}
}

// �޸�ƴ���Ժ�ĺ�������(����������)
BOOL CIMEWindow::RenewSy ()
{
	// ��������
	RenewUpperLine ();
	// �õ���ǰѡ���ƴ���ַ���
	unsigned char cSymbol[7];
	memset (cSymbol, 0x20, 6);
	cSymbol[6] = 0x0;
	int iCurSyPos = (m_iSyLength + 1) * m_iCurSy;
	memcpy (cSymbol, &m_cUpperLine[iCurSyPos], m_iSyLength);
	// ���ݵ�ǰƴ����ϲ�ѯ����
	m_iHzCount = m_pIMEDB->GetHz (cSymbol, &m_cHzReturn);
	// ��������ǰҳ����Ϊ��һҳ
	m_iCurPage = 0;
	// ��������
	RenewLowerLine ();

	return TRUE;
}

// ����m_cSyReturn��m_iSyLength��m_iSyCount�����������ַ���m_cUpperLine������m_iSyValidCount
void CIMEWindow::RenewUpperLine ()
{
	m_bAllSyInvalid = FALSE;

	memset (m_cUpperLine, 0x0, 33);
	m_iSyValidCount = 0;
	int i;
	for (i=0; i<m_iSyCount; i++)
	{
		int iOffset = i * 8;
		// �������ȫƥ��ģ����������
		if (m_cSyReturn[iOffset + 6] == '1')
		{
			int iTargetOffset = m_iSyValidCount * ( m_iSyLength + 1);
			memcpy (&m_cUpperLine[iTargetOffset], &m_cSyReturn[iOffset], m_iSyLength);
			m_cUpperLine[iTargetOffset+m_iSyLength] = 0x20;		// ƴ������λ�ò�һ���ո�
			m_iSyValidCount ++;
		}
	}

	// ����ܵ�������Ϊ0����Ч����Ϊ0��
	// ˵������ƴ����϶��ǲ���ȫ��ϣ���ʱӦ���г����в���ȫ���
	// ע�⣺Ӧ������go go ho ho����ko ko lo lo��������
	if ((m_iSyCount > 0) && (m_iSyValidCount == 0))
	{
		m_bAllSyInvalid = TRUE;
		char cOldSymbol [7];	// �ñȽϷ�����go go ho ho����
		for (i=0; i<m_iSyCount; i++)
		{
			int iOffset = i * 8;
			int iTargetOffset = m_iSyValidCount * ( m_iSyLength + 1);
			if ((memcmp (cOldSymbol, &m_cSyReturn[iOffset], m_iSyLength)) != 0)
			{
				memcpy (&m_cUpperLine[iTargetOffset], &m_cSyReturn[iOffset], m_iSyLength);
				m_cUpperLine[iTargetOffset+m_iSyLength] = 0x20;		// ƴ������λ�ò�һ���ո�
				m_iSyValidCount ++;
				memcpy (cOldSymbol, &m_cSyReturn[iOffset], 6);
			}
		}
	}
}

// ����m_cHzReturn��m_iHzCount��m_iCurPage�����������ַ���m_cLowerLine
void CIMEWindow::RenewLowerLine ()
{
	memset (m_cLowerLine, 0x0, 37);

	if (m_iHzCount == 0)
		return;

	int iCount = 9;
	// ��������һ�У��򳤶�ȡ����
	if (m_iCurPage == (((m_iHzCount + 8) / 9) - 1))
	{
		iCount = m_iHzCount % 9;
		if (iCount == 0)
			iCount = 9;
	}

	// ����cLowerLine�ַ���������
	int iOffset = m_iCurPage * 18;
	int i;
	for (i=0; i<iCount; i++)
	{
		m_cLowerLine[i*4] = (i+0x31);				// ���(��1��ʼ)
		m_cLowerLine[i*4+1] = m_cHzReturn[iOffset+i*2];		// ��һ���ֽ�
		m_cLowerLine[i*4+2] = m_cHzReturn[iOffset+i*2+1];	// �ڶ����ֽ�
		m_cLowerLine[i*4+3] = 0x20;				// �ո�
	}
}

// ���з�ҳ����ǰ��ҳ����
void CIMEWindow::LowerLinePageUp ()
{
	if (m_iCurPage > 0)
		m_iCurPage --;
	RenewLowerLine ();
}

// ���з�ҳ�����ҳ����
void CIMEWindow::LowerLinePageDown ()
{
	int iPageCount = (m_iHzCount + 8) / 9;
	if (m_iCurPage < (iPageCount - 1))
		m_iCurPage ++;
	RenewLowerLine ();
}

// ������ĸ���Ҷ�Ӧ�����ּ�
char CIMEWindow::CharToKey (char cc)
{
	// ����Ǵ�д��ĸ����ת��ΪСд��ĸ
	if ((cc >= 'A') && (cc <= 'Z'))
		cc += 0x20;

	int i;
	for (i=0; i<4; i++)
	{
		if (Key0[i] == cc)
			return '0';
	}
	for (i=0; i<4; i++)
	{
		if (Key1[i] == cc)
			return '1';
	}
	for (i=0; i<4; i++)
	{
		if (Key2[i] == cc)
			return '2';
	}
	for (i=0; i<4; i++)
	{
		if (Key3[i] == cc)
			return '3';
	}
	for (i=0; i<4; i++)
	{
		if (Key4[i] == cc)
			return '4';
	}
	for (i=0; i<4; i++)
	{
		if (Key5[i] == cc)
			return '5';
	}
	for (i=0; i<4; i++)
	{
		if (Key6[i] == cc)
			return '6';
	}
	for (i=0; i<4; i++)
	{
		if (Key7[i] == cc)
			return '7';
	}
	for (i=0; i<4; i++)
	{
		if (Key8[i] == cc)
			return '8';
	}
	for (i=0; i<4; i++)
	{
		if (Key9[i] == cc)
			return '9';
	}
	return 0x0;
}

// ��������Ϣת���ɰ����ַ�
char CIMEWindow::MessageToKey (int iMessage)
{
	char cKey = 0x0;
	switch (iMessage)
	{
	case TVK_0:	cKey='0'; break;
	case TVK_1:	cKey='1'; break;
	case TVK_2:	cKey='2'; break;
	case TVK_3:	cKey='3'; break;
	case TVK_4:	cKey='4'; break;
	case TVK_5:	cKey='5'; break;
	case TVK_6:	cKey='6'; break;
	case TVK_7:	cKey='7'; break;
	case TVK_8:	cKey='8'; break;
	case TVK_9:	cKey='9'; break;
	}
	return cKey;
}

// �������ּ����Ҷ�Ӧ����ĸ(���ص�һ��)
char CIMEWindow::KeyToChar (char cKey)
{
	switch (cKey)
	{
	case '0':	return Key0[0];
	case '1':	return Key1[0];
	case '2':	return Key2[0];
	case '3':	return Key3[0];
	case '4':	return Key4[0];
	case '5':	return Key5[0];
	case '6':	return Key6[0];
	case '7':	return Key7[0];
	case '8':	return Key8[0];
	case '9':	return Key9[0];
	}
	return 0x0;
}

// ���Ҹ���ĸ�������ּ������һ����ĸ
char CIMEWindow::KeyNextChar (char cc)
{
	// ����Ǵ�д��ĸ����ת��ΪСд��ĸ
	if ((cc >= 'A') && (cc <= 'Z'))
		cc += 0x20;

	char cKey = CharToKey (cc);

	char* pGroup = NULL;
	switch (cKey)
	{
	case '0':	pGroup = (char *)Key0;	break;
	case '1':	pGroup = (char *)Key1;	break;
	case '2':	pGroup = (char *)Key2;	break;
	case '3':	pGroup = (char *)Key3;	break;
	case '4':	pGroup = (char *)Key4;	break;
	case '5':	pGroup = (char *)Key5;	break;
	case '6':	pGroup = (char *)Key6;	break;
	case '7':	pGroup = (char *)Key7;	break;
	case '8':	pGroup = (char *)Key8;	break;
	case '9':	pGroup = (char *)Key9;	break;
	}

	if (pGroup == NULL)
		return 0x0;

	int i;
	for (i=0; i<4; i++)
	{
		char cTemp = pGroup[i];
		if (cTemp == cc)
		{
			if (i == 3)	// �ü�������һ��
			{
				return pGroup[0];
			}
			else if(pGroup[i+1] == '\0')
			{
				return pGroup[0];
			}
			else
			{
				return pGroup[i+1];
			}
		}
	}
	return 0x0;
}
/* END */

