// TWindow.cpp: implementation of the CTWindow class.
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTWindow::CTWindow()
{
	m_iTabOrder		= 0x0;
	m_pVScroll		= NULL;
	m_pHScroll		= NULL;
	memset (m_cCaption, 0x0, 256);
	m_pApp			= NULL;
	m_pParent		= NULL;
	m_iChildCount	= 0;
	m_pChildren		= NULL;
	m_pActive		= NULL;
	m_pDefaultButton= NULL;
	m_pNext			= NULL;
	m_pPrev			= NULL;
	m_pOldParentActiveWindow = NULL;
#if defined(MOUSE_SUPPORT)
	m_iMouseMoving   = 0;
#endif // defined(MOUSE_SUPPORT)
}

CTWindow::~CTWindow()
{
	if (m_pVScroll != NULL)
	{
		delete m_pVScroll;
		m_pVScroll = NULL;
	}

	if (m_pHScroll != NULL)
	{
		delete m_pHScroll;
		m_pHScroll = NULL;
	}

	// ɾ���Ӵ�������
	if (m_iChildCount == 0)
		return;

	CTWindow* pNext = m_pChildren->m_pNext;
	int i;
	for (i=0; i<m_iChildCount; i++)
	{
		if (m_pChildren != NULL)
		{
			delete m_pChildren;
			m_pChildren = NULL;
		}
		if (pNext != NULL)
		{
			m_pChildren = pNext;
			pNext = m_pChildren->m_pNext;
		}
		else
		{
			return;
		}
	}
}

// ����һ�����ڣ�
// ��������ڲ���NULL����Ѹô��ڲ��븸���ڵ��ִ��������У�
// tab��Ÿ��ݸ����ڵ��ִ�����Ŀȷ����
// ��ݼ��б����ַ�������������ͷ���ֿ����ڴ��ڴ������ٽ������ã�
BOOL CTWindow::Create
(
	CTWindow* pParent,			// ������ָ��
	WORD wWndType,				// ��������
	WORD wStyle,				// ���ڵ���ʽ
	WORD wStatus,				// ���ڵ�״̬
	int x,
	int y,
	int w,
	int h,						// ����λ��
	int ID						// ���ڵ�ID��
)
{
	m_wWndType   = wWndType;
	m_wStyle     = wStyle;
	m_wStatus    = wStatus;
	m_x          = x;
	m_y          = y;
	m_w          = w;
	m_h          = h;
	m_ID         = ID;

	// ��������ڲ��ǿյģ����Լ����ڸ����ڵ��Ӵ��������У���������Ӧ�Ĳ���
	if (pParent == NULL)
	{
		if (m_wWndType == TWND_TYPE_DESKTOP)
		{
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	m_pParent  = pParent;
	m_pApp = pParent->m_pApp;
	m_pOldParentActiveWindow = pParent->m_pActive;
	m_iTabOrder= pParent->m_iChildCount;

	if (pParent->m_iChildCount == 0)
	{
		m_pPrev = NULL;
		m_pNext = NULL;
		m_pParent->m_pChildren = this;
		pParent->m_iChildCount ++;
	}
	else
	{
		m_pNext = pParent->m_pChildren;

		if (pParent->m_pChildren->m_pPrev != NULL)
		{
			m_pPrev = pParent->m_pChildren->m_pPrev;
			m_pNext = pParent->m_pChildren;
			pParent->m_pChildren->m_pPrev->m_pNext = this;
			pParent->m_pChildren->m_pPrev = this;
		}
		else
		{
			m_pPrev = pParent->m_pChildren;
			m_pNext = pParent->m_pChildren;
			pParent->m_pChildren->m_pPrev = this;
			pParent->m_pChildren->m_pNext = this;
		}

		pParent->m_iChildCount ++;
	}

	// ����������ǰ�ť�Ҿ���Ĭ�����ԣ����޸ĸ����ڵ�m_pDefaultButton
	if ( ((m_wWndType & TWND_TYPE_BUTTON) > 0)
	   &&((m_wStatus & TWND_STATUS_DEFAULT) > 0) )
	{
		pParent->m_pDefaultButton = this;
	}


	// ��������ھ��н�������(�Ի�����ߵ�һ���ؼ�)�����򸸴��ڷ�����Ϣ������������Ϊ����
	if ((m_wStatus & TWND_STATUS_FOCUSED) > 0)
	{
		_TMSG msg;
		msg.pWnd	= m_pParent;
		msg.message = TMSG_SETCHILDACTIVE;
		msg.wParam  = m_ID;
		msg.lParam  = 0;
		m_pApp->SendMessage (&msg);
	}

	// ����TMSG_INITWINDOW��Ϣ������Ϣ������OnInit����
	_TMSG msg;
	msg.pWnd    = this;
	msg.message = TMSG_INITWINDOW;
	msg.wParam  = 0;
	msg.lParam  = 0;
	m_pApp->PostMessage (&msg);
	DebugPrintf("CTWindow::Create() Send Out TMSG_INITWINDOW Message.\n");

	return TRUE;
}

// �麯�������ƴ��ڣ����Ƹ��ӵĹ�����
// ���ش˺��������������øú���
void CTWindow::Paint (CLCD* pLCD)
{
	// ������ɼ�����ʲôҲ������
	if( !IsWindowVisible() )
		return;

	// ���Ƹ��ӵĹ�����
	if((m_wStyle & TWND_STYLE_NO_SCROLL) == 0)
	{
		if (m_pVScroll != NULL)
			m_pVScroll->Paint (pLCD);
		if (m_pHScroll != NULL)
			m_pHScroll->Paint (pLCD);
	}

	// Ȼ�󣬵��ø����Ӵ��ڵĻ��ƺ���
	CTWindow* pNext = m_pChildren;
	int i;
	for (i=0; i<m_iChildCount; i++)
	{
		if (pNext == NULL)
			return;
		//	DebugPrintf( "Debug: CTWindow::Paint: pNext->m_wWndType = %d \n", pNext->m_wWndType );
		//	DebugPrintf( "Debug: CTWindow::Paint: pNext->m_cCaption = %s \n", pNext->m_cCaption );
		pNext->Paint (pLCD);
		pNext = pNext->m_pNext;
	}

	pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );
}

// �麯������Ϣ����
// ��Ϣ������ˣ�����1��δ������0
// ������ش˺����������ȵ��øú�����Ȼ���ٽ�����������
int CTWindow::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = 0;

	// �ؽ���·�����͵���Ϣ������Ѿ�������������Ĵ��ڲ��ٴ���
	// ���ȵ��ý��㴰�ڵĴ�����
	if (m_pActive != NULL)
	{
		if (m_pActive->Proc (pWnd, iMsg, wParam, lParam) == 1)
			iReturn = 1;
	}

	// ������㴰��δ����������Ĭ�ϰ�ť����
	if (iReturn != 1)
	{
		if (m_pDefaultButton != NULL)
		{
			if (m_pDefaultButton->Proc (pWnd, iMsg, wParam, lParam) == 1)
				iReturn = 1;
		}
	}
	
	// ����ض������ڽ��еĴ���
	// �����Ƿ����ˣ������ı�iReturn��ֵ��
	if (pWnd == this)
	{
		switch (iMsg)
		{
		case TMSG_INITWINDOW:
			OnInit();
			break;
		case TMSG_TIMER:
			OnTimer( wParam, lParam );
			break;
		case TMSG_CLOSE:
			{
				// �������������
				m_pApp->CleanKeyBuffer();
				// �����Լ���OnClose����
				BOOL bClose = OnClose ();
				// �򸸴��ڷ�����Ϣ�����Լ�
				if (bClose)
				{
					// ���ȣ����Ӵ��ڷ���ʧȥ�������Ϣ
					// ��Ҫ��CTEdit�ؼ�����Ҫ��ʧȥ�����ʱ��ر����뷨���������ַ�
					_TMSG msg;

					if (m_iChildCount != 0)
					{
						msg.message = TMSG_KILLFOCUS;
						msg.lParam  = 0;
						CTWindow* pNext = m_pChildren;
						int i;
						for (i=0; i<m_iChildCount; i++)
						{
							if (pNext != NULL)
							{
								msg.pWnd   = pNext;
								msg.wParam = pNext->m_ID;
								m_pApp->SendMessage (&msg);
								pNext = pNext->m_pNext;
							}
						}
					}

					msg.pWnd    = m_pParent;
					msg.message = TMSG_DELETECHILD;
					msg.wParam  = m_ID;
					if( IsWindowActive() )
						msg.lParam = (int)m_pOldParentActiveWindow;
					else
						msg.lParam = 0;
					m_pApp->PostMessage (&msg);
				}
			}
			break;
		case TMSG_DATACHANGE:
			{
				OnDataChange (wParam);

				// �����洰�ڷ����ػ���Ӵ��ڵ���Ϣ
				CTWindow* pWnd = FindChildByID (wParam);
				_TMSG msg;
				msg.pWnd	= pWnd;
				msg.message = TMSG_KILLFOCUS;
				msg.wParam  = wParam;
				msg.lParam  = 0;
				m_pApp->SendMessage (&msg);
			}
			break;
		case TMSG_SETCHILDACTIVE:
			{
				// ��ָ�����Ӵ�������Ϊ����
				CTWindow* pWnd = FindChildByID (wParam);
				if ((pWnd != m_pActive) && (pWnd != NULL))
				{
					// �����Լ���OnKillFocus����
					OnKillFocus (wParam);
					// ��ʧȥ����Ĵ��ڷ���TMSG_KILLFOCUS֪ͨ��Ϣ

					_TMSG msg;
					if (m_pActive != NULL)
					{
						msg.pWnd    = m_pActive;
						msg.message = TMSG_KILLFOCUS;
						msg.wParam  = wParam;
						msg.lParam  = 0;
						m_pApp->SendMessage (&msg);
					}

					// �޸Ľ����Ӵ���
					int id = SetFocus (pWnd);

					// �����Լ���OnSetFocus����
					OnSetFocus( id );
					// ����ý���Ŀؼ�����TMSG_SETFOCUS֪ͨ��Ϣ
					msg.pWnd    = FindChildByID( id );
					msg.message = TMSG_SETFOCUS;
					msg.wParam  = id;
					msg.lParam  = 0;
					m_pApp->SendMessage (&msg);
					// �����洰�ڷ����ػ游���ڵ���Ϣ

					UpdateView (m_pParent);
				}
			}
			break;
		case TMSG_DELETECHILD:
			{
				// ɾ��ָ�����Ӵ���
				CTWindow* pChild = FindChildByID (wParam);

				if( pChild != NULL )
				{
					DeleteChild (pChild);

					// ����ǰ���������Ϊָ������
					CTWindow* pActiveWindow = (CTWindow*) lParam;
					if (pActiveWindow != NULL)
					{
						SetFocus (pActiveWindow);

						// ����ý���Ŀؼ�����TMSG_SETFOCUS֪ͨ��Ϣ
						_TMSG msg;
						msg.pWnd    = pActiveWindow;
						msg.message = TMSG_SETFOCUS;
						msg.wParam  = pActiveWindow->m_ID;
						msg.lParam  = 0;
						m_pApp->SendMessage (&msg);
					}

					// �����洰�ڷ����ػ��Լ�����Ϣ
					UpdateView (this);
				}
			}
			break;
		case TMSG_KILLFOCUS:
			{
				if (m_iChildCount != 0)
				{
					_TMSG msg;
					msg.message = TMSG_KILLFOCUS;
					msg.lParam  = 0;
					CTWindow* pNext = m_pChildren;
					int i;
					for (i=0; i<m_iChildCount; i++)
					{
						if (pNext != NULL)
						{
							msg.pWnd   = pNext;
							msg.wParam = pNext->m_ID;
							m_pApp->SendMessage (&msg);
							pNext = pNext->m_pNext;
						}
					}
				}

				OnKillFocus( m_ID );	// �麯��������ʧȥ����
			}
			break;
		case TMSG_SETFOCUS:
			{
				// �����㷢��SetFocus��Ϣ
				if( m_pActive != NULL )
				{
					_TMSG msg;
					msg.pWnd    = m_pActive;
					msg.message = TMSG_SETFOCUS;
					msg.lParam  = m_pActive->m_ID;
					msg.wParam  = 0;
					m_pApp->SendMessage( &msg );
				}

				OnSetFocus( m_ID );	// �麯��������õ�����
			}
			break;
		default:
			{
			}
		}
	}

	return iReturn;

}

#if defined(MOUSE_SUPPORT)
// �����豸����꣩��Ϣ����
int CTWindow::PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = 0;

	if (iMsg == TMSG_LBUTTONDOWN)
	{
		if (PtInWindow (wParam, lParam) )
		{
			if( IsWindowEnabled() )
			{
				// ��������ڲ��ǽ��㣬���Ҹ����ڵĽ��㲻�ǶԻ���
				// �򽫱���������Ϊ����
				if (m_pParent != NULL)
				{
					CTWindow* pParentActive = m_pParent->m_pActive;
					if (pParentActive != NULL)
					{
						if( (pParentActive != this) && (pParentActive->m_wWndType != TWND_TYPE_DIALOG) )
						{
							_TMSG msg;	
							msg.pWnd    = m_pParent;
							msg.message = TMSG_SETCHILDACTIVE;
							msg.wParam  = m_ID;
							msg.lParam  = 0;
							m_pApp->SendMessage (&msg);
						}
					}
					else
					{
						_TMSG msg;	
						msg.pWnd    = m_pParent;
						msg.message = TMSG_SETCHILDACTIVE;
						msg.wParam  = m_ID;
						msg.lParam  = 0;
						m_pApp->SendMessage (&msg);
					}
				}
			}
		}
	}

	// �ؽ���·�����͵���Ϣ������Ѿ�������������Ĵ��ڲ��ٴ���
	// ���ȵ��ý��㴰�ڵĴ�����
	if (m_pActive != NULL)
		iReturn = m_pActive->PtProc (pWnd, iMsg, wParam, lParam);

	// �����Ϣ��Ҫ�㲥������ֻ����һ��
	if (iReturn != 1)
	{
		if (iMsg == TMSG_LBUTTONDOWN)
		{
			CTWindow* pNext = m_pChildren;
			int i;
			for (i=0; i<m_iChildCount; i++)
			{
				if (pNext->PtProc (pWnd, iMsg, wParam, lParam) == 1)
				{
					iReturn = 1;
					break;
				}
				pNext = pNext->m_pNext;
			}
		}
	}

	// �����Ϣ��Scroll����
	if (iReturn != 1)
	{
		if (iMsg == TMSG_LBUTTONDOWN)
		{
			int iRet = 0;

			if (m_pVScroll != NULL)
			{
				iRet = m_pVScroll->TestPt (wParam, lParam);
				if (iRet > 0)
				{
					switch (iRet)
					{
					case SCROLL_PT_UP:
						OnScrollUp();
						break;
					case SCROLL_PT_PAGEUP:
						OnScrollPageUp();
						break;
					case SCROLL_PT_DOWN:
						OnScrollDown();
						break;
					case SCROLL_PT_PAGEDOWN:
						OnScrollPageDown();
						break;
					case SCROLL_PT_BUTTON:
						m_pVScroll->RecordPos( lParam );
						m_iMouseMoving = 1;
						break;
					}
					iReturn = 1;
				}
			}

			if( (iRet == 0) && (m_pHScroll != NULL) )
			{
				int iRet = m_pHScroll->TestPt (wParam, lParam);
				if (iRet > 0)
				{
					switch (iRet)
					{
					case SCROLL_PT_UP:
						OnScrollLeft();
						break;
					case SCROLL_PT_PAGEUP:
						OnScrollPageLeft();
						break;
					case SCROLL_PT_DOWN:
						OnScrollRight();
						break;
					case SCROLL_PT_PAGEDOWN:
						OnScrollPageRight();
						break;
					case SCROLL_PT_BUTTON:
						m_pHScroll->RecordPos( wParam );
						m_iMouseMoving = 2;
						break;
					}
					iReturn = 1;
				}
			}
		}
		else if( iMsg == TMSG_LBUTTONUP )
		{
			if( m_iMouseMoving > 0 )
			{
				// ����������ȡ����ק
				m_iMouseMoving = 0;
				iReturn = 1;
			}
		}
		else if( iMsg == TMSG_MOUSEMOVE )
		{
		  DebugPrintf("MouseMove: x=%d,y=%d\n",wParam,lParam);
			if( m_iMouseMoving == 1 )
			{
				// ������ק
				if( m_pVScroll != NULL )
				{
					int iNewPos = m_pVScroll->TestNewPos( wParam, lParam );
					if( iNewPos != -1 )
						OnVScrollNewPos( iNewPos );
					iReturn = 1;
				}
			}
			else if( m_iMouseMoving == 2 )
			{
				// ������ק
				if( m_pHScroll != NULL )
				{
					int iNewPos = m_pHScroll->TestNewPos( wParam, lParam );
					if( iNewPos != -1 )
						OnHScrollNewPos( iNewPos );
					iReturn = 1;
				}
			}
		}
	}

	return iReturn;
}
#endif // defined(MOUSE_SUPPORT)

// �ı䴰�ڵ�λ�úͳߴ�
// ���óɹ�����TRUE��ʧ�ܷ���FALSE
BOOL CTWindow::SetPosition (int x, int y, int w, int h)
{
	m_x = x;
	m_y = y;
	m_w = w;
	m_h = h;

	// �������ù������ĳߴ�
	if (m_pVScroll != NULL)
	{
		m_pVScroll->m_x = m_x+m_w-13;
		m_pVScroll->m_y = m_y;
		m_pVScroll->m_w = 13;
		m_pVScroll->m_h = m_h;
	}

	if (m_pHScroll != NULL)
	{
		m_pHScroll->m_x = m_x;
		m_pHScroll->m_y = m_y+m_h-13;
		m_pHScroll->m_w = m_w;
		m_pHScroll->m_h = 13;
	}

	return TRUE;
}

// ��ô��ڵ�λ�úͳߴ�
BOOL CTWindow::GetPosition (int* px, int* py, int* pw, int* ph)
{
	*px = m_x;
	*py = m_y;
	*pw = m_w;
	*ph = m_h;
	return TRUE;
}

// ��ô��ڵ�ID��
int CTWindow::GetID ()
{
	return m_ID;
}

// ���ô��ڵ���ͷ
BOOL CTWindow::SetText (char* pText, int iLength)
{
	if (pText == NULL)
		return FALSE;

	if (iLength > 255)
		iLength = 255;

	strncpy (m_cCaption, pText, iLength);
	return TRUE;
}

// ��ȡ���ڵ���ͷ
BOOL CTWindow::GetText (char* pText)
{
	strncpy (pText, m_cCaption, 256);
	return TRUE;
}

// ��ô�����ͷ�ַ����ĳ���
int CTWindow::GetTextLength ()
{
	return strlen (m_cCaption);
}

// ���õ�ǰ���ڻ״̬�����ڽ��㣩���Ӵ���
// ע�⣺Ӧ������ǰ���ڻ״̬���Ӵ��ڸĳɷǻ��
// ����ʵ�ʻ�ý���Ŀؼ���ID��
int CTWindow::SetFocus (CTWindow* pWnd)
{
	CTWindow* pOld = m_pActive;
	CTWindow* pFocus = pWnd;

	int i;
	for (i=0; i<m_iChildCount; i++)
	{
		if (pFocus == NULL)
			return -1;

		if( pFocus->IsWindowEnabled() )
		{
			if (pOld != pFocus)
			{
				// ʹ�ϴ���ʧȥ����
				if (pOld != NULL)
					{
					pOld->m_wStatus &= ~TWND_STATUS_FOCUSED;		// ȥ����������
					if( pOld->m_wWndType == TWND_TYPE_BUTTON )
					{
						// ȥ��Ĭ������
						pOld->m_wStatus &= ~TWND_STATUS_DEFAULT;
						m_pDefaultButton = FindDefaultButton ();// ��Ĭ�ϰ�ť����ΪԭʼĬ�ϰ�ť
					}
				}

				// ���Ӵ�������Ϊ����
				m_pActive = pFocus;
				m_pActive->m_wStatus |= TWND_STATUS_FOCUSED;		// ���Ӵ�����ӽ�������
				if( m_pActive->m_wWndType == TWND_TYPE_BUTTON )
				{
					if (m_pDefaultButton != NULL)
						m_pDefaultButton->m_wStatus &= ~TWND_STATUS_DEFAULT;

					m_pActive->m_wStatus |= TWND_STATUS_DEFAULT;	// ��������ǰ�ť���ټ�Ĭ������
					m_pDefaultButton = m_pActive;				// ������ΪĬ�ϰ�ť
				}
				else
				{
					// ��Ĭ�ϰ�ť���Ĭ������
					CTWindow* pWnd = FindDefaultButton ();
					if (pWnd != NULL)
					{
						m_pDefaultButton = pWnd;
						m_pDefaultButton->m_wStatus |= TWND_STATUS_DEFAULT;
					}
				}
			}
			return pFocus->m_ID;
		}
		pFocus = pFocus->m_pNext;
	}
	// û���ҵ����Ա�����Ϊ����Ŀؼ�
	m_pActive = NULL;
	return -1;
}

// ���Ӵ��������в�����ӦID�Ĵ��ڣ�����Ҳ����򷵻�NULL
CTWindow* CTWindow::FindChildByID (int id)
{
	CTWindow* pNext = m_pChildren;
	int i;
	for (i=0; i<m_iChildCount; i++)
	{
		if (pNext->m_ID == id)
		{
			return pNext;
		}
		pNext = pNext->m_pNext;
	}
	return NULL;
}

// ���Ӵ��������в�����Ӧtab�ŵĴ��ڣ�����Ҳ����򷵻�NULL
CTWindow* CTWindow::FindChildByTab (int iTab)
{
	CTWindow* pNext = m_pChildren;
	int i;
	for (i=0; i<m_iChildCount; i++)
	{
		if (pNext->m_iTabOrder == iTab)
		{
			return pNext;
		}
		pNext = pNext->m_pNext;
	}
	return NULL;
}

// ���Ӵ��������в���Ĭ�ϰ�ť
CTWindow* CTWindow::FindDefaultButton ()
{
	CTWindow* pNext = m_pChildren;
	int i;
	for (i=0; i<m_iChildCount; i++)
	{
		if ((pNext->m_wStyle & TWND_STYLE_ORIDEFAULT) > 0)
		{
			return pNext;
		}
		pNext = pNext->m_pNext;
	}
	return NULL;
}

// ������λ��һ���Ӵ��ڴ�ֱ���·����Ӵ���
CTWindow* CTWindow::FindWindowDown( CTWindow* pWnd )
{
	CTWindow* pNext = m_pChildren;
	CTWindow* pRet  = NULL;
	int i;
	int nTmp;
	int nLen = SCREEN_H * SCREEN_H + SCREEN_W * SCREEN_W;
	for( i=0; i<m_iChildCount; i++ )
	{
		if( (pNext == pWnd)|| !(pNext->IsWindowEnabled()) )
		{
			pNext = pNext->m_pNext;
			continue;
		}

		// ���Ȳ���λ�ڱ��ؼ��·��Ŀؼ�
		if( (pNext->m_y > pWnd->m_y) &&
			( abs((pWnd->m_x - pNext->m_x) * 2 + pWnd->m_w - pNext->m_w) < (pWnd->m_w + pNext->m_w) ) )
		{
			// ���Ҿ�������Ŀؼ�
			int dx = pWnd->m_x - pNext->m_x;
			int dy = pWnd->m_y - pNext->m_y;
			nTmp = dx * dx + dy * dy;
			if( ( nTmp > 0 )&&(nTmp < nLen ) )
			{
				nLen = nTmp;
				pRet = pNext;
			}
		}

		pNext = pNext->m_pNext;
	}

	return pRet;
}

// ������λ��һ���Ӵ��ڴ�ֱ���Ϸ����Ӵ���
CTWindow* CTWindow::FindWindowUp( CTWindow* pWnd )
{
	CTWindow* pNext = m_pChildren;
	CTWindow* pRet  = NULL;
	int i;
	int nTmp;
	int nLen = SCREEN_H * SCREEN_H + SCREEN_W * SCREEN_W;
	for( i=0; i<m_iChildCount; i++ )
	{
		if( (pNext == pWnd)|| !(pNext->IsWindowEnabled()) )
		{
			pNext = pNext->m_pNext;
			continue;
		}

		// ���Ȳ���λ�ڱ��ؼ��Ϸ��Ŀؼ�
		// �ж�����ͶӰ�Ƿ��ཻ
		if( (pWnd->m_y > pNext->m_y) &&
			( abs((pWnd->m_x - pNext->m_x) * 2 + pWnd->m_w - pNext->m_w) < (pWnd->m_w + pNext->m_w) ) )
		{
			// ���Ҿ�������Ŀؼ�
			int dx = pWnd->m_x - pNext->m_x;
			int dy = pWnd->m_y - pNext->m_y;
			nTmp = dx * dx + dy * dy;
			if( ( nTmp > 0 )&&(nTmp < nLen ) )
			{
				nLen = nTmp;
				pRet = pNext;
			}
		}

		pNext = pNext->m_pNext;
	}

	return pRet;
}

// ������λ��һ���Ӵ���ˮƽ���󷽵��Ӵ���
CTWindow* CTWindow::FindWindowLeft( CTWindow* pWnd )
{
	CTWindow* pNext = m_pChildren;
	CTWindow* pRet  = NULL;
	int i;
	int nTmp;
	int nLen = SCREEN_H * SCREEN_H + SCREEN_W * SCREEN_W;
	for( i=0; i<m_iChildCount; i++ )
	{
		if( (pNext == pWnd)|| !(pNext->IsWindowEnabled()) )
		{
			pNext = pNext->m_pNext;
			continue;
		}

		// ���Ȳ���λ�ڱ��ؼ��󷽵Ŀؼ�
		// �ж�ˮƽͶӰ�Ƿ��ཻ
		if( (pWnd->m_x > pNext->m_x) &&
			( abs((pWnd->m_y - pNext->m_y) * 2 + pWnd->m_h - pNext->m_h) < (pWnd->m_h + pNext->m_h) ) )
		{
			// ���Ҿ�������Ŀؼ�
			int dx = pWnd->m_x - pNext->m_x;
			int dy = pWnd->m_y - pNext->m_y;
			nTmp = dx * dx + dy * dy;
			if( ( nTmp > 0 )&&(nTmp < nLen ) )
			{
				nLen = nTmp;
				pRet = pNext;
			}
		}

		pNext = pNext->m_pNext;
	}

	return pRet;
}

// ������λ��һ���Ӵ���ˮƽ���ҷ����Ӵ���
CTWindow* CTWindow::FindWindowRight( CTWindow* pWnd )
{
	CTWindow* pNext = m_pChildren;
	CTWindow* pRet  = NULL;
	int i;
	int nTmp;
	int nLen = SCREEN_H * SCREEN_H + SCREEN_W * SCREEN_W;
	for( i=0; i<m_iChildCount; i++ )
	{
		if( (pNext == pWnd)|| !(pNext->IsWindowEnabled()) )
		{
			pNext = pNext->m_pNext;
			continue;
		}

		// ���Ȳ���λ�ڱ��ؼ��ҷ��Ŀؼ�
		// �ж�ˮƽͶӰ�Ƿ��ཻ
		if( (pNext->m_x > pWnd->m_x) &&
			( abs((pWnd->m_y - pNext->m_y) * 2 + pWnd->m_h - pNext->m_h) < (pWnd->m_h + pNext->m_h) ) )
		{
			// ���Ҿ�������Ŀؼ�
			int dx = pWnd->m_x - pNext->m_x;
			int dy = pWnd->m_y - pNext->m_y;
			nTmp = dx * dx + dy * dy;
			if( nTmp < nLen )
			{
				nLen = nTmp;
				pRet = pNext;
			}
		}

		pNext = pNext->m_pNext;
	}

	return pRet;
}


// ɾ��һ���Ӵ���
// ��Ӧ��Ҫ���¸������ڵ�tab���
BOOL CTWindow::DeleteChild (CTWindow* pChild)
{
	if (m_iChildCount <= 0)
		return FALSE;

	if (m_iChildCount == 1)
	{
		m_pChildren		= NULL;
		m_pActive		= NULL;
		m_pDefaultButton= NULL;
		m_iChildCount	= 0;
	}
	else if (m_iChildCount > 1)
	{
		pChild->m_pPrev->m_pNext = pChild->m_pNext;
		pChild->m_pNext->m_pPrev = pChild->m_pPrev;
		m_iChildCount --;

		if (m_pChildren == pChild)
		{
			if (pChild->m_pNext == NULL)
				return FALSE;
			m_pChildren = pChild->m_pNext;	// ���Ӵ����б��е���һ������Ϊ��λ
		}

		if (m_pActive == pChild)
		{
			// ����ǰ���ں���Ŀؼ���Ϊ����
			CTWindow* pWnd = FindChildByTab (pChild->m_iTabOrder+1);
			if (SetFocus (pWnd) == -1)
				m_pActive = NULL;
		}

		if (m_pDefaultButton == pChild)
		{
			m_pParent->m_pDefaultButton = FindDefaultButton ();
		}

		// �޸Ŀؼ���tab���
		int iTab = pChild->m_iTabOrder;
		CTWindow* pNext = m_pChildren;
		int i;
		for (i=0; i<m_iChildCount; i++)
		{
			if (pNext == NULL)
				return FALSE;

			if (pNext->m_iTabOrder > iTab)
			{
				pNext->m_iTabOrder --;
			}
			pNext = pNext->m_pNext;
		}
	}
	delete pChild;
	return TRUE;
}

// ���ô��ڵĹ�����
// iMode = 1�������Ҳ�Ĵ�ֱ��������
// iMode = 2�������·���ˮƽ��������
// �������ĳߴ���ݴ��ڳߴ��������
// ���Ҫ���õĹ������������ڣ��򴴽�
BOOL CTWindow::SetScrollBar (int iMode, int iRange, int iSize, int iPos)
{
	if((m_wStyle & TWND_STYLE_NO_SCROLL) > 0)
		return FALSE;

	if (iMode == 1)
	{
		if (m_pVScroll == NULL)
		{
			// ����
			m_pVScroll = new CTScroll ();
			if (m_pHScroll == NULL)
			{
				// ֻ����һ��������������Ϊ��һ��Ԥ���ռ�
				m_pVScroll->Create (1, m_x+m_w-13, m_y, 13, m_h, iRange, iSize, iPos);
			}
			else
			{
				m_pVScroll->Create (1, m_x+m_w-13, m_y, 13, m_h-13, iRange, iSize, iPos);
				m_pHScroll->m_w -= 13;
			}
		}
		else
		{
			// ����
			m_pVScroll->SetRange (iRange);
			m_pVScroll->SetSize (iSize);
			m_pVScroll->SetPos (iPos);
		}
		return TRUE;
	}
	else if (iMode == 2)
	{
		if (m_pHScroll == NULL)
		{
			// ����
			m_pHScroll = new CTScroll ();
			if (m_pVScroll == NULL)
			{
				// ֻ����һ��������������Ϊ��һ��Ԥ���ռ�
				m_pHScroll->Create (2, m_x, m_y+m_h-13, m_w, 13, iRange, iSize, iPos);
			}
			else
			{
				m_pHScroll->Create (2, m_x, m_y+m_h-13, m_w-13, 13, iRange, iSize, iPos);
				m_pVScroll->m_h -= 13;
			}
		}
		else
		{
			// ����
			m_pHScroll->SetRange (iRange);
			m_pHScroll->SetSize (iSize);
			m_pHScroll->SetPos (iPos);
		}
		return TRUE;
	}

	return FALSE;
}

// ���ƹ���������ʾ������
// iScroll = 1�����ô�ֱ��������iScroll = 2������ˮƽ��������
// ���Ҫ���õĹ������������ڣ��򷵻�FALSE
BOOL CTWindow::ShowScrollBar (int iScroll, BOOL bShow)
{
	if((m_wStyle & TWND_STYLE_NO_SCROLL) > 0)
		return FALSE;

	if (iScroll == 1)
	{
		if (m_pVScroll == NULL)
		{
			return FALSE;
		}
		else
		{
			if (bShow)
			{
				m_pVScroll->m_iStatus = 1;
			}
			else
			{
				m_pVScroll->m_iStatus = 0;
			}
			return TRUE;
		}
	}
	else if (iScroll == 2)
	{
		if (m_pHScroll == NULL)
		{
			return FALSE;
		}
		else
		{
			if (bShow)
			{
				m_pHScroll->m_iStatus = 1;
			}
			else
			{
				m_pHScroll->m_iStatus = 0;
			}
			return TRUE;
		}
	}
	
	return FALSE;
}

// �����淢���ػ洰�ڵ���Ϣ
BOOL CTWindow::UpdateView (CTWindow* pWnd)
{
	_TMSG msg;
	msg.pWnd    = m_pApp->m_pMainWnd;
	msg.message = TMSG_PAINT;
	msg.wParam  = (int)pWnd;
	msg.lParam  = 0;
	return m_pApp->PostMessage (&msg);
}

// ����ʹ��
BOOL CTWindow::EnableWindow( BOOL bEnable )
{
	// �����Enableת��ΪDisable�������ʧȥ����Ĵ���
	if( ((m_wStatus & TWND_STATUS_INVALID) == 0) && (!bEnable) )
	{
		struct _TMSG msg;
		msg.pWnd    = this;
		msg.message = TMSG_KILLFOCUS;
		msg.wParam  = 0;
		msg.lParam  = 0;
		m_pApp->SendMessage( &msg );
	}

	if( bEnable )
		m_wStatus &= ~TWND_STATUS_INVALID;
	else
		m_wStatus |=  TWND_STATUS_INVALID;

	return TRUE;
}

// �жϴ����Ƿ�ʹ��
BOOL CTWindow::IsWindowEnabled()
{
	if (((m_wStatus & TWND_STATUS_INVALID) == 0) &&
		((m_wStatus & TWND_STATUS_INVISIBLE) == 0))
		return TRUE;

	return FALSE;
}

// �жϴ����Ƿ�ɼ�
BOOL CTWindow::IsWindowVisible()
{
	if( (m_wStatus & TWND_STATUS_INVISIBLE) == 0 )
		return TRUE;

	return FALSE;
}

// �жϴ����Ƿ񽹵����ϵĴ���
BOOL CTWindow::IsWindowActive()
{
	CTWindow* pActive = this;
	CTWindow* pParent = m_pParent;

	while( pParent != NULL )
	{
		if( pParent->m_pActive != pActive )
			return FALSE;

		// ��������ϵ����׷�ݵ�MainWindow���򷵻�TRUE
		if( pParent->m_wWndType == TWND_TYPE_DESKTOP )
			return TRUE;

		pActive = pParent;
		pParent = pActive->m_pParent;
	}

	return FALSE;
}

#if defined(MOUSE_SUPPORT)
// �ж�һ�����Ƿ����ڱ����ڵķ�Χ֮��
BOOL CTWindow::PtInWindow(int x, int y)
{
	if( PtInRect( x,y,m_x,m_y,m_x+m_w,m_y+m_h ) )
		return TRUE;

	return FALSE;
}
#endif // defined(MOUSE_SUPPORT)

// ��ʱ����Ϣ�����麯����ʲôҲ����
void CTWindow::OnTimer (int iTimerID, int iInterval)
{
}

// ���ڴ�����ĳ�ʼ������
void CTWindow::OnInit()
{
}

// �رմ��ڣ�����رձ������򷵻�TRUE���������򷵻�FALSE
BOOL CTWindow::OnClose ()
{
	return TRUE;
}

// �Ӵ��ڵõ�����
void CTWindow::OnSetFocus (int id)
{
}

// �Ӵ���ʧȥ����
void CTWindow::OnKillFocus (int id)
{
}

// �Ӵ������ݸı�
void CTWindow::OnDataChange (int id)
{
}

#if defined(MOUSE_SUPPORT)
// ��������йصĺ���
void CTWindow::OnScrollUp ()
{
}
void CTWindow::OnScrollDown ()
{
}
void CTWindow::OnScrollLeft ()
{
}
void CTWindow::OnScrollRight ()
{
}
void CTWindow::OnScrollPageUp ()
{
}
void CTWindow::OnScrollPageDown ()
{
}
void CTWindow::OnScrollPageLeft ()
{
}
void CTWindow::OnScrollPageRight ()
{
}
void CTWindow::OnVScrollNewPos (int iNewPos)
{
}
void CTWindow::OnHScrollNewPos (int iNewPos)
{
}
#endif // defined(MOUSE_SUPPORT)
/* END */
