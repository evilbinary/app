// TEdit.cpp: implementation of the CTEdit class.
//
//////////////////////////////////////////////////////////////////////
#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTEdit::CTEdit ()
{
	m_Caret.bShow	= FALSE;
	m_Caret.bFlash	= FALSE;
	m_Caret.bValid	= FALSE;
	m_Caret.lTimeInterval = 1;
	m_Caret.x		= 0;
	m_Caret.y		= 0;
	m_Caret.w		= 0;
	m_Caret.h		= 0;

	m_cIMEStatus	= 0;		// 输入法状态；0禁用输入法；1输入法未开；2输入法已开

	m_iSelStart     = -1;       // 插入起始位置
	m_iSelEnd       = -1;       // 插入终止位置
	m_iTextLimit    = 255;      // 字符串默认最大长度
	m_iLeftIndex    = 0;        // 显示区域左端的第一个字符的位置
	

#if defined(CHINESE_SUPPORT)
	m_pIME			= NULL;
#endif //defined(CHINESE_SUPPORT)
}

CTEdit::~CTEdit ()
{
#if defined(CHINESE_SUPPORT)
	m_pIME = NULL;              // m_pIME是全局的，不能删除！！！
#endif // defined(CHINESE_SUPPORT)
}

// 创建编辑框
BOOL CTEdit::Create (CTWindow* pParent,WORD wWndType,WORD wStyle,WORD wStatus,int x,int y,int w,int h,int ID)
{
	// 如果禁用输入法，则将m_cIMEStatus设置为0
	if ((wStyle & TWND_STYLE_DISABLE_IME) > 0)
		m_cIMEStatus = 0;
	else
		m_cIMEStatus = 1;

	if (!CTWindow::Create (pParent,wWndType,wStyle,wStatus,x,y,w,h,ID))
		return FALSE;

	return TRUE;
}

// 虚函数，绘制编辑框
void CTEdit::Paint (CLCD* pLCD)
{
	// 如果不可见，则什么也不绘制
	if( !IsWindowVisible() )
		return;

	int iLeftPos	= m_iLeftIndex;
	int iRightPos	= GetRightDisplayIndex ();

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

	// 绘制文字
	if ((m_wStyle & TWND_STYLE_PASSWORD) == 0)
	{
		// 正常模式
		pLCD->LCDTextOut (fb,w,h,m_x+2,m_y+2,0,(unsigned char*)(m_cCaption+iLeftPos),(iRightPos-iLeftPos));
	}
	else
	{
		// 密码模式
		char cTemp [256];
		memset (cTemp, 0x0, 256);
		strncpy (cTemp, (m_cCaption+iLeftPos), (iRightPos-iLeftPos));
		memset (cTemp, '*', (iRightPos-iLeftPos));
		pLCD->LCDTextOut (fb,w,h,m_x+2,m_y+2,0,(unsigned char*)cTemp,(iRightPos-iLeftPos));
	}
}

// 虚函数，消息处理
// 消息处理过了，返回1，未处理返回0
int CTEdit::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = 0;

#if defined(CHINESE_SUPPORT)
	// 判断当前IME窗口是否可以处理控制键
	BOOL bIMECanHandleControlKey = FALSE;
	if (m_pIME != NULL)
		bIMECanHandleControlKey = m_pIME->CanHandleControlKey();
#endif // defined(CHINESE_SUPPORT)

	// 对按键消息的处理
	if (iMsg == TMSG_KEYDOWN)
	{
		switch (wParam)
		{
		case TVK_TAB:
		case TVK_CAPS_LOCK:
			break;
		case TVK_ESCAPE:
			{

#if defined(CHINESE_SUPPORT)
				// 如果IME处于打开状态，则传给IME窗口处理，并修改iReturn为1
				if (m_cIMEStatus == 2)
				{
					if ((m_pIME->m_iCurInputLine == 0) ||
					    (m_pIME->m_iCurIME == 5))
					{
						// 关闭IME窗口
				 		if (m_pApp->CloseIME(this))
							m_cIMEStatus = 1;

						m_pIME = NULL;
					}
					else
					{
						_TMSG msg;
						msg.pWnd	= m_pIME;
						msg.message = TMSG_KEYDOWN;
						msg.wParam	= TVK_ESCAPE;
						msg.lParam	= 0;
						m_pApp->PostMessage (&msg);
					}
					iReturn = 1;
				}
#endif // defined(CHINESE_SUPPORT)

			}
			break;
		case TVK_UP:
			{

#if defined(CHINESE_SUPPORT)
				// 如果IME处于打开状态，则将此消息发送给IME窗口
				if (m_cIMEStatus == 2)
				{
					_TMSG msg;
					msg.pWnd	= m_pIME;
					msg.message = TMSG_KEYDOWN;
					msg.wParam	= TVK_UP;
					msg.lParam	= 0;
					m_pApp->PostMessage (&msg);
					iReturn = 1;
				}
#endif // defined(CHINESE_SUPPORT)

			}
			break;
		case TVK_DOWN:
			{

#if defined(CHINESE_SUPPORT)
				// 如果IME处于打开状态，则将此消息发送给IME窗口
				if (m_cIMEStatus == 2)
				{
					_TMSG msg;
					msg.pWnd	= m_pIME;
					msg.message = TMSG_KEYDOWN;
					msg.wParam	= TVK_DOWN;
					msg.lParam	= 0;
					m_pApp->PostMessage (&msg);
					iReturn = 1;
				}
#endif // defined(CHINESE_SUPPORT)

			}
			break;
		case TVK_LEFT:			// 将当前输入位置向前移动一个字符
			{

#if defined(CHINESE_SUPPORT)
				// 如果IME处于打开状态，且可以处理控制按键，则将此消息发送给IME窗口
				if ((m_cIMEStatus == 2) && bIMECanHandleControlKey)
				{
					_TMSG msg;
					msg.pWnd    = m_pIME;
					msg.message = TMSG_KEYDOWN;
					msg.wParam  = TVK_LEFT;
					msg.lParam  = 0;
					m_pApp->PostMessage (&msg);
				}
				else
				{
#endif // defined(CHINESE_SUPPORT)

					int iStartPos = m_iSelStart;
					int iEndPos   = m_iSelEnd;

					if ((iStartPos != -1) && (iEndPos != -1))
					{
						if (iStartPos != iEndPos)
						{
							iEndPos = iStartPos;
						}
						else
						{
							if (iStartPos > 0)
							{
								if (IsChinese (m_cCaption, iStartPos, FALSE))
									iStartPos -= 2;
								else
									iStartPos -= 1;

								iEndPos = iStartPos;
							}
						}
						SetSel (iStartPos, iEndPos);
					}

#if defined(CHINESE_SUPPORT)
				}
#endif // defined(CHINESE_SUPPORT)

				iReturn = 1;
			}
			break;
		case TVK_RIGHT:			// 将当前输入位置向后移动一个字符
			{

#if defined(CHINESE_SUPPORT)
				// 如果IME处于打开状态，且可以处理控制按键，则将此消息发送给IME窗口
				if ((m_cIMEStatus == 2) && bIMECanHandleControlKey)
				{
					_TMSG msg;
					msg.pWnd    = m_pIME;
					msg.message = TMSG_KEYDOWN;
					msg.wParam  = TVK_RIGHT;
					msg.lParam  = 0;
					m_pApp->PostMessage (&msg);
				}
				else
				{
#endif // defined(CHINESE_SUPPORT)

					int iStartPos = m_iSelStart;
					int iEndPos   = m_iSelEnd;

					if ((iStartPos != -1) && (iEndPos != -1))
					{
						if (iStartPos != iEndPos)
						{
							iStartPos = iEndPos;
						}
						else
						{
							if (iStartPos < GetTextLength())
							{
								if (IsChinese (m_cCaption, iStartPos, TRUE))
									iStartPos += 2;
								else
									iStartPos += 1;

								iEndPos = iStartPos;
							}
						}
						SetSel (iStartPos, iEndPos);
					}

#if defined(CHINESE_SUPPORT)
				}
#endif // defined(CHINESE_SUPPORT)

				iReturn = 1;
			}
			break;
		case TVK_HOME:			// 将当前插入位置设置在最前
			{

#if defined(CHINESE_SUPPORT)
				// 如果IME处于关闭状态，或者IME窗口中没有需要编辑的字符，则处理此消息
				if (!((m_cIMEStatus == 2) && bIMECanHandleControlKey))
				{
#endif // defined(CHINESE_SUPPORT)

					SetSel (0, 0);

#if defined(CHINESE_SUPPORT)
				}
#endif // defined(CHINESE_SUPPORT)

				iReturn = 1;
			}
			break;
		case TVK_END:			// 将当前插入位置放置在最后
			{

#if defined(CHINESE_SUPPORT)
				// 如果IME处于关闭状态，或者IME窗口中没有需要编辑的字符，则处理此消息
				if (!((m_cIMEStatus == 2) && bIMECanHandleControlKey))
				{
#endif // defined(CHINESE_SUPPORT)

					SetSel (GetTextLength(), GetTextLength());

#if defined(CHINESE_SUPPORT)
				}
#endif // defined(CHINESE_SUPPORT)

				iReturn = 1;
			}
			break;
		case TVK_BACK_SPACE:	// 删除当前插入位置前面的字符
			{

#if defined(CHINESE_SUPPORT)
				// 如果IME处于打开状态，且可以处理控制按键，则将此消息发送给IME窗口
				if ((m_cIMEStatus == 2) && bIMECanHandleControlKey)
				{
					_TMSG msg;
					msg.pWnd    = m_pIME;
					msg.message = TMSG_KEYDOWN;
					msg.wParam  = TVK_BACK_SPACE;
					msg.lParam  = 0;
					m_pApp->PostMessage (&msg);
				}
				else
				{
#endif // defined(CHINESE_SUPPORT)

					int iStartPos	= m_iSelStart;
					int iEndPos		= m_iSelEnd;
					if ((iStartPos != -1) && (iEndPos != -1))
					{
						// 如果是插入点，则删除前面那个字符
						// 如果是选择区域，则删除选中的内容
						if (iStartPos == iEndPos)
						{
							DelOneCharacter (FALSE);
						}
						else
						{
							DelCurSel ();
						}
					}

#if defined(CHINESE_SUPPORT)
				}
#endif // defined(CHINESE_SUPPORT)

				iReturn = 1;
			}
			break;
		case TVK_DELETE:		// 删除当前插入位置后面的字符
			{

#if defined(CHINESE_SUPPORT)
				// 如果IME处于关闭状态，或者IME窗口中没有需要编辑的字符，则处理此消息
				if (!((m_cIMEStatus == 2) && bIMECanHandleControlKey))
				{
#endif // defined(CHINESE_SUPPORT)

					int iStartPos	= m_iSelStart;
					int iEndPos		= m_iSelEnd;
					if ((iStartPos != -1) && (iEndPos != -1))
					{
						// 如果是插入点，则删除后面那个字符
						// 如果是选择区域，则删除选中的内容
						if (iStartPos == iEndPos)
						{
							DelOneCharacter (TRUE);
						}
						else
						{
							DelCurSel ();
						}
					}

#if defined(CHINESE_SUPPORT)
				}
#endif // defined(CHINESE_SUPPORT)

				iReturn = 1;
			}
			break;
		case TVK_F11:		// 打开关闭输入法
			{

			  // test
			  printf("IME Status: %d\n",m_cIMEStatus);
			  // test end

#if defined(CHINESE_SUPPORT)
				// cIMEStatus输入法状态：0禁用输入法；1输入法未开；2输入法已开
				if (m_cIMEStatus == 1)
				{
					// 打开IME
					if (m_pApp->OpenIME(this))
						m_cIMEStatus = 2;

					m_pIME = m_pApp->m_pIMEWnd;
				}
				else if (m_cIMEStatus == 2)
				{
					// 关闭IME
					if (m_pApp->CloseIME(this))
						m_cIMEStatus = 1;

					m_pIME = NULL;
				}
				iReturn = 0;
#endif // defined(CHINESE_SUPPORT)

			}
			break;
		case TVK_F12:		// 切换输入法
			{

#if defined(CHINESE_SUPPORT)
				// 如果IME处于打开状态，则将此消息发送给IME窗口
				if (m_cIMEStatus == 2)
				{
					_TMSG msg;
					msg.pWnd    = m_pIME;
					msg.message = TMSG_KEYDOWN;
					msg.wParam  = TVK_F12;
					msg.lParam  = 0;
					m_pApp->PostMessage (&msg);
				}
				iReturn = 1;
#endif // defined(CHINESE_SUPPORT)

			}
			break;
		case TVK_ENTER:			// 传送给输入法的确认键
			{

#if defined(CHINESE_SUPPORT)
				// 如果IME处于打开状态，则将此消息发送给IME窗口
				// 如果IME未打开，向父窗口发送TVK_TAB消息
				if (m_cIMEStatus == 2)
				{
					_TMSG msg;
					msg.pWnd    = m_pIME;
					msg.message = TMSG_KEYDOWN;
					msg.wParam  = TVK_ENTER;
					msg.lParam  = 0;
					m_pApp->PostMessage (&msg);
					iReturn = 1;
				}
				else
				{
#endif // defined(CHINESE_SUPPORT)

					if( IsWindowEnabled() &&
					    ((m_wStyle  & TWND_STYLE_IGNORE_ENTER) == 0) )
					{ 
						_TMSG msg;
						msg.pWnd    = m_pParent;
						msg.message = TMSG_KEYDOWN;
						msg.wParam  = TVK_TAB;
						msg.lParam  = 0;
						m_pApp->PostMessage (&msg);
						iReturn = 1;
					}
					else
						iReturn = 0;

#if defined(CHINESE_SUPPORT)
				}
#endif // defined(CHINESE_SUPPORT)

			}
			break;
		case TVK_0:				// 数字键和小数点
		case TVK_1:
		case TVK_2:
		case TVK_3:
		case TVK_4:
		case TVK_5:
		case TVK_6:
		case TVK_7:
		case TVK_8:
		case TVK_9:
		case TVK_PERIOD:
		case TVK_SPACE:
			{

#if defined(CHINESE_SUPPORT)
				// 如果IME处于打开状态，则将此消息发送给IME窗口
				// 如果IME未打开，如果当前插入位置有效，则将字符插入
				if (m_cIMEStatus == 2)
				{
					_TMSG msg;
					msg.pWnd    = m_pIME;
					msg.message = TMSG_KEYDOWN;
					msg.wParam  = wParam;
					msg.lParam  = 0;
					m_pApp->PostMessage (&msg);
					iReturn = 1;
				}
				else
				{
#endif // defined(CHINESE_SUPPORT)

					// 插入字符
					char cInsert [2];
					memset (cInsert, 0x0, 2);

					switch (wParam)
					{
					case TVK_0:      cInsert[0] = '0';      break;
					case TVK_1:      cInsert[0] = '1';      break;
					case TVK_2:      cInsert[0] = '2';      break;
					case TVK_3:      cInsert[0] = '3';      break;
					case TVK_4:      cInsert[0] = '4';      break;
					case TVK_5:      cInsert[0] = '5';      break;
					case TVK_6:      cInsert[0] = '6';      break;
					case TVK_7:      cInsert[0] = '7';      break;
					case TVK_8:      cInsert[0] = '8';      break;
					case TVK_9:      cInsert[0] = '9';      break;
					case TVK_PERIOD: cInsert[0] = '.';      break;
					case TVK_SPACE:  cInsert[0] = ' ';      break;
					}

					InsertCharacter (cInsert);
					
					RenewLeftPos ();	// 更改显示区左端第一个字符的索引
					RenewCaret ();		// 更新脱字符信息
					iReturn = 1;

#if defined(CHINESE_SUPPORT)
				}
#endif // defined(CHINESE_SUPPORT)

			}
			break;
		case TVK_00:
			{

#if defined(CHINESE_SUPPORT)
				if (m_cIMEStatus != 2)
				{
#endif // defined(CHINESE_SUPPORT)

					// 插入字符
					char cInsert [3];
					cInsert[0] = '0';
					cInsert[1] = '0';
					cInsert[2] = 0x0;

					InsertCharacter (cInsert);
					
					RenewLeftPos ();	// 更改显示区左端第一个字符的索引
					RenewCaret ();		// 更新脱字符信息
					iReturn = 1;

#if defined(CHINESE_SUPPORT)
				}
#endif // defined(CHINESE_SUPPORT)

			}
			break;
		default:
			{
				// 纯英文环境下，如果禁用输入法，则不允许输入英文字母
				if( (m_wStyle & TWND_STYLE_DISABLE_IME) > 0 )
				{
					iReturn = 1;
					break;
				}

#if defined(CHINESE_SUPPORT)
				// 输入其他英文字母
				// 如果IME处于打开状态，则将此消息发送给IME窗口
				// 如果IME未打开，如果当前插入位置有效，则将字符插入
				if (m_cIMEStatus == 2)
				{
					_TMSG msg;
					msg.pWnd    = m_pIME;
					msg.message = TMSG_KEYDOWN;
					msg.wParam  = wParam;
					msg.lParam  = 0;
					m_pApp->PostMessage (&msg);
					iReturn = 1;
				}
				else
				{
#endif // defined(CHINESE_SUPPORT)

					// 插入字符
					char cInsert [2];
					memset (cInsert, 0x0, 2);

					if( TVKToASC( cInsert, wParam, lParam ) )
					{
						InsertCharacter (cInsert);
						RenewLeftPos ();	// 更改显示区左端第一个字符的索引
						RenewCaret ();		// 更新脱字符信息
						iReturn = 1;
					}

#if defined(CHINESE_SUPPORT)
				}
#endif // defined(CHINESE_SUPPORT)

			}
		}
	}
	else if (iMsg == TMSG_SETFOCUS)
	{
		// 得到焦点，应当将选择范围设置为全选，并设置相应的脱字符
		if (pWnd == this)
		{
			m_iSelStart = 0;
			m_iSelEnd = GetTextLength ();
			RenewLeftPos ();	// 更改显示区左端第一个字符的索引
			RenewCaret ();		// 更新脱字符信息
		}

#if defined(CHINESE_SUPPORT)
		// 自动打开输入法窗口
		if( (m_wStyle & TWND_STYLE_AUTO_OPEN_IME) > 0 )
		{
			if (m_cIMEStatus == 1)
			{
				if (m_pApp->OpenIME(this))
					m_cIMEStatus = 2;

				m_pIME = m_pApp->m_pIMEWnd;
			}
		}
#endif // defined(CHINESE_SUPPORT)

	}
	else if (iMsg == TMSG_KILLFOCUS)
	{
		// 失去焦点，恢复脱字符，如果输入法处于打开状态则关闭输入法
		if (pWnd == this)
		{

#if defined(CHINESE_SUPPORT)
			if (m_cIMEStatus == 2)
			{
				m_cIMEStatus = 1;
				m_pApp->CloseIME(this);
			}
#endif // defined(CHINESE_SUPPORT)

			m_Caret.bValid = FALSE;
			m_Caret.bShow  = FALSE;
			m_pApp->SetCaret (&m_Caret);
		}
	}
	else if (iMsg == TMSG_CHAR)
	{
		// 处理输入法窗口发回的字符
		if (pWnd == this)
		{
			// 插入字符
			char cInsert [3];
			memset (cInsert, 0x0, 3);

			cInsert[0] = (unsigned char)wParam;
			if (wParam > 127)	// 汉字
				cInsert[1] = lParam;

			InsertCharacter (cInsert);
			
			RenewLeftPos ();	// 更改显示区左端第一个字符的索引
			RenewCaret ();		// 更新脱字符信息
		}
	}

	UpdateView (this);
	return iReturn;
}

#if defined(MOUSE_SUPPORT)
// 坐标设备消息处理
int CTEdit::PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = CTWindow::PtProc (pWnd, iMsg, wParam, lParam);

	// 鼠标左键消息
	if( iMsg == TMSG_LBUTTONDOWN )
	{
		if( PtInWindow (wParam, lParam) )
		{
			if( IsWindowEnabled() )
			{
				// 如果本窗口不是焦点，则将本窗口设置为焦点
				if (m_pParent != this)
				{
					_TMSG msg;
					msg.pWnd    = m_pParent;
					msg.message = TMSG_SETCHILDACTIVE;
					msg.wParam  = m_ID;
					msg.lParam  = 0;
					m_pApp->SendMessage (&msg);
				}

				// 改变当前插入点的位置
				int iCurPos = PtInItems( wParam );
				if( iCurPos != -1 )
					SetSel( iCurPos, iCurPos );

				// 设置托拽处理标志
				m_iMouseMoving = 1;
				m_iOldPos = iCurPos;
			}
			iReturn = 1;
		}
	}
	else if( iMsg == TMSG_LBUTTONUP )
	{
		if( m_iMouseMoving == 1 )
		{
			// 鼠标左键弹起，取消托拽
			m_iMouseMoving = 0;
			m_iOldPos = 0;
			iReturn = 1;
		}
	}
	else if( iMsg == TMSG_MOUSEMOVE )
	{
		if( m_iMouseMoving == 1 )
		{
			// 处理托拽
			int iCurPos = PtInItems( wParam );

			int iLeftPos  = m_iLeftIndex;
			int iRightPos = GetRightDisplayIndex();
			if( iCurPos > iRightPos )
			{
				// 左端位置需要向右移动
				iLeftPos = iLeftPos + (iCurPos - iRightPos);

				// 如果当前位置不合理，则将当前位置向右移动一个字节
				if (!IsPosValid (m_cCaption, iLeftPos))
					iLeftPos += 1;

				m_iLeftIndex = iLeftPos;
			}

			SetSel( m_iOldPos, iCurPos );
			RenewCaret();
			iReturn = 1;
		}
	}

	UpdateView (this);
	return iReturn;
}

// 测试坐标落于的位置，该测试不关心y值
int CTEdit::PtInItems( int x )
{
	// 选择到边界之左，则选择到最前
	if( x < m_x )
		return 0;

	// 选择到边界之右，则选择到最后
	if( x > m_x + m_w )
		return GetTextLength();

	int iPointX   = x - m_x - 2;
	int iCurPos   = 0;
	int iLeftPos  = m_iLeftIndex;
	int iRightPos = GetRightDisplayIndex ();

	if (iPointX > 1)
	{
		// 逐个检查合理位置
		char* pString = &(m_cCaption[iLeftPos]);

		int iStrLength = iRightPos - iLeftPos;
		int iDisplayLength = 0;
		int iValidPos = 0;

		int iDistence1 = 0;
		int iDistence2 = 0;
		int i;
		for (i=0; i<iStrLength; i++)
		{
			if (IsChinese (pString, iValidPos, TRUE))
			{
				iValidPos += 2;
				iDisplayLength += (HZK_W + 1);

				iDistence2 = iDisplayLength;
				if ((iDistence1 <= iPointX) && (iDistence2 >= iPointX))
				{
					iCurPos = iValidPos;
					if ((iPointX - iDistence1) < (iDistence2 - iPointX))
						iCurPos -= 2;
					break;
				}
			}
			else
			{
				iValidPos ++;
				iDisplayLength += (ASC_W + 1);
				iDistence2 = iDisplayLength;
				if ((iDistence1 <= iPointX) && (iDistence2 >= iPointX))
				{
					iCurPos = iValidPos;
					if ((iPointX - iDistence1) < (iDistence2 - iPointX))
						iCurPos --;
					break;
				}
			}

			if (iValidPos >= iStrLength)
			{
				iCurPos = iStrLength;
				break;
			}

			iDistence1 = iDistence2;
		}
	}

	return iCurPos + iLeftPos;
}
#endif // defined(MOUSE_SUPPORT)

// 设置当前选择区域的起始位置和终止位置
// 如果位置跨越了汉字，则向后推一个字节
BOOL CTEdit::SetSel (int iStart, int iEnd)
{
	// 如果Start比End大，则交换
	if (iStart > iEnd)
	{
		int iTemp = iEnd;
		iEnd = iStart;
		iStart = iTemp;
	}

	if (!IsPosValid (m_cCaption, iStart))
		iStart ++;

	if (!IsPosValid (m_cCaption, iEnd))
		iEnd ++;

	if (iEnd > GetTextLength ())
		iEnd = GetTextLength ();

	if (iStart < 0)
		iStart = 0;

	if (iEnd < iStart)
		iEnd = iStart;

	m_iSelStart = iStart;
	m_iSelEnd = iEnd;

	RenewLeftPos ();	// 更改显示区左端第一个字符的索引
	RenewCaret ();		// 更新脱字符信息
	return TRUE;
}

// 获得当前选择区域的起始位置和终止位置
BOOL CTEdit::GetSel (int* iStart, int* iEnd)
{
	int iStartPos  = m_iSelStart;
	int iEndPos    = m_iSelEnd;

	if ((iStartPos == -1) || (iEndPos == -1))
		return FALSE;

	*iStart = iStartPos;
	*iEnd   = iEndPos;
	return TRUE;
}

// 向当前位置插入字符串
// 如果当前位置是一个选择区，则替换当前选择区域的字符串，
// 然后将选择区修改成一个插入位置
// 如果当前位置是一个插入位置，则在此插入位置上插入字符串
// 注意，如果总长度超越了长度限制，则截取合适长度的字串
// 截取时应注意汉字的处理
BOOL CTEdit::InsertCharacter (char* cString)
{
	// 选择区域的开始位置和终止位置
	int iStartPos   = m_iSelStart;
	int iEndPos     = m_iSelEnd;

	if ((iStartPos == -1) || (iEndPos == -1))
		return FALSE;

	if (iStartPos > iEndPos)
		return FALSE;

	// 根据长度判断可以插入多少个字符
	int iLengthLimit = m_iTextLimit;
	int iInsertCount = iLengthLimit-(GetTextLength()-(iEndPos-iStartPos));

	// 如果当前EDIT中的字符串长度已经超过了iLengthLimit，
	// 则iInsertCount会变成负数，会导致程序出错
	if( iInsertCount <= 0 )
		return FALSE;

	// 从输入字符串中截取适当的长度(注意判断汉字！)
	char cTemp [256];
	memset (cTemp, 0x0, 256);
	strncpy (cTemp, cString, iInsertCount);
	int iInsertLength = strlen (cTemp);

	if (iInsertLength == 1)
		if ((unsigned char)cTemp[0] >127)
			return TRUE;

	if (!IsPosValid (cTemp, iInsertLength))		// 如果截断了汉字，则减掉一个字节
		iInsertLength --;

	if (iInsertLength <= 0)		// 插入长度为0，不修改当前选区的长度，直接返回
		return TRUE;

	// 插入字符串并修改当前插入点的位置
	char cTemp2 [256];
	memset (cTemp2, 0x0, 256);
	int iOldLength = GetTextLength();
	strncpy (cTemp2, m_cCaption, iStartPos);			// 拷贝出插入点以前的部分
	strncpy ((cTemp2+iStartPos), cTemp, iInsertLength);	// 将插入的部分连接在后面
	strncpy ((cTemp2+iStartPos+iInsertLength),
			 (m_cCaption+iEndPos),
			 (iOldLength-iEndPos));						// 将插入位置以后的部分接在最后
	memset (m_cCaption, 0x0, 256);
	strncpy (m_cCaption, cTemp2, iOldLength+iInsertLength);

	// 设置当前插入点
	m_iSelStart = iStartPos + iInsertLength;
	m_iSelEnd = m_iSelStart;

	RenewLeftPos ();	// 更改显示区左端第一个字符的索引
	RenewCaret ();		// 更新脱字符信息
	return TRUE;
}

// 删除当前位置前面的一个字符或者后面的一个字符
// bMode: TRUE,删后面的;FALSE,删前面的
BOOL CTEdit::DelOneCharacter (BOOL bMode)
{
	// 选择区域的开始位置和终止位置
	int iStartPos	= m_iSelStart;
	int iEndPos		= m_iSelEnd;

	if ((iStartPos == -1) || (iEndPos == -1))
		return FALSE;

	if (iStartPos != iEndPos)	// 不是插入点，退出
		return FALSE;

	if (bMode)
	{
		// 删除插入点后面一个字符
		int iDelLen = 0;
		if (IsChinese (m_cCaption, iStartPos, TRUE))
			iDelLen = 2;
		else
			iDelLen = 1;

		if ((iStartPos+iDelLen) > GetTextLength())
			return FALSE;

		m_iSelEnd = iStartPos + iDelLen;

		if (DelCurSel ())
			return TRUE;
		else
			return FALSE;
	}
	else
	{
		// 删除插入点前面一个字符
		int iDelLen = 0;
		if (IsChinese (m_cCaption, iStartPos, FALSE))
			iDelLen = 2;
		else
			iDelLen = 1;

		if ((iStartPos-iDelLen) < 0)
			return FALSE;

		m_iSelStart = iStartPos - iDelLen;

		if (DelCurSel ())
			return TRUE;
		else
			return FALSE;
	}

	RenewLeftPos ();	// 更改显示区左端第一个字符的索引
	RenewCaret ();		// 更新脱字符信息

}

// 删除当前选中区域的内容
BOOL CTEdit::DelCurSel ()
{
	// 选择区域的开始位置和终止位置
	int iStartPos	= m_iSelStart;
	int iEndPos		= m_iSelEnd;

	if ((iStartPos == -1) || (iEndPos == -1))
		return FALSE;

	if (iStartPos >= iEndPos)	// 没必要删除，直接退出
		return TRUE;

	// 插入字符串并修改当前插入点的位置
	char cTemp2 [256];
	memset (cTemp2, 0x0, 256);
	int iOldLength = GetTextLength();
	strncpy (cTemp2, m_cCaption, iStartPos);			// 拷贝出插入点以前的部分
	strncpy ((cTemp2+iStartPos),
			 (m_cCaption+iEndPos),
			 (iOldLength-iEndPos));						// 将插入位置以后的部分接在最后
	memset (m_cCaption, 0x0, 256);
	strncpy (m_cCaption, cTemp2, iOldLength-(iEndPos-iStartPos));

	// 设置当前插入点
	m_iSelStart = iStartPos;
	m_iSelEnd   = iStartPos;

	RenewLeftPos ();	// 更改显示区左端第一个字符的索引
	RenewCaret ();		// 更新脱字符信息
	return TRUE;
}

// 限制输入字符串的最大长度
BOOL CTEdit::LimitText (int iLength)
{
	if (iLength > 255)
		return FALSE;

	m_iTextLimit = iLength;
	return TRUE;
}

// 清空字符串的内容
BOOL CTEdit::Clean ()
{
	memset (m_cCaption, 0x0, 256);
	m_iSelStart  = 0;
	m_iSelEnd    = 0;
	m_iLeftIndex = 0;
	RenewLeftPos ();	// 更改显示区左端第一个字符的索引
	RenewCaret ();

	return TRUE;
}

// 返回输入法窗口的状态
BOOL CTEdit::IsIMEOpen()
{
        if( m_cIMEStatus == 2 )
                return TRUE;

        return FALSE;
}

// 更改显示区左端第一个字符的索引
void CTEdit::RenewLeftPos ()
{
	// 如果当前脱字符超出了编辑框的矩形范围，则应当更新显示区域的位置
	int iStartPos	= m_iSelStart;
	int iEndPos		= m_iSelEnd;
	int iLeftPos	= m_iLeftIndex;
	int iRightPos	= GetRightDisplayIndex ();

	// 如果脱字符可见，则不进行处理
	if ((iStartPos >= iLeftPos) && (iStartPos <= iRightPos))
		return;

	int iNewLeftPos = 0;
	if (iStartPos < iLeftPos)
	{
		// 左端位置需要向左移动
		iNewLeftPos = iStartPos;
	}
	else if (iStartPos > iRightPos)
	{
		// 左端位置需要向右移动
		iNewLeftPos = iLeftPos + (iEndPos - iRightPos);
		if (iNewLeftPos > iStartPos)
			iNewLeftPos = iStartPos;
	}

	// 如果当前位置不合理，则将当前位置向右移动一个字节
	if (!IsPosValid (m_cCaption, iNewLeftPos))
		iNewLeftPos += 1;
	
	m_iLeftIndex = iNewLeftPos;
}

// 根据当前的脱字符设置更新系统脱字符
void CTEdit::RenewCaret ()
{
	// 如果本控件是焦点，才进行此操作
	if( !IsWindowActive() )
		return;

	int iStartPos	= m_iSelStart;
	int iEndPos		= m_iSelEnd;
	int iLeftPos	= m_iLeftIndex;

	// 根据编辑框矩形尺寸算出当前显示字符串右端的位置
	int iRightPos	= GetRightDisplayIndex ();

	if (iStartPos == iEndPos)
	{
		// 插入位置是一个点，设置成闪烁的竖线
		// 如果脱字符位置超越矩形区域，不显示
		if ((iStartPos < iLeftPos) || (iStartPos > iRightPos))
		{
			m_Caret.bValid = FALSE;
			m_pApp->SetCaret (&m_Caret);
			return;
		}

		// 计算从插入点到脱字符的长度(像素)
		int iLength = GetDisplayLength ((m_cCaption+iLeftPos), (iStartPos-iLeftPos));

		m_Caret.x = m_x+1+iLength;
		m_Caret.y = m_y+1;
		m_Caret.w = 2;
		m_Caret.h = m_h - 2;
		m_Caret.bFlash = TRUE;
		m_Caret.bShow  = TRUE;
		m_Caret.lTimeInterval = 500;
		m_Caret.bValid = TRUE;
	}
	else
	{
		// 插入位置是一个区域，设置成反白的区域
		// 根据边界修改脱字符的开始位置和终止位置
		if (iStartPos < iLeftPos)
			iStartPos = iLeftPos;

		if (iEndPos > iRightPos)
			iEndPos = iRightPos;

		if (iEndPos < iStartPos)
		{
			m_Caret.bValid = FALSE;
			m_pApp->SetCaret (&m_Caret);
			return;
		}

		// 计算脱字符左端的位置(像素)
		int iLeft = GetDisplayLength ((m_cCaption+iLeftPos), (iStartPos-iLeftPos));

		// 计算脱字符右端的位置(像素)
		int iRight = GetDisplayLength ((m_cCaption+iLeftPos), (iEndPos-iLeftPos));

		m_Caret.x = m_x+2+iLeft;
		m_Caret.y = m_y+2;
		m_Caret.w = iRight-iLeft;
		m_Caret.h = m_h - 4;
		m_Caret.bFlash = FALSE;
		m_Caret.bShow  = TRUE;
		m_Caret.lTimeInterval = 2000;
		m_Caret.bValid = TRUE;
	}

	m_pApp->SetCaret (&m_Caret);
}

// 取得当前显示区域最右端字符的索引
int CTEdit::GetRightDisplayIndex ()
{
	int iStartPos	= m_iSelStart;
	int iEndPos		= m_iSelEnd;
	int iLeftPos	= m_iLeftIndex;

	int iRightPos	= iLeftPos;
	int iCheckLen	= 0;
	int iDisplayLength	= 0;
	int iStringLength	= GetTextLength ();

	while (iDisplayLength < (m_w - 4))	// 4,左右边距
	{
		iRightPos = iLeftPos + iCheckLen;
		
		if (IsChinese (m_cCaption, (iLeftPos+iCheckLen), TRUE))
			iCheckLen += 2;
		else
			iCheckLen += 1;

		if ((iLeftPos + iCheckLen) > iStringLength)
			break;

		iDisplayLength = GetDisplayLength ((m_cCaption+iLeftPos), iCheckLen);
	}
	return iRightPos;
}

// 将字母键转换成ASC码
BOOL CTEdit::TVKToASC( char* psString, int nVK, int nMask )
{
#if !defined(CHINESE_SUPPORT)

	if( (nMask & CAPSLOCK_MASK) > 0 )
	{
		// 输入大写字母
		switch( nVK )
		{
		case TVK_A:    psString[0] = 'A';    break;
		case TVK_B:    psString[0] = 'B';    break;
		case TVK_C:    psString[0] = 'C';    break;
		case TVK_D:    psString[0] = 'D';    break;
		case TVK_E:    psString[0] = 'E';    break;
		case TVK_F:    psString[0] = 'F';    break;
		case TVK_G:    psString[0] = 'G';    break;
		case TVK_H:    psString[0] = 'H';    break;
		case TVK_I:    psString[0] = 'I';    break;
		case TVK_J:    psString[0] = 'J';    break;
		case TVK_K:    psString[0] = 'K';    break;
		case TVK_L:    psString[0] = 'L';    break;
		case TVK_M:    psString[0] = 'M';    break;
		case TVK_N:    psString[0] = 'N';    break;
		case TVK_O:    psString[0] = 'O';    break;
		case TVK_P:    psString[0] = 'P';    break;
		case TVK_Q:    psString[0] = 'Q';    break;
		case TVK_R:    psString[0] = 'R';    break;
		case TVK_S:    psString[0] = 'S';    break;
		case TVK_T:    psString[0] = 'T';    break;
		case TVK_U:    psString[0] = 'U';    break;
		case TVK_V:    psString[0] = 'V';    break;
		case TVK_W:    psString[0] = 'W';    break;
		case TVK_X:    psString[0] = 'X';    break;
		case TVK_Y:    psString[0] = 'Y';    break;
		case TVK_Z:    psString[0] = 'Z';    break;
		default:       return FALSE;
		}
	}
	else
	{
		// 输入小写字母
		switch( nVK )
		{
		case TVK_A:    psString[0] = 'a';    break;
		case TVK_B:    psString[0] = 'b';    break;
		case TVK_C:    psString[0] = 'c';    break;
		case TVK_D:    psString[0] = 'd';    break;
		case TVK_E:    psString[0] = 'e';    break;
		case TVK_F:    psString[0] = 'f';    break;
		case TVK_G:    psString[0] = 'g';    break;
		case TVK_H:    psString[0] = 'h';    break;
		case TVK_I:    psString[0] = 'i';    break;
		case TVK_J:    psString[0] = 'j';    break;
		case TVK_K:    psString[0] = 'k';    break;
		case TVK_L:    psString[0] = 'l';    break;
		case TVK_M:    psString[0] = 'm';    break;
		case TVK_N:    psString[0] = 'n';    break;
		case TVK_O:    psString[0] = 'o';    break;
		case TVK_P:    psString[0] = 'p';    break;
		case TVK_Q:    psString[0] = 'q';    break;
		case TVK_R:    psString[0] = 'r';    break;
		case TVK_S:    psString[0] = 's';    break;
		case TVK_T:    psString[0] = 't';    break;
		case TVK_U:    psString[0] = 'u';    break;
		case TVK_V:    psString[0] = 'v';    break;
		case TVK_W:    psString[0] = 'w';    break;
		case TVK_X:    psString[0] = 'x';    break;
		case TVK_Y:    psString[0] = 'y';    break;
		case TVK_Z:    psString[0] = 'z';    break;
		default:       return FALSE;
		}
	}

	psString[1] = 0;
	return TRUE;

#endif //!defined(CHINESE_SUPPORT)

#if defined( CHINESE_SUPPORT )
	return FALSE;
#endif //defined( CHINESE_SUPPORT )

}


/* END */
