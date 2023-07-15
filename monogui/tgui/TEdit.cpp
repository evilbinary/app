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

	m_cIMEStatus	= 0;		// ���뷨״̬��0�������뷨��1���뷨δ����2���뷨�ѿ�

	m_iSelStart     = -1;       // ������ʼλ��
	m_iSelEnd       = -1;       // ������ֹλ��
	m_iTextLimit    = 255;      // �ַ���Ĭ����󳤶�
	m_iLeftIndex    = 0;        // ��ʾ������˵ĵ�һ���ַ���λ��
	

#if defined(CHINESE_SUPPORT)
	m_pIME			= NULL;
#endif //defined(CHINESE_SUPPORT)
}

CTEdit::~CTEdit ()
{
#if defined(CHINESE_SUPPORT)
	m_pIME = NULL;              // m_pIME��ȫ�ֵģ�����ɾ��������
#endif // defined(CHINESE_SUPPORT)
}

// �����༭��
BOOL CTEdit::Create (CTWindow* pParent,WORD wWndType,WORD wStyle,WORD wStatus,int x,int y,int w,int h,int ID)
{
	// ����������뷨����m_cIMEStatus����Ϊ0
	if ((wStyle & TWND_STYLE_DISABLE_IME) > 0)
		m_cIMEStatus = 0;
	else
		m_cIMEStatus = 1;

	if (!CTWindow::Create (pParent,wWndType,wStyle,wStatus,x,y,w,h,ID))
		return FALSE;

	return TRUE;
}

// �麯�������Ʊ༭��
void CTEdit::Paint (CLCD* pLCD)
{
	// ������ɼ�����ʲôҲ������
	if( !IsWindowVisible() )
		return;

	int iLeftPos	= m_iLeftIndex;
	int iRightPos	= GetRightDisplayIndex ();

	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// ���Ʊ߿���䱳��
	pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 0);
	if ((m_wStyle & TWND_STYLE_NO_BORDER) == 0)
	{
		pLCD->LCDHLine (fb, w, h, m_x, m_y, m_w, 1);
		pLCD->LCDHLine (fb, w, h, m_x, m_y+m_h-1, m_w, 1);
		pLCD->LCDVLine (fb, w, h, m_x, m_y, m_h, 1);
		pLCD->LCDVLine (fb, w, h, m_x+m_w-1, m_y, m_h, 1);
	}

	// ��������
	if ((m_wStyle & TWND_STYLE_PASSWORD) == 0)
	{
		// ����ģʽ
		pLCD->LCDTextOut (fb,w,h,m_x+2,m_y+2,0,(unsigned char*)(m_cCaption+iLeftPos),(iRightPos-iLeftPos));
	}
	else
	{
		// ����ģʽ
		char cTemp [256];
		memset (cTemp, 0x0, 256);
		strncpy (cTemp, (m_cCaption+iLeftPos), (iRightPos-iLeftPos));
		memset (cTemp, '*', (iRightPos-iLeftPos));
		pLCD->LCDTextOut (fb,w,h,m_x+2,m_y+2,0,(unsigned char*)cTemp,(iRightPos-iLeftPos));
	}
}

// �麯������Ϣ����
// ��Ϣ������ˣ�����1��δ������0
int CTEdit::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = 0;

#if defined(CHINESE_SUPPORT)
	// �жϵ�ǰIME�����Ƿ���Դ�����Ƽ�
	BOOL bIMECanHandleControlKey = FALSE;
	if (m_pIME != NULL)
		bIMECanHandleControlKey = m_pIME->CanHandleControlKey();
#endif // defined(CHINESE_SUPPORT)

	// �԰�����Ϣ�Ĵ���
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
				// ���IME���ڴ�״̬���򴫸�IME���ڴ������޸�iReturnΪ1
				if (m_cIMEStatus == 2)
				{
					if ((m_pIME->m_iCurInputLine == 0) ||
					    (m_pIME->m_iCurIME == 5))
					{
						// �ر�IME����
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
				// ���IME���ڴ�״̬���򽫴���Ϣ���͸�IME����
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
				// ���IME���ڴ�״̬���򽫴���Ϣ���͸�IME����
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
		case TVK_LEFT:			// ����ǰ����λ����ǰ�ƶ�һ���ַ�
			{

#if defined(CHINESE_SUPPORT)
				// ���IME���ڴ�״̬���ҿ��Դ�����ư������򽫴���Ϣ���͸�IME����
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
		case TVK_RIGHT:			// ����ǰ����λ������ƶ�һ���ַ�
			{

#if defined(CHINESE_SUPPORT)
				// ���IME���ڴ�״̬���ҿ��Դ�����ư������򽫴���Ϣ���͸�IME����
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
		case TVK_HOME:			// ����ǰ����λ����������ǰ
			{

#if defined(CHINESE_SUPPORT)
				// ���IME���ڹر�״̬������IME������û����Ҫ�༭���ַ����������Ϣ
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
		case TVK_END:			// ����ǰ����λ�÷��������
			{

#if defined(CHINESE_SUPPORT)
				// ���IME���ڹر�״̬������IME������û����Ҫ�༭���ַ����������Ϣ
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
		case TVK_BACK_SPACE:	// ɾ����ǰ����λ��ǰ����ַ�
			{

#if defined(CHINESE_SUPPORT)
				// ���IME���ڴ�״̬���ҿ��Դ�����ư������򽫴���Ϣ���͸�IME����
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
						// ����ǲ���㣬��ɾ��ǰ���Ǹ��ַ�
						// �����ѡ��������ɾ��ѡ�е�����
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
		case TVK_DELETE:		// ɾ����ǰ����λ�ú�����ַ�
			{

#if defined(CHINESE_SUPPORT)
				// ���IME���ڹر�״̬������IME������û����Ҫ�༭���ַ����������Ϣ
				if (!((m_cIMEStatus == 2) && bIMECanHandleControlKey))
				{
#endif // defined(CHINESE_SUPPORT)

					int iStartPos	= m_iSelStart;
					int iEndPos		= m_iSelEnd;
					if ((iStartPos != -1) && (iEndPos != -1))
					{
						// ����ǲ���㣬��ɾ�������Ǹ��ַ�
						// �����ѡ��������ɾ��ѡ�е�����
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
		case TVK_F11:		// �򿪹ر����뷨
			{

			  // test
			  printf("IME Status: %d\n",m_cIMEStatus);
			  // test end

#if defined(CHINESE_SUPPORT)
				// cIMEStatus���뷨״̬��0�������뷨��1���뷨δ����2���뷨�ѿ�
				if (m_cIMEStatus == 1)
				{
					// ��IME
					if (m_pApp->OpenIME(this))
						m_cIMEStatus = 2;

					m_pIME = m_pApp->m_pIMEWnd;
				}
				else if (m_cIMEStatus == 2)
				{
					// �ر�IME
					if (m_pApp->CloseIME(this))
						m_cIMEStatus = 1;

					m_pIME = NULL;
				}
				iReturn = 0;
#endif // defined(CHINESE_SUPPORT)

			}
			break;
		case TVK_F12:		// �л����뷨
			{

#if defined(CHINESE_SUPPORT)
				// ���IME���ڴ�״̬���򽫴���Ϣ���͸�IME����
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
		case TVK_ENTER:			// ���͸����뷨��ȷ�ϼ�
			{

#if defined(CHINESE_SUPPORT)
				// ���IME���ڴ�״̬���򽫴���Ϣ���͸�IME����
				// ���IMEδ�򿪣��򸸴��ڷ���TVK_TAB��Ϣ
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
		case TVK_0:				// ���ּ���С����
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
				// ���IME���ڴ�״̬���򽫴���Ϣ���͸�IME����
				// ���IMEδ�򿪣������ǰ����λ����Ч�����ַ�����
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

					// �����ַ�
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
					
					RenewLeftPos ();	// ������ʾ����˵�һ���ַ�������
					RenewCaret ();		// �������ַ���Ϣ
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

					// �����ַ�
					char cInsert [3];
					cInsert[0] = '0';
					cInsert[1] = '0';
					cInsert[2] = 0x0;

					InsertCharacter (cInsert);
					
					RenewLeftPos ();	// ������ʾ����˵�һ���ַ�������
					RenewCaret ();		// �������ַ���Ϣ
					iReturn = 1;

#if defined(CHINESE_SUPPORT)
				}
#endif // defined(CHINESE_SUPPORT)

			}
			break;
		default:
			{
				// ��Ӣ�Ļ����£�����������뷨������������Ӣ����ĸ
				if( (m_wStyle & TWND_STYLE_DISABLE_IME) > 0 )
				{
					iReturn = 1;
					break;
				}

#if defined(CHINESE_SUPPORT)
				// ��������Ӣ����ĸ
				// ���IME���ڴ�״̬���򽫴���Ϣ���͸�IME����
				// ���IMEδ�򿪣������ǰ����λ����Ч�����ַ�����
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

					// �����ַ�
					char cInsert [2];
					memset (cInsert, 0x0, 2);

					if( TVKToASC( cInsert, wParam, lParam ) )
					{
						InsertCharacter (cInsert);
						RenewLeftPos ();	// ������ʾ����˵�һ���ַ�������
						RenewCaret ();		// �������ַ���Ϣ
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
		// �õ����㣬Ӧ����ѡ��Χ����Ϊȫѡ����������Ӧ�����ַ�
		if (pWnd == this)
		{
			m_iSelStart = 0;
			m_iSelEnd = GetTextLength ();
			RenewLeftPos ();	// ������ʾ����˵�һ���ַ�������
			RenewCaret ();		// �������ַ���Ϣ
		}

#if defined(CHINESE_SUPPORT)
		// �Զ������뷨����
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
		// ʧȥ���㣬�ָ����ַ���������뷨���ڴ�״̬��ر����뷨
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
		// �������뷨���ڷ��ص��ַ�
		if (pWnd == this)
		{
			// �����ַ�
			char cInsert [3];
			memset (cInsert, 0x0, 3);

			cInsert[0] = (unsigned char)wParam;
			if (wParam > 127)	// ����
				cInsert[1] = lParam;

			InsertCharacter (cInsert);
			
			RenewLeftPos ();	// ������ʾ����˵�һ���ַ�������
			RenewCaret ();		// �������ַ���Ϣ
		}
	}

	UpdateView (this);
	return iReturn;
}

#if defined(MOUSE_SUPPORT)
// �����豸��Ϣ����
int CTEdit::PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = CTWindow::PtProc (pWnd, iMsg, wParam, lParam);

	// ��������Ϣ
	if( iMsg == TMSG_LBUTTONDOWN )
	{
		if( PtInWindow (wParam, lParam) )
		{
			if( IsWindowEnabled() )
			{
				// ��������ڲ��ǽ��㣬�򽫱���������Ϊ����
				if (m_pParent != this)
				{
					_TMSG msg;
					msg.pWnd    = m_pParent;
					msg.message = TMSG_SETCHILDACTIVE;
					msg.wParam  = m_ID;
					msg.lParam  = 0;
					m_pApp->SendMessage (&msg);
				}

				// �ı䵱ǰ������λ��
				int iCurPos = PtInItems( wParam );
				if( iCurPos != -1 )
					SetSel( iCurPos, iCurPos );

				// ������ק�����־
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
			// ����������ȡ����ק
			m_iMouseMoving = 0;
			m_iOldPos = 0;
			iReturn = 1;
		}
	}
	else if( iMsg == TMSG_MOUSEMOVE )
	{
		if( m_iMouseMoving == 1 )
		{
			// ������ק
			int iCurPos = PtInItems( wParam );

			int iLeftPos  = m_iLeftIndex;
			int iRightPos = GetRightDisplayIndex();
			if( iCurPos > iRightPos )
			{
				// ���λ����Ҫ�����ƶ�
				iLeftPos = iLeftPos + (iCurPos - iRightPos);

				// �����ǰλ�ò������򽫵�ǰλ�������ƶ�һ���ֽ�
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

// �����������ڵ�λ�ã��ò��Բ�����yֵ
int CTEdit::PtInItems( int x )
{
	// ѡ�񵽱߽�֮����ѡ����ǰ
	if( x < m_x )
		return 0;

	// ѡ�񵽱߽�֮�ң���ѡ�����
	if( x > m_x + m_w )
		return GetTextLength();

	int iPointX   = x - m_x - 2;
	int iCurPos   = 0;
	int iLeftPos  = m_iLeftIndex;
	int iRightPos = GetRightDisplayIndex ();

	if (iPointX > 1)
	{
		// ���������λ��
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

// ���õ�ǰѡ���������ʼλ�ú���ֹλ��
// ���λ�ÿ�Խ�˺��֣��������һ���ֽ�
BOOL CTEdit::SetSel (int iStart, int iEnd)
{
	// ���Start��End���򽻻�
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

	RenewLeftPos ();	// ������ʾ����˵�һ���ַ�������
	RenewCaret ();		// �������ַ���Ϣ
	return TRUE;
}

// ��õ�ǰѡ���������ʼλ�ú���ֹλ��
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

// ��ǰλ�ò����ַ���
// �����ǰλ����һ��ѡ���������滻��ǰѡ��������ַ�����
// Ȼ��ѡ�����޸ĳ�һ������λ��
// �����ǰλ����һ������λ�ã����ڴ˲���λ���ϲ����ַ���
// ע�⣬����ܳ��ȳ�Խ�˳������ƣ����ȡ���ʳ��ȵ��ִ�
// ��ȡʱӦע�⺺�ֵĴ���
BOOL CTEdit::InsertCharacter (char* cString)
{
	// ѡ������Ŀ�ʼλ�ú���ֹλ��
	int iStartPos   = m_iSelStart;
	int iEndPos     = m_iSelEnd;

	if ((iStartPos == -1) || (iEndPos == -1))
		return FALSE;

	if (iStartPos > iEndPos)
		return FALSE;

	// ���ݳ����жϿ��Բ�����ٸ��ַ�
	int iLengthLimit = m_iTextLimit;
	int iInsertCount = iLengthLimit-(GetTextLength()-(iEndPos-iStartPos));

	// �����ǰEDIT�е��ַ��������Ѿ�������iLengthLimit��
	// ��iInsertCount���ɸ������ᵼ�³������
	if( iInsertCount <= 0 )
		return FALSE;

	// �������ַ����н�ȡ�ʵ��ĳ���(ע���жϺ��֣�)
	char cTemp [256];
	memset (cTemp, 0x0, 256);
	strncpy (cTemp, cString, iInsertCount);
	int iInsertLength = strlen (cTemp);

	if (iInsertLength == 1)
		if ((unsigned char)cTemp[0] >127)
			return TRUE;

	if (!IsPosValid (cTemp, iInsertLength))		// ����ض��˺��֣������һ���ֽ�
		iInsertLength --;

	if (iInsertLength <= 0)		// ���볤��Ϊ0�����޸ĵ�ǰѡ���ĳ��ȣ�ֱ�ӷ���
		return TRUE;

	// �����ַ������޸ĵ�ǰ������λ��
	char cTemp2 [256];
	memset (cTemp2, 0x0, 256);
	int iOldLength = GetTextLength();
	strncpy (cTemp2, m_cCaption, iStartPos);			// �������������ǰ�Ĳ���
	strncpy ((cTemp2+iStartPos), cTemp, iInsertLength);	// ������Ĳ��������ں���
	strncpy ((cTemp2+iStartPos+iInsertLength),
			 (m_cCaption+iEndPos),
			 (iOldLength-iEndPos));						// ������λ���Ժ�Ĳ��ֽ������
	memset (m_cCaption, 0x0, 256);
	strncpy (m_cCaption, cTemp2, iOldLength+iInsertLength);

	// ���õ�ǰ�����
	m_iSelStart = iStartPos + iInsertLength;
	m_iSelEnd = m_iSelStart;

	RenewLeftPos ();	// ������ʾ����˵�һ���ַ�������
	RenewCaret ();		// �������ַ���Ϣ
	return TRUE;
}

// ɾ����ǰλ��ǰ���һ���ַ����ߺ����һ���ַ�
// bMode: TRUE,ɾ�����;FALSE,ɾǰ���
BOOL CTEdit::DelOneCharacter (BOOL bMode)
{
	// ѡ������Ŀ�ʼλ�ú���ֹλ��
	int iStartPos	= m_iSelStart;
	int iEndPos		= m_iSelEnd;

	if ((iStartPos == -1) || (iEndPos == -1))
		return FALSE;

	if (iStartPos != iEndPos)	// ���ǲ���㣬�˳�
		return FALSE;

	if (bMode)
	{
		// ɾ����������һ���ַ�
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
		// ɾ�������ǰ��һ���ַ�
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

	RenewLeftPos ();	// ������ʾ����˵�һ���ַ�������
	RenewCaret ();		// �������ַ���Ϣ

}

// ɾ����ǰѡ�����������
BOOL CTEdit::DelCurSel ()
{
	// ѡ������Ŀ�ʼλ�ú���ֹλ��
	int iStartPos	= m_iSelStart;
	int iEndPos		= m_iSelEnd;

	if ((iStartPos == -1) || (iEndPos == -1))
		return FALSE;

	if (iStartPos >= iEndPos)	// û��Ҫɾ����ֱ���˳�
		return TRUE;

	// �����ַ������޸ĵ�ǰ������λ��
	char cTemp2 [256];
	memset (cTemp2, 0x0, 256);
	int iOldLength = GetTextLength();
	strncpy (cTemp2, m_cCaption, iStartPos);			// �������������ǰ�Ĳ���
	strncpy ((cTemp2+iStartPos),
			 (m_cCaption+iEndPos),
			 (iOldLength-iEndPos));						// ������λ���Ժ�Ĳ��ֽ������
	memset (m_cCaption, 0x0, 256);
	strncpy (m_cCaption, cTemp2, iOldLength-(iEndPos-iStartPos));

	// ���õ�ǰ�����
	m_iSelStart = iStartPos;
	m_iSelEnd   = iStartPos;

	RenewLeftPos ();	// ������ʾ����˵�һ���ַ�������
	RenewCaret ();		// �������ַ���Ϣ
	return TRUE;
}

// ���������ַ�������󳤶�
BOOL CTEdit::LimitText (int iLength)
{
	if (iLength > 255)
		return FALSE;

	m_iTextLimit = iLength;
	return TRUE;
}

// ����ַ���������
BOOL CTEdit::Clean ()
{
	memset (m_cCaption, 0x0, 256);
	m_iSelStart  = 0;
	m_iSelEnd    = 0;
	m_iLeftIndex = 0;
	RenewLeftPos ();	// ������ʾ����˵�һ���ַ�������
	RenewCaret ();

	return TRUE;
}

// �������뷨���ڵ�״̬
BOOL CTEdit::IsIMEOpen()
{
        if( m_cIMEStatus == 2 )
                return TRUE;

        return FALSE;
}

// ������ʾ����˵�һ���ַ�������
void CTEdit::RenewLeftPos ()
{
	// �����ǰ���ַ������˱༭��ľ��η�Χ����Ӧ��������ʾ�����λ��
	int iStartPos	= m_iSelStart;
	int iEndPos		= m_iSelEnd;
	int iLeftPos	= m_iLeftIndex;
	int iRightPos	= GetRightDisplayIndex ();

	// ������ַ��ɼ����򲻽��д���
	if ((iStartPos >= iLeftPos) && (iStartPos <= iRightPos))
		return;

	int iNewLeftPos = 0;
	if (iStartPos < iLeftPos)
	{
		// ���λ����Ҫ�����ƶ�
		iNewLeftPos = iStartPos;
	}
	else if (iStartPos > iRightPos)
	{
		// ���λ����Ҫ�����ƶ�
		iNewLeftPos = iLeftPos + (iEndPos - iRightPos);
		if (iNewLeftPos > iStartPos)
			iNewLeftPos = iStartPos;
	}

	// �����ǰλ�ò������򽫵�ǰλ�������ƶ�һ���ֽ�
	if (!IsPosValid (m_cCaption, iNewLeftPos))
		iNewLeftPos += 1;
	
	m_iLeftIndex = iNewLeftPos;
}

// ���ݵ�ǰ�����ַ����ø���ϵͳ���ַ�
void CTEdit::RenewCaret ()
{
	// ������ؼ��ǽ��㣬�Ž��д˲���
	if( !IsWindowActive() )
		return;

	int iStartPos	= m_iSelStart;
	int iEndPos		= m_iSelEnd;
	int iLeftPos	= m_iLeftIndex;

	// ���ݱ༭����γߴ������ǰ��ʾ�ַ����Ҷ˵�λ��
	int iRightPos	= GetRightDisplayIndex ();

	if (iStartPos == iEndPos)
	{
		// ����λ����һ���㣬���ó���˸������
		// ������ַ�λ�ó�Խ�������򣬲���ʾ
		if ((iStartPos < iLeftPos) || (iStartPos > iRightPos))
		{
			m_Caret.bValid = FALSE;
			m_pApp->SetCaret (&m_Caret);
			return;
		}

		// ����Ӳ���㵽���ַ��ĳ���(����)
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
		// ����λ����һ���������óɷ��׵�����
		// ���ݱ߽��޸����ַ��Ŀ�ʼλ�ú���ֹλ��
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

		// �������ַ���˵�λ��(����)
		int iLeft = GetDisplayLength ((m_cCaption+iLeftPos), (iStartPos-iLeftPos));

		// �������ַ��Ҷ˵�λ��(����)
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

// ȡ�õ�ǰ��ʾ�������Ҷ��ַ�������
int CTEdit::GetRightDisplayIndex ()
{
	int iStartPos	= m_iSelStart;
	int iEndPos		= m_iSelEnd;
	int iLeftPos	= m_iLeftIndex;

	int iRightPos	= iLeftPos;
	int iCheckLen	= 0;
	int iDisplayLength	= 0;
	int iStringLength	= GetTextLength ();

	while (iDisplayLength < (m_w - 4))	// 4,���ұ߾�
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

// ����ĸ��ת����ASC��
BOOL CTEdit::TVKToASC( char* psString, int nVK, int nMask )
{
#if !defined(CHINESE_SUPPORT)

	if( (nMask & CAPSLOCK_MASK) > 0 )
	{
		// �����д��ĸ
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
		// ����Сд��ĸ
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
