// IMEWindow.cpp: implementation of the CIMEWindow class.
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"

//////////////////////////////////////////////////////////////////////
// 为手写输入法使用的，从端口读入数据的线程
//////////////////////////////////////////////////////////////////////
void* GraffitiPortProc( void* pWindow )
{
  CIMEWindow* pWnd = (CIMEWindow *)pWindow;

  // 打开对应串口，并设置为9600波特率，8位数据位，1位停止位，无硬件流控。
  int com_fd = 0;

  // 根据端口设置打开相应串口(指定非阻塞方式)
  if( pWnd->m_iGraffitiPort == 1 )
    com_fd = open( "/dev/ttyS0", O_RDWR|O_NONBLOCK );
  else if( pWnd->m_iGraffitiPort == 2 )
    com_fd = open( "/dev/ttyS1", O_RDWR|O_NONBLOCK );

  // 如果串口打开失败，则通知输入法窗口显示相关信息
  if( com_fd <= 0 )
    {
      pWnd->m_bGraffitiPortStatus = FALSE;
      printf("Debug: Graffiti Port open failure\n");
      pthread_exit( NULL );
    }

  // 获取当前串口设置
  struct termios tty;
  tcgetattr( com_fd, &tty );

  // 将当前串口设为非加工模式
  tty.c_iflag &= ~(IGNBRK | IGNCR | INLCR | ICRNL | IUCLC | 
                   IXANY | IXON | IXOFF | INPCK | ISTRIP);
  tty.c_iflag |= (BRKINT | IGNPAR);
  tty.c_oflag &= ~OPOST;
  tty.c_lflag &=~(XCASE | ECHONL | NOFLSH);
  tty.c_lflag &=~( ICANON | ISIG | ECHO | ECHOE );
  tty.c_cflag |= CREAD;
  tty.c_cflag &= ~CRTSCTS; //屏蔽硬件流控制
  tty.c_cc[VTIME] = 0;
  tty.c_cc[VMIN]  = 0;

  tcflush( com_fd, TCIOFLUSH );

  cfsetispeed( &tty, B9600 );
  cfsetospeed( &tty, B9600 );

  // 8位数据位
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;

  // 无奇偶校验位
  tty.c_cflag &= ~PARENB;
  tty.c_iflag &= ~INPCK;

  // 1位停止位
  tty.c_cflag &= ~CSTOPB;

  // 设置串口
  tcsetattr( com_fd, TCSANOW ,&tty );

  pWnd->m_bGraffitiPortStatus = TRUE;

  // 开始读串口的循环
  while( pWnd->m_bGraffitiProcRun )
    {
      // 如果读串口线程mutex已经锁定,则锁定操作将导致阻塞
      pthread_mutex_lock( &(pWnd->m_mutexReadCom) );
      pthread_mutex_unlock( &(pWnd->m_mutexReadCom) );

      // 如果发现清除串口数据的标志,则用一个循环读空串口缓冲区
      if( pWnd->m_bCleanReadCom )
	{
	  char psTemp[256];
	  while( read( com_fd, psTemp, 255 ) );
	  pWnd->m_bCleanReadCom = FALSE;
	}
      
      // 从串口读数据
      char psBuffer[256];
      int nReceiveLen = read( com_fd, psBuffer, 255 );
      psBuffer[ nReceiveLen ] = 0;
      
      if( nReceiveLen > 0 )
	printf( "%s\n", psBuffer );
    }

  // 关闭串口设备
  close( com_fd );

  // 退出线程
  pWnd->m_bGraffitiPortStatus = FALSE;
  printf("Debug: Graffiti Thread Exit!\n");
  pthread_exit( NULL );
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CIMEWindow::CIMEWindow ()
{
	m_iCurIME = 0;				// 不打开输入法
	m_iCurInputLine	= IME_CURRENT_UPPER;	// 默认上栏
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

// 创建输入法窗口
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

	// 初始化用于控制读串口线程暂停的mutex
	pthread_mutex_init( &m_mutexReadCom, NULL );

	// 读串口线程mutex锁定
	pthread_mutex_lock( &m_mutexReadCom );
}

// 绘制输入法窗口
void CIMEWindow::Paint (CLCD* pLCD)
{
	// 如果不可见，则什么也不绘制
	if( !IsWindowVisible() )
		return;

	if (m_iCurIME == 0)
		return;

	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// 绘制背景
	pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 0);
//	pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 2);	// 填充灰色底子的效果
//	pLCD->LCDFillRect (fb, w, h, m_x+7, m_y+2, m_w-10, m_h-4, 0);

	// 如果是手写输入状态，则在上栏显示提示文字
	if( m_iCurIME == 6 )
	{
		if( m_bGraffitiPortStatus )
			pLCD->LCDTextOut (fb, w, h, m_x+9, m_y+4, 
					  LCD_MODE_NORMAL, (unsigned char *)"                  ", 255);
		else
			pLCD->LCDTextOut (fb, w, h, m_x+9, m_y+4, 
					  LCD_MODE_NORMAL, (unsigned char *)"与手写板通讯失败！", 255);
	}
	else
	{
		// 绘制上行的文字(可以绘制32个英文字母)
		pLCD->LCDTextOut (fb, w, h, m_x+9, m_y+2, LCD_MODE_NORMAL, m_cUpperLine, 255);
	}

	// 如果是英文输入法，且没有超时，则最后一个字母反白显示；
	// 如果超时了，则最后一个字母的后面添加一条小竖线
	if ((m_iCurIME == 3) || (m_iCurIME == 4))
	{
		if (m_iTimeOut == 0)
		{
			// 未超时，最后一个字母反白显示
			if (m_iOutStrLength > 0)
			{
				int iLength = GetDisplayLength ((char*)m_cUpperLine, m_iOutStrLength - 1);
				unsigned char cc = m_cUpperLine[m_iOutStrLength - 1];
				pLCD->LCDTextOut (fb, w, h, m_x+9+iLength, m_y+2, LCD_MODE_INVERSE, &cc, 1);
			}
		}
		else
		{
			// 超时，如果上行有内容则在最后显示一条小竖线
			if (m_iOutStrLength > 0)
			{
				int iLength = GetDisplayLength ((char*)m_cUpperLine, m_iOutStrLength);
				pLCD->LCDVLine (fb, w, h, m_x+9+iLength, m_y+2, 14, 1);
			}
		}
	}
	// 绘制下行的文字
	pLCD->LCDTextOut (fb, w, h, m_x+9, m_y+19, LCD_MODE_NORMAL, m_cLowerLine, 255);

	// 绘制外框
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

	// 绘制当前输入行指示
	int iYPos = 0;
	if (m_iCurInputLine == IME_CURRENT_UPPER)
		iYPos = m_y+6;
	else
		iYPos = m_y+21;

	pLCD->LCDDrawImage (fb, w, h, m_x, iYPos, 7, 7, LCD_MODE_OR,
				pBitmap_Arror_Right, 7, 7, 0, 0);

	// 绘制文字指示当前输入法
	unsigned char cText[5];
	memset (cText, 0x0, 5);
	memcpy (cText, &cIMEName[(m_iCurIME-1)*4], 4);
	pLCD->LCDTextOut (fb, w, h, m_x+211, m_y+4, LCD_MODE_NORMAL, cText, 4);

	if (!m_bAllSyInvalid)
	{
		// 绘制上栏的左右选择指示箭头
		if ((m_iSyLength > 0) && (m_iSyValidCount > 1))
		{
			if (m_iCurSy > 0)	// 绘制向左的箭头
			{
				pLCD->LCDDrawImage (fb, w, h, m_x+199, m_y+7, 5, 5, LCD_MODE_NORMAL,
					pBitmap_Little_Arror_Left, 5, 5, 0, 0);
			}
			if (m_iCurSy < (m_iSyValidCount-1))	// 绘制向右的箭头
			{
				pLCD->LCDDrawImage (fb, w, h, m_x+204, m_y+7, 5, 5, LCD_MODE_OR,
					pBitmap_Little_Arror_Right, 5, 5, 0, 0);
			}
		}

		// 绘制当前拼音选择的下划线
		int ix = m_x + 9 + (ASC_W + ASC_GAP) * (m_iSyLength + 1) * m_iCurSy;
		int iw = (ASC_W + ASC_GAP) * m_iSyLength;
		pLCD->LCDHLine (fb, w, h, ix, m_y+15, iw, 1);
	}

	// 绘制下栏的上下翻页指示箭头
	if (m_iHzCount > 9)
	{
		if (m_iCurPage > 0)		// 绘制向上箭头
		{
			pLCD->LCDDrawImage (fb, w, h, m_x+230, m_y+20, 5, 5, LCD_MODE_NORMAL,
				pBitmap_Little_Arror_Up, 5, 5, 0, 0);
		}
		if (m_iCurPage < ((m_iHzCount+8)/9-1))	// 绘制向下箭头
		{
			pLCD->LCDDrawImage (fb, w, h, m_x+230, m_y+25, 5, 5, LCD_MODE_NORMAL,
				pBitmap_Little_Arror_Down, 5, 5, 0, 0);
		}
	}
}

// 消息处理，返回1
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

			// 切换输入法
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
						// 解除读串口线程mutex的锁定
						pthread_mutex_unlock( &m_mutexReadCom );
						m_bCleanReadCom = TRUE;
					}
				}
				else
				{
					m_iCurIME = 1;
					// 读串口线程mutex锁定
					pthread_mutex_lock( &m_mutexReadCom );
				}
			}

			// 根据输入法的不同进行不同的初始化
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
				// 符号输入法，将当前输入行设置在下行，并初始化下行的显示内容
				m_iCurInputLine = IME_CURRENT_LOWER;
				m_cHzReturn = (unsigned char*)strPunctuation;
				m_iHzCount = IME_PUNCTUATION_NUM;
				RenewLowerLine ();		// 根据m_cHzReturn和当前行等信息更新下栏
			}

			if (m_iCurIME == 6)
			{
				m_iHzCount = 0;
				m_iCurInputLine = IME_CURRENT_LOWER;
			}

			if (m_iCurIME == 1)                     // 标准拼音输入法
				m_pIMEDB->SetCurIME (1);
			else if (m_iCurIME == 2)                // 方言扩展输入法
				m_pIMEDB->SetCurIME (2);
		}
		else
		{
			switch (m_iCurIME)
			{
			case 1:		// 拼音输入法
			case 2:
				{
					PinYinProc (pWnd, iMsg, wParam, lParam);
				}
				break;
			case 3:		// 英文输入法
			case 4:
				{
					YingWenProc (pWnd, iMsg, wParam, lParam);
				}
				break;
			case 5:		// 符号输入
				{
					PunctuationProc (pWnd, iMsg, wParam, lParam);
				}
				break;
			case 6:		// 手写输入
				{
					GraffitiProc (pWnd, iMsg, wParam, lParam);
				}
			}
		}
	}

	return CTWindow::Proc (pWnd, iMsg, wParam, lParam);
//	UpdateView (this);	// 如果界面刷新不好，解开此语句
}

// 是否可以处理控制按键(如果输入窗是空的，则不处理控制按键，而让CTEdit窗口去处理)
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

// 打开输入法窗口(打开显示，创建联系)
BOOL CIMEWindow::OpenIME (CTWindow* pWnd)
{
	if (pWnd == NULL)
		return FALSE;

	m_pTargetWnd = pWnd;

	// 如果输入法窗口挡住了目标控件，则显示在目标控件的上方
	if ((pWnd->m_y + pWnd->m_h) > 93)
		m_y = pWnd->m_y - 37;
	else
		m_y = 93;

	// 如果手写打开，则默认使用手写输入法
	if( m_iGraffitiPort > 0 )
	{
		m_iCurIME = 6;

		// 启动读串口线程
		m_bGraffitiProcRun = TRUE;
		int ret = pthread_create( &m_threadReadCom, NULL, GraffitiPortProc, (void*)this );
		if( ret != 0 )
			m_bGraffitiPortStatus = FALSE;

		// 解除读串口线程mutex的锁定
		pthread_mutex_unlock( &m_mutexReadCom );
	}
	else
	{
		m_iCurIME = 1;	// 默认拼音输入法
		m_pIMEDB->SetCurIME (1);
	}

	return TRUE;
}

// 关闭输入法窗口(关闭显示，断开联系)
BOOL CIMEWindow::CloseIME (CTWindow* pWnd)
{
	if (pWnd != m_pTargetWnd)
		return FALSE;

	// 如果手写输入有效，则关闭读手写板串口的线程
	if( m_iGraffitiPort > 0 )
	{
		if( m_bGraffitiPortStatus )
		{
			// 解除读串口线程mutex的锁定
			pthread_mutex_unlock( &m_mutexReadCom );

			// 使读串口循环退出
			m_bGraffitiProcRun = FALSE;

			// 等待线程退出
			pthread_join( m_threadReadCom, NULL );
		}
	}

	m_iCurIME = 0;	// 关闭输入法窗口
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

// 看输入法窗口是否处于打开状态
BOOL CIMEWindow::IsIMEOpen ()
{
	if (m_iCurIME == 0)
		return FALSE;
	else
		return TRUE;
}

// 设置手写输入的相关参数
BOOL CIMEWindow::EnableGraffiti( int nSerialPort )
{
	// 不能在输入法窗口打开的状态下改编设置
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

// 处理定时器消息(只用于英文输入法)
void CIMEWindow::OnTimer (int iTimerID, int iInterval)
{
	m_pApp->KillTimer (IME_TIMER_ID);
	m_iTimeOut = 1;
	UpdateView(this);
}

// 向目标窗口发送字符(如果是英文，则只有c1，c2为0)
void CIMEWindow::SendChar (unsigned char c1, unsigned char c2)
{
	_TMSG msg;
	msg.pWnd = m_pTargetWnd;
	msg.message = TMSG_CHAR;
	msg.wParam = c1;
	msg.lParam = c2;
	m_pApp->SendMessage (&msg);
}

// 根据数字件1~9选择字符并发送
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

// 三种不同输入法的处理函数
// 拼音输入法
void CIMEWindow::PinYinProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if (iMsg == TMSG_KEYDOWN)
	{
		switch (wParam)
		{
		case TVK_UP:
			{
				// 向上(下栏，向前翻页)
				if (m_iCurInputLine == IME_CURRENT_LOWER)
				{
					LowerLinePageUp ();
				}
			}
			break;
		case TVK_DOWN:
			{
				// 向下(下栏，向后翻页)
				if (m_iCurInputLine == IME_CURRENT_LOWER)
				{
					LowerLinePageDown ();
				}
			}
			break;
		case TVK_LEFT:
			{
				// 向左(上栏，选择前面的一个拼音)(下栏，向前翻页)
				if (m_iCurInputLine == IME_CURRENT_UPPER)
				{
					if (m_iCurSy > 0)
						m_iCurSy --;
					// 修改拼音的后续处理
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
				// 向右(上栏，选择后面的一个拼音)(下栏，向后翻页)
				if (m_iCurInputLine == IME_CURRENT_UPPER)
				{
					if (m_iCurSy < (m_iSyValidCount - 1))
						m_iCurSy ++;
					// 修改拼音的后续处理
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
				// 删除(上栏，删除一个字符)
				if (m_iCurInputLine == IME_CURRENT_UPPER)
				{
					if (m_iSyLength > 0)
					{
						// 删掉最后一个字符
						m_pIMEDB->SyRemove ();
						// 重新获得拼音长度
						m_iSyLength = m_pIMEDB->GetSyLength ();
						// 查询拼音组合
						m_iSyCount = m_pIMEDB->GetSy (&m_cSyReturn);
						// 修改拼音的后续处理
						m_iCurSy = 0;
						RenewSy ();
					}
				}
			}
			break;
		case TVK_SPACE:
			{
				// 空格键
				SendChar (0x20, 0x0);
			}
			break;
		case TVK_PERIOD:
			{
				// 输入小数点
				SendChar ('.', 0x0);
			}
			break;
		case TVK_ENTER:
			{
				// 确认键(切换到下栏)
				if (m_iCurInputLine == IME_CURRENT_UPPER)
				{
					// 上栏
					// 如果下栏有东西，则将当前输入设置为下栏
					if (m_iHzCount > 0)
					{
						m_iCurInputLine = IME_CURRENT_LOWER;
					}
				}
			}
			break;
		case TVK_ESCAPE:
			{
				// ESCAPE键(切换回上栏)
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
				// 数字键(上栏输入拼音，下栏选择汉字)
				if (m_iCurInputLine == IME_CURRENT_UPPER)
				{
					// 上栏
					// 将按键转换成ASC字符，插入拼音序列
					// 如果返回集是空的，则把刚才插入的删掉
					// 首先将按键转换成字符
					char cKey = MessageToKey (wParam);
					// 向拼音集添加字符
					m_pIMEDB->SyAdd (cKey);
					// 查询拼音组合
					m_iSyCount = m_pIMEDB->GetSy (&m_cSyReturn);
					// 如果拼音组合是空的，则删除刚才输入的字符
					if (m_iSyCount == 0)
					{
						m_pIMEDB->SyRemove ();
						// 查询拼音组合
						m_iSyCount = m_pIMEDB->GetSy (&m_cSyReturn);
					}
					else
					{
						// 重新获得拼音长度
						m_iSyLength = m_pIMEDB->GetSyLength ();
						// 将当前选择设定为第一个
						m_iCurSy = 0;
						// 修改拼音的后续处理
						RenewSy ();
					}
				}
				else if (m_iCurInputLine == IME_CURRENT_LOWER)
				{
					// 下栏
					// 选择一个汉字，然后查找联想词库，根据返回集更新下栏
					// 然后将当前输入切换到上栏
					unsigned char* cHz = LowerLineGetChar (wParam, 2);
					if (cHz != NULL)
					{
						unsigned char cc [3];
						memset (cc, 0x0, 3);
						memcpy (cc, cHz, 2);
						m_iHzCount = m_pIMEDB->GetLx (cc, &m_cHzReturn);
						m_iCurPage = 0;
						RenewLowerLine ();
						// 清空上栏并将当前输入切换到上栏
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

// 英文输入法
void CIMEWindow::YingWenProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if (iMsg == TMSG_KEYDOWN)
	{
		switch (wParam)
		{
		case TVK_SPACE:
			{
				// 空格键
				int i;
				for (i=0; i<m_iOutStrLength; i++)
				{
					char cc = m_cUpperLine[i];
					SendChar (cc, 0x0);
				}
				m_iOutStrLength = 0;
				memset (m_cUpperLine, 0x0, 33);
				// 关闭超时定时器
				m_pApp->KillTimer (IME_TIMER_ID);
				m_iTimeOut = 0;
				// 输出一个空格
				SendChar (0x20, 0x0);
			}
			break;
		case TVK_DOWN:
		case TVK_RIGHT:
			{
				// 向右和向下，提前终止超时定时器
				m_pApp->KillTimer (IME_TIMER_ID);
				m_iTimeOut = 1;
			}
			break;
		case TVK_BACK_SPACE:
			{
				// 删除
				if (m_iOutStrLength > 0)
				{
					m_cUpperLine[m_iOutStrLength - 1] = 0x0;
					m_iOutStrLength --;
				}
				// 关闭超时定时器
				m_pApp->KillTimer (IME_TIMER_ID);
				m_iTimeOut = 1;
			}
			break;
		case TVK_ENTER:
			{
				// 确认键
				int i;
				for (i=0; i<m_iOutStrLength; i++)
				{
					char cc = m_cUpperLine[i];
					SendChar (cc, 0x0);
				}
				m_iOutStrLength = 0;
				memset (m_cUpperLine, 0x0, 33);
				// 关闭超时定时器
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
				// 数字键
				if (m_iOutStrLength < IME_OUT_STRING_LIMIT)
				{
					// 将数字键转换成ASC字符
					char cKey = MessageToKey (wParam);

					// 首先检测超时标志的情况
					if (m_iTimeOut == 1)
					{
						// 已经超时，则输入新的字符
						// 根据大小写状态进行不同的处理
						char cc = KeyToChar (cKey);	// 默认小写字母
						if (cc != 0x0)
						{
							if (m_iCurIME == 4)	// 英文大写
								cc -= 0x20;
							m_cUpperLine[m_iOutStrLength] = cc;
							m_iOutStrLength ++;
						}
					}
					else
					{
						// 没有超时，看新输入的字符是什么字符
						// 如果与上次输入的不是同一个字符，则输入新的字符
						// 如果与上次输入的是同一个字符，则修改最后一个字符
						// 根据大小写状态进行不同的处理
						char cc = 0x0;
						if (m_iOutStrLength > 0)
							cc = CharToKey (m_cUpperLine[m_iOutStrLength-1]);

						if (cc == cKey)		// 同一个按键，修改最后一个字符
						{
							cc = KeyNextChar (m_cUpperLine[m_iOutStrLength-1]);
							if (m_iCurIME == 4)		// 英文大写
								cc -= 0x20;
							m_cUpperLine[m_iOutStrLength-1] = cc;
						}
						else				// 插入新的字母
						{
							cc = KeyToChar (cKey);	// 默认小写字母
							if (cc != 0x0)
							{
								if (m_iCurIME == 4)		// 英文大写
									cc -= 0x20;
								m_cUpperLine[m_iOutStrLength] = cc;
								m_iOutStrLength ++;
							}
						}
					}
					// 设置超时定时器
					m_pApp->SetTimer (this, IME_TIMER_ID, IME_KEY_PRESS_GAP);
					m_iTimeOut = 0;
				}
			}
			break;
		}
	}
}

// 符号输入法
void CIMEWindow::PunctuationProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if (iMsg == TMSG_KEYDOWN)
	{
		switch (wParam)
		{
		case TVK_UP:
			{
				// 向上
				LowerLinePageUp ();
			}
			break;
		case TVK_DOWN:
			{
				// 向下
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
				// 数字键
				LowerLineGetChar (wParam, 1);
			}
			break;
		case TVK_PERIOD:
			{
				// 小数点
				SendChar ('.', 0x0);
			}
			break;
		case TVK_SPACE:
			{
				// 空格键
				SendChar (0x20, 0x0);
			}
			break;
		}
	}
}

// 手写输入法(外部设备通过端口输入备选文字)
void CIMEWindow::GraffitiProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if (iMsg == TMSG_KEYDOWN)
	{
		switch (wParam)
		{
		case TVK_UP:
			{
				// 向上
				LowerLinePageUp ();
			}
			break;
		case TVK_DOWN:
			{
				// 向下
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
				// 数字键
				LowerLineGetChar (wParam, 1);
			}
			break;
		case TVK_PERIOD:
			{
				// 小数点
				SendChar ('.', 0x0);
			}
			break;
		case TVK_SPACE:
			{
				// 空格键
				SendChar (0x20, 0x0);
			}
			break;
		}
	}
}

// 修改拼音以后的后续处理(更新上下栏)
BOOL CIMEWindow::RenewSy ()
{
	// 更新上栏
	RenewUpperLine ();
	// 得到当前选择的拼音字符串
	unsigned char cSymbol[7];
	memset (cSymbol, 0x20, 6);
	cSymbol[6] = 0x0;
	int iCurSyPos = (m_iSyLength + 1) * m_iCurSy;
	memcpy (cSymbol, &m_cUpperLine[iCurSyPos], m_iSyLength);
	// 根据当前拼音组合查询汉字
	m_iHzCount = m_pIMEDB->GetHz (cSymbol, &m_cHzReturn);
	// 将下栏当前页设置为第一页
	m_iCurPage = 0;
	// 更新下栏
	RenewLowerLine ();

	return TRUE;
}

// 根据m_cSyReturn、m_iSyLength、m_iSyCount更新上栏的字符串m_cUpperLine并设置m_iSyValidCount
void CIMEWindow::RenewUpperLine ()
{
	m_bAllSyInvalid = FALSE;

	memset (m_cUpperLine, 0x0, 33);
	m_iSyValidCount = 0;
	int i;
	for (i=0; i<m_iSyCount; i++)
	{
		int iOffset = i * 8;
		// 如果是完全匹配的，则加入上行
		if (m_cSyReturn[iOffset + 6] == '1')
		{
			int iTargetOffset = m_iSyValidCount * ( m_iSyLength + 1);
			memcpy (&m_cUpperLine[iTargetOffset], &m_cSyReturn[iOffset], m_iSyLength);
			m_cUpperLine[iTargetOffset+m_iSyLength] = 0x20;		// 拼音结束位置补一个空格
			m_iSyValidCount ++;
		}
	}

	// 如果总的行数不为0而有效行数为0，
	// 说明所有拼音组合都是不完全组合，这时应当列出所有不完全组合
	// 注意：应消除“go go ho ho”“ko ko lo lo”的现象
	if ((m_iSyCount > 0) && (m_iSyValidCount == 0))
	{
		m_bAllSyInvalid = TRUE;
		char cOldSymbol [7];	// 用比较法消除go go ho ho现象
		for (i=0; i<m_iSyCount; i++)
		{
			int iOffset = i * 8;
			int iTargetOffset = m_iSyValidCount * ( m_iSyLength + 1);
			if ((memcmp (cOldSymbol, &m_cSyReturn[iOffset], m_iSyLength)) != 0)
			{
				memcpy (&m_cUpperLine[iTargetOffset], &m_cSyReturn[iOffset], m_iSyLength);
				m_cUpperLine[iTargetOffset+m_iSyLength] = 0x20;		// 拼音结束位置补一个空格
				m_iSyValidCount ++;
				memcpy (cOldSymbol, &m_cSyReturn[iOffset], 6);
			}
		}
	}
}

// 根据m_cHzReturn、m_iHzCount、m_iCurPage更新下栏的字符串m_cLowerLine
void CIMEWindow::RenewLowerLine ()
{
	memset (m_cLowerLine, 0x0, 37);

	if (m_iHzCount == 0)
		return;

	int iCount = 9;
	// 如果是最后一行，则长度取余数
	if (m_iCurPage == (((m_iHzCount + 8) / 9) - 1))
	{
		iCount = m_iHzCount % 9;
		if (iCount == 0)
			iCount = 9;
	}

	// 设置cLowerLine字符串的内容
	int iOffset = m_iCurPage * 18;
	int i;
	for (i=0; i<iCount; i++)
	{
		m_cLowerLine[i*4] = (i+0x31);				// 标号(从1开始)
		m_cLowerLine[i*4+1] = m_cHzReturn[iOffset+i*2];		// 第一个字节
		m_cLowerLine[i*4+2] = m_cHzReturn[iOffset+i*2+1];	// 第二个字节
		m_cLowerLine[i*4+3] = 0x20;				// 空格
	}
}

// 下行分页的向前翻页处理
void CIMEWindow::LowerLinePageUp ()
{
	if (m_iCurPage > 0)
		m_iCurPage --;
	RenewLowerLine ();
}

// 下行分页的向后翻页处理
void CIMEWindow::LowerLinePageDown ()
{
	int iPageCount = (m_iHzCount + 8) / 9;
	if (m_iCurPage < (iPageCount - 1))
		m_iCurPage ++;
	RenewLowerLine ();
}

// 根据字母查找对应的数字键
char CIMEWindow::CharToKey (char cc)
{
	// 如果是大写字母，则转换为小写字母
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

// 将按键消息转换成按键字符
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

// 根据数字键查找对应的字母(返回第一个)
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

// 查找该字母所在数字键组的下一个字母
char CIMEWindow::KeyNextChar (char cc)
{
	// 如果是大写字母，则转换为小写字母
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
			if (i == 3)	// 该键组的最后一个
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

