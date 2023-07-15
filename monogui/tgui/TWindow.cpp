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

	// 删除子窗口链表
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

// 创建一个窗口；
// 如果父窗口不是NULL，则把该窗口插入父窗口的字窗口链表中；
// tab序号根据父窗口的字窗口数目确定；
// 快捷键列表、脱字符、滚动条、题头文字可以在窗口创建后再进行设置；
BOOL CTWindow::Create
(
	CTWindow* pParent,			// 父窗口指针
	WORD wWndType,				// 窗口类型
	WORD wStyle,				// 窗口的样式
	WORD wStatus,				// 窗口的状态
	int x,
	int y,
	int w,
	int h,						// 绝对位置
	int ID						// 窗口的ID号
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

	// 如果父窗口不是空的，则将自己挂在父窗口的子窗口链表中，并设置相应的参数
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

	// 如果本窗口是按钮且具有默认属性，则修改父窗口的m_pDefaultButton
	if ( ((m_wWndType & TWND_TYPE_BUTTON) > 0)
	   &&((m_wStatus & TWND_STATUS_DEFAULT) > 0) )
	{
		pParent->m_pDefaultButton = this;
	}


	// 如果本窗口具有焦点属性(对话框或者第一个控件)，则向父窗口发送消息将本窗口设置为焦点
	if ((m_wStatus & TWND_STATUS_FOCUSED) > 0)
	{
		_TMSG msg;
		msg.pWnd	= m_pParent;
		msg.message = TMSG_SETCHILDACTIVE;
		msg.wParam  = m_ID;
		msg.lParam  = 0;
		m_pApp->SendMessage (&msg);
	}

	// 发送TMSG_INITWINDOW消息，该消息将引发OnInit处理。
	_TMSG msg;
	msg.pWnd    = this;
	msg.message = TMSG_INITWINDOW;
	msg.wParam  = 0;
	msg.lParam  = 0;
	m_pApp->PostMessage (&msg);
	DebugPrintf("CTWindow::Create() Send Out TMSG_INITWINDOW Message.\n");

	return TRUE;
}

// 虚函数，绘制窗口，绘制附加的滚动条
// 重载此函数，请在最后调用该函数
void CTWindow::Paint (CLCD* pLCD)
{
	// 如果不可见，则什么也不绘制
	if( !IsWindowVisible() )
		return;

	// 绘制附加的滚动条
	if((m_wStyle & TWND_STYLE_NO_SCROLL) == 0)
	{
		if (m_pVScroll != NULL)
			m_pVScroll->Paint (pLCD);
		if (m_pHScroll != NULL)
			m_pHScroll->Paint (pLCD);
	}

	// 然后，调用各个子窗口的绘制函数
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

// 虚函数，消息处理
// 消息处理过了，返回1，未处理返回0
// 如果重载此函数，请首先调用该函数，然后再进行其他处理
int CTWindow::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = 0;

	// 沿焦点路径发送的消息，如果已经被处理，则后续的窗口不再处理。
	// 首先调用焦点窗口的处理函数
	if (m_pActive != NULL)
	{
		if (m_pActive->Proc (pWnd, iMsg, wParam, lParam) == 1)
			iReturn = 1;
	}

	// 如果焦点窗口未做处理，交给默认按钮处理
	if (iReturn != 1)
	{
		if (m_pDefaultButton != NULL)
		{
			if (m_pDefaultButton->Proc (pWnd, iMsg, wParam, lParam) == 1)
				iReturn = 1;
		}
	}
	
	// 针对特定本窗口进行的处理，
	// 无论是否处理了，均不改变iReturn的值。
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
				// 清除按键缓冲区
				m_pApp->CleanKeyBuffer();
				// 调用自己的OnClose函数
				BOOL bClose = OnClose ();
				// 向父窗口发送消息销毁自己
				if (bClose)
				{
					// 首先，向子窗口发送失去焦点的消息
					// 主要是CTEdit控件，需要在失去焦点的时候关闭输入法，隐藏脱字符
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

				// 向桌面窗口发出重绘该子窗口的消息
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
				// 将指定的子窗口设置为焦点
				CTWindow* pWnd = FindChildByID (wParam);
				if ((pWnd != m_pActive) && (pWnd != NULL))
				{
					// 调用自己的OnKillFocus函数
					OnKillFocus (wParam);
					// 给失去焦点的窗口发送TMSG_KILLFOCUS通知消息

					_TMSG msg;
					if (m_pActive != NULL)
					{
						msg.pWnd    = m_pActive;
						msg.message = TMSG_KILLFOCUS;
						msg.wParam  = wParam;
						msg.lParam  = 0;
						m_pApp->SendMessage (&msg);
					}

					// 修改焦点子窗口
					int id = SetFocus (pWnd);

					// 调用自己的OnSetFocus函数
					OnSetFocus( id );
					// 给获得焦点的控件发送TMSG_SETFOCUS通知消息
					msg.pWnd    = FindChildByID( id );
					msg.message = TMSG_SETFOCUS;
					msg.wParam  = id;
					msg.lParam  = 0;
					m_pApp->SendMessage (&msg);
					// 向桌面窗口发出重绘父窗口的消息

					UpdateView (m_pParent);
				}
			}
			break;
		case TMSG_DELETECHILD:
			{
				// 删除指定的子窗口
				CTWindow* pChild = FindChildByID (wParam);

				if( pChild != NULL )
				{
					DeleteChild (pChild);

					// 将当前活动窗口设置为指定窗口
					CTWindow* pActiveWindow = (CTWindow*) lParam;
					if (pActiveWindow != NULL)
					{
						SetFocus (pActiveWindow);

						// 给获得焦点的控件发送TMSG_SETFOCUS通知消息
						_TMSG msg;
						msg.pWnd    = pActiveWindow;
						msg.message = TMSG_SETFOCUS;
						msg.wParam  = pActiveWindow->m_ID;
						msg.lParam  = 0;
						m_pApp->SendMessage (&msg);
					}

					// 向桌面窗口发出重绘自己的消息
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

				OnKillFocus( m_ID );	// 虚函数，处理失去焦点
			}
			break;
		case TMSG_SETFOCUS:
			{
				// 给焦点发送SetFocus消息
				if( m_pActive != NULL )
				{
					_TMSG msg;
					msg.pWnd    = m_pActive;
					msg.message = TMSG_SETFOCUS;
					msg.lParam  = m_pActive->m_ID;
					msg.wParam  = 0;
					m_pApp->SendMessage( &msg );
				}

				OnSetFocus( m_ID );	// 虚函数，处理得到焦点
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
// 坐标设备（鼠标）消息处理
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
				// 如果本窗口不是焦点，而且父窗口的焦点不是对话框
				// 则将本窗口设置为焦点
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

	// 沿焦点路径发送的消息，如果已经被处理，则后续的窗口不再处理。
	// 首先调用焦点窗口的处理函数
	if (m_pActive != NULL)
		iReturn = m_pActive->PtProc (pWnd, iMsg, wParam, lParam);

	// 鼠标消息需要广播处理，但只处理一次
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

	// 鼠标消息由Scroll处理
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
				// 鼠标左键弹起，取消拖拽
				m_iMouseMoving = 0;
				iReturn = 1;
			}
		}
		else if( iMsg == TMSG_MOUSEMOVE )
		{
		  DebugPrintf("MouseMove: x=%d,y=%d\n",wParam,lParam);
			if( m_iMouseMoving == 1 )
			{
				// 处理拖拽
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
				// 处理拖拽
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

// 改变窗口的位置和尺寸
// 设置成功返回TRUE，失败返回FALSE
BOOL CTWindow::SetPosition (int x, int y, int w, int h)
{
	m_x = x;
	m_y = y;
	m_w = w;
	m_h = h;

	// 重新设置滚动条的尺寸
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

// 获得窗口的位置和尺寸
BOOL CTWindow::GetPosition (int* px, int* py, int* pw, int* ph)
{
	*px = m_x;
	*py = m_y;
	*pw = m_w;
	*ph = m_h;
	return TRUE;
}

// 获得窗口的ID号
int CTWindow::GetID ()
{
	return m_ID;
}

// 设置窗口的题头
BOOL CTWindow::SetText (char* pText, int iLength)
{
	if (pText == NULL)
		return FALSE;

	if (iLength > 255)
		iLength = 255;

	strncpy (m_cCaption, pText, iLength);
	return TRUE;
}

// 获取窗口的题头
BOOL CTWindow::GetText (char* pText)
{
	strncpy (pText, m_cCaption, 256);
	return TRUE;
}

// 获得窗口题头字符串的长度
int CTWindow::GetTextLength ()
{
	return strlen (m_cCaption);
}

// 设置当前处于活动状态（处于焦点）的子窗口
// 注意：应当将当前处于活动状态的子窗口改成非活动的
// 返回实际获得焦点的控件的ID号
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
				// 使老窗口失去焦点
				if (pOld != NULL)
					{
					pOld->m_wStatus &= ~TWND_STATUS_FOCUSED;		// 去除焦点属性
					if( pOld->m_wWndType == TWND_TYPE_BUTTON )
					{
						// 去除默认属性
						pOld->m_wStatus &= ~TWND_STATUS_DEFAULT;
						m_pDefaultButton = FindDefaultButton ();// 将默认按钮设置为原始默认按钮
					}
				}

				// 将子窗口设置为焦点
				m_pActive = pFocus;
				m_pActive->m_wStatus |= TWND_STATUS_FOCUSED;		// 给子窗口添加焦点属性
				if( m_pActive->m_wWndType == TWND_TYPE_BUTTON )
				{
					if (m_pDefaultButton != NULL)
						m_pDefaultButton->m_wStatus &= ~TWND_STATUS_DEFAULT;

					m_pActive->m_wStatus |= TWND_STATUS_DEFAULT;	// 如果焦点是按钮，再加默认属性
					m_pDefaultButton = m_pActive;				// 并设置为默认按钮
				}
				else
				{
					// 给默认按钮添加默认属性
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
	// 没有找到可以被设置为焦点的控件
	m_pActive = NULL;
	return -1;
}

// 在子窗口链表中查找相应ID的窗口，如果找不到则返回NULL
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

// 在子窗口链表中查找相应tab号的窗口，如果找不到则返回NULL
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

// 在子窗口链表中查找默认按钮
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

// 查找在位于一个子窗口垂直正下方的子窗口
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

		// 首先查找位于本控件下方的控件
		if( (pNext->m_y > pWnd->m_y) &&
			( abs((pWnd->m_x - pNext->m_x) * 2 + pWnd->m_w - pNext->m_w) < (pWnd->m_w + pNext->m_w) ) )
		{
			// 再找距离最近的控件
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

// 查找在位于一个子窗口垂直正上方的子窗口
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

		// 首先查找位于本控件上方的控件
		// 判断纵向投影是否相交
		if( (pWnd->m_y > pNext->m_y) &&
			( abs((pWnd->m_x - pNext->m_x) * 2 + pWnd->m_w - pNext->m_w) < (pWnd->m_w + pNext->m_w) ) )
		{
			// 再找距离最近的控件
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

// 查找在位于一个子窗口水平正左方的子窗口
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

		// 首先查找位于本控件左方的控件
		// 判断水平投影是否相交
		if( (pWnd->m_x > pNext->m_x) &&
			( abs((pWnd->m_y - pNext->m_y) * 2 + pWnd->m_h - pNext->m_h) < (pWnd->m_h + pNext->m_h) ) )
		{
			// 再找距离最近的控件
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

// 查找在位于一个子窗口水平正右方的子窗口
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

		// 首先查找位于本控件右方的控件
		// 判断水平投影是否相交
		if( (pNext->m_x > pWnd->m_x) &&
			( abs((pWnd->m_y - pNext->m_y) * 2 + pWnd->m_h - pNext->m_h) < (pWnd->m_h + pNext->m_h) ) )
		{
			// 再找距离最近的控件
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


// 删除一个子窗口
// 相应的要更新各个窗口的tab序号
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
			m_pChildren = pChild->m_pNext;	// 将子窗口列表中的下一个设置为首位
		}

		if (m_pActive == pChild)
		{
			// 将当前窗口后面的控件设为焦点
			CTWindow* pWnd = FindChildByTab (pChild->m_iTabOrder+1);
			if (SetFocus (pWnd) == -1)
				m_pActive = NULL;
		}

		if (m_pDefaultButton == pChild)
		{
			m_pParent->m_pDefaultButton = FindDefaultButton ();
		}

		// 修改控件的tab序号
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

// 设置窗口的滚动条
// iMode = 1：设置右侧的垂直滚动条；
// iMode = 2：设置下方的水平滚动条；
// 滚动条的尺寸根据窗口尺寸进行设置
// 如果要设置的滚动条并不存在，则创建
BOOL CTWindow::SetScrollBar (int iMode, int iRange, int iSize, int iPos)
{
	if((m_wStyle & TWND_STYLE_NO_SCROLL) > 0)
		return FALSE;

	if (iMode == 1)
	{
		if (m_pVScroll == NULL)
		{
			// 创建
			m_pVScroll = new CTScroll ();
			if (m_pHScroll == NULL)
			{
				// 只有这一个滚动条，不必为另一个预留空间
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
			// 设置
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
			// 创建
			m_pHScroll = new CTScroll ();
			if (m_pVScroll == NULL)
			{
				// 只有这一个滚动条，不必为另一个预留空间
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
			// 设置
			m_pHScroll->SetRange (iRange);
			m_pHScroll->SetSize (iSize);
			m_pHScroll->SetPos (iPos);
		}
		return TRUE;
	}

	return FALSE;
}

// 控制滚动条的显示与消隐
// iScroll = 1：设置垂直滚动条；iScroll = 2：设置水平滚动条；
// 如果要设置的滚动条并不存在，则返回FALSE
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

// 向桌面发送重绘窗口的消息
BOOL CTWindow::UpdateView (CTWindow* pWnd)
{
	_TMSG msg;
	msg.pWnd    = m_pApp->m_pMainWnd;
	msg.message = TMSG_PAINT;
	msg.wParam  = (int)pWnd;
	msg.lParam  = 0;
	return m_pApp->PostMessage (&msg);
}

// 窗口使能
BOOL CTWindow::EnableWindow( BOOL bEnable )
{
	// 如果从Enable转化为Disable，则进行失去焦点的处理
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

// 判断窗口是否使能
BOOL CTWindow::IsWindowEnabled()
{
	if (((m_wStatus & TWND_STATUS_INVALID) == 0) &&
		((m_wStatus & TWND_STATUS_INVISIBLE) == 0))
		return TRUE;

	return FALSE;
}

// 判断窗口是否可见
BOOL CTWindow::IsWindowVisible()
{
	if( (m_wStatus & TWND_STATUS_INVISIBLE) == 0 )
		return TRUE;

	return FALSE;
}

// 判断窗口是否焦点链上的窗口
BOOL CTWindow::IsWindowActive()
{
	CTWindow* pActive = this;
	CTWindow* pParent = m_pParent;

	while( pParent != NULL )
	{
		if( pParent->m_pActive != pActive )
			return FALSE;

		// 如果焦点关系可以追溯到MainWindow，则返回TRUE
		if( pParent->m_wWndType == TWND_TYPE_DESKTOP )
			return TRUE;

		pActive = pParent;
		pParent = pActive->m_pParent;
	}

	return FALSE;
}

#if defined(MOUSE_SUPPORT)
// 判断一个点是否落在本窗口的范围之内
BOOL CTWindow::PtInWindow(int x, int y)
{
	if( PtInRect( x,y,m_x,m_y,m_x+m_w,m_y+m_h ) )
		return TRUE;

	return FALSE;
}
#endif // defined(MOUSE_SUPPORT)

// 定时器消息处理，虚函数，什么也不做
void CTWindow::OnTimer (int iTimerID, int iInterval)
{
}

// 窗口创建后的初始化处理
void CTWindow::OnInit()
{
}

// 关闭窗口，允许关闭本窗口则返回TRUE，不允许则返回FALSE
BOOL CTWindow::OnClose ()
{
	return TRUE;
}

// 子窗口得到焦点
void CTWindow::OnSetFocus (int id)
{
}

// 子窗口失去焦点
void CTWindow::OnKillFocus (int id)
{
}

// 子窗口数据改变
void CTWindow::OnDataChange (int id)
{
}

#if defined(MOUSE_SUPPORT)
// 与滚动条有关的函数
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
