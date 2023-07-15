// TList.cpp: implementation of the CTList class.
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTList::CTList()
{
	m_iItemCount = 0;      // 用于存放当前有多少条列表项，初始为零
	m_iTopIndex  = 0;      // 用于存放当前显示列表中最上面一条的Index值，初始为零
	m_iCurSel    = -1;     // 用于存放当前选中的列表项，初始为－1
	m_pContent = NULL;
}

CTList::~CTList()
{
	RemoveAll();
}

// 创建列表框
BOOL CTList::Create
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
	if (!CTWindow::Create (pParent,TWND_TYPE_LIST,wStyle,TWND_STATUS_NORMAL,x,y,w,h,ID))
		return FALSE;

	m_iHeightOfLinage = (m_h - 3) / LIST_ITEM_H;		// 一页能够显示的条目数
	RenewScroll();
	return TRUE;
}

// 绘制列表框
void CTList::Paint(CLCD* pLCD)
{
	// 如果不可见，则什么也不绘制
	if( !IsWindowVisible() )
		return;

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

	// 如果指定了立体效果，则绘制阴影
	if ((m_wStyle & TWND_STYLE_SOLID) > 0)
	{
		pLCD->LCDHLine (fb, w, h, m_x+1, m_y+m_h, m_w, 1);
		pLCD->LCDVLine (fb, w, h, m_x+m_w, m_y+1, m_h, 1);
	}

	// 绘制列表内容
	int iCount			= m_iItemCount;
	int iTopItem		= m_iTopIndex;
	int iCurSel			= m_iCurSel;
	int iCanDiaplayed	= m_iHeightOfLinage;

	char cTemp[256];
	int i;
	for( i=0; i<iCanDiaplayed; i++)
	{
		if( iTopItem+i >= iCount )
			break;

		// 取得要显示的字符串
		GetString( iTopItem+i, cTemp );
		int iTextLength = GetStringLength( iTopItem+i );
		// 计算可以显示的长度
		int iWidth = m_w - 4;
		if( (m_pVScroll != NULL) && ((m_wStyle & TWND_STYLE_NO_SCROLL)==0) )
		{
			if( m_pVScroll->m_iStatus == 1)              // 滚动条处于显示状态
				iWidth = iWidth - SCROLL_WIDTH + 1;      // 减去滚动条的宽度
		}

		int iLengthLimit = GetDisplayLimit_1( cTemp, iWidth);

		if( iTopItem+i != iCurSel )
		{
			pLCD->LCDTextOut_1( fb, w, h, m_x+2, m_y+LIST_ITEM_H*i+2, LCD_MODE_NORMAL, (unsigned char*)cTemp, iLengthLimit );
		}
		else
		{
			// 如果本控件处于焦点，则反白显示选中条目
			// 如果不是焦点，则用虚线框圈起选中条目
			if( (m_wStatus & TWND_STATUS_FOCUSED) > 0 )
			{
				pLCD->LCDFillRect( fb, w, h, m_x+2, m_y+LIST_ITEM_H*i+2, iWidth, HZK_H, 1 );
				pLCD->LCDTextOut_1( fb, w, h, m_x+2, m_y+LIST_ITEM_H*i+2, LCD_MODE_INVERSE, (unsigned char*)cTemp, iLengthLimit );
			}
			else
			{
				pLCD->LCDTextOut_1( fb, w, h, m_x+2, m_y+LIST_ITEM_H*i+2, LCD_MODE_NORMAL, (unsigned char*)cTemp, iLengthLimit );
				pLCD->LCDFillRect (fb,w,h,m_x+2,m_y+LIST_ITEM_H*i+1,iWidth+1,1,1);            // 上
				pLCD->LCDFillRect (fb,w,h,m_x+1,m_y+LIST_ITEM_H*i+1,1,HZK_H+2,1);             // 左
				pLCD->LCDFillRect (fb,w,h,m_x+2,m_y+LIST_ITEM_H*(i+1)+1,iWidth+1,1,1);        // 下
				pLCD->LCDFillRect (fb,w,h,m_x+iWidth+2,m_y+LIST_ITEM_H*i+1,1,HZK_H+2,1);      // 右
			}
		}
	}

	// 先调用基类的绘制函数(用于绘制滚动条)
	CTWindow::Paint(pLCD);
}

// 消息处理
int CTList::Proc(CTWindow *pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iCount			= m_iItemCount;
	int iTopItem		= m_iTopIndex;
	int iCurSel			= m_iCurSel;
	int iCanDisplayed	= m_iHeightOfLinage;

	int iReturn = 0;

	if (iMsg == TMSG_KEYDOWN)
	{
		switch (wParam)
		{
		case TVK_UP:
			{
				// 向上移动选项
				iCurSel -= 1;
				if( iCurSel < 0 )
					iCurSel = 0;

				SetCurSel (iCurSel);

				if( iTopItem > iCurSel )
					iTopItem = iCurSel;
				if( iTopItem+iCanDisplayed-1 < iCurSel )
					iTopItem = iCurSel-iCanDisplayed+1;

				m_iTopIndex = iTopItem;

				// 更新滚动条
				RenewScroll();
				iReturn = 1;
			}
			break;
		case TVK_DOWN:
			{
				// 向下移动选项
				iCurSel += 1;
				if( iCurSel > iCount-1 )
					iCurSel = iCount-1;
				if( iCurSel < 0 )
					iCurSel = 0;

				SetCurSel (iCurSel);

				if( iTopItem > iCurSel )
					iTopItem = iCurSel;
				if( iTopItem+iCanDisplayed-1 < iCurSel )
					iTopItem = iCurSel-iCanDisplayed+1;

				m_iTopIndex = iTopItem;

				// 更新滚动条
				RenewScroll();
				iReturn = 1;
			}
			break;
		case TVK_PAGE_UP:
			{
				// 向前翻页
				iCurSel -= iCanDisplayed;
				if( iCurSel < 0 )
					iCurSel = 0;

				SetCurSel (iCurSel);

				if( iTopItem > iCurSel )
					iTopItem = iCurSel;
				if( iTopItem+iCanDisplayed-1 < iCurSel )
					iTopItem = iCurSel-iCanDisplayed+1;

				m_iTopIndex = iTopItem;

				// 更新滚动条
				RenewScroll();
				iReturn = 1;
			}
			break;
		case TVK_PAGE_DOWN:
			{
				// 向后翻页
				// 向下移动选项
				iCurSel += iCanDisplayed;

				if( iCurSel > iCount-1 )
					iCurSel = iCount-1;
				if( iCurSel < 0 )
					iCurSel = 0;

				SetCurSel (iCurSel);

				if( iTopItem > iCurSel )
					iTopItem = iCurSel;
				if( iTopItem+iCanDisplayed-1 < iCurSel )
					iTopItem = iCurSel-iCanDisplayed+1;

				m_iTopIndex = iTopItem;

				// 更新滚动条
				RenewScroll();
				iReturn = 1;
			}
			break;
		case TVK_ENTER:			// 传送给输入法的确认键
			{
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
			}
			break;
		}
	}
	else if (iMsg == TMSG_SETFOCUS)
	{
		// 得到焦点，如果没有处于处于选择状态的条目应当将第一条设置为当前选项
		// 并且移动显示列表将当前选项放置在合适的位置
		if (pWnd == this)
		{
			if( iCurSel == -1 )
				iCurSel = 0;

			if( iTopItem > iCurSel )
				iTopItem = iCurSel;
			if( iTopItem+iCanDisplayed-1 < iCurSel )
				iTopItem = iCurSel-iCanDisplayed+1;

			m_iTopIndex = iTopItem;
			m_iCurSel = iCurSel;
			
			RenewScroll();
		}
	}

	return iReturn;
}

#if defined(MOUSE_SUPPORT)
// 坐标设备消息处理
int CTList::PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = CTWindow::PtProc (pWnd, iMsg, wParam, lParam);

	if (iReturn != 1)
	{
		if( iMsg == TMSG_LBUTTONDOWN )
		{
			if (PtInWindow (wParam, lParam) )
			{
				if( IsWindowEnabled() )
				{
					int x = wParam;
					int y = lParam;
					int iCurSel = PtInItems( x, y );
					if( iCurSel != -1 )
					{
						SetCurSel( iCurSel );
						RenewScroll();
					}
					iReturn = 1;
				}
			}
		}
	}

	return iReturn;
}

// 测试坐标落于的条目，-1表示未落于任何条目
int CTList::PtInItems( int x, int y )
{
	int iCount        = m_iItemCount;
	int iTopItem      = m_iTopIndex;
	int iCurSel       = m_iCurSel;
	int iCanDiaplayed = m_iHeightOfLinage;

	int iRight = m_x + m_w;
	if( (m_pVScroll != NULL) && ((m_wStyle & TWND_STYLE_NO_SCROLL)==0) )
	{
		if( m_pVScroll->m_iStatus == 1)    // 滚动条处于显示状态
			iRight -= SCROLL_WIDTH;        // 减去滚动条的宽度
	}

	int L = m_x+2;
	int T = m_y+1;
	int R = iRight-2;
	int B = T+LIST_ITEM_H*iCanDiaplayed;

	if( !PtInRect( x,y,L,T,R,B ) )
		return -1;

	B = T+LIST_ITEM_H;

	int i;
	for( i=0; i<iCanDiaplayed; i++)
	{
		if( iTopItem+i >= iCount )
			return -1;

		if( PtInRect( x,y,L,T,R,B ) )
			return (iTopItem + i);

		T += LIST_ITEM_H;
		B += LIST_ITEM_H;
	}

	return -1;
}
#endif // defined(MOUSE_SUPPORT)

// 获得条目的数量
int CTList::GetCount()
{
	return m_iItemCount;
}

// 获得当前显示区域最上面一个条目的Index
int CTList::GetTopIndex()
{
	return m_iTopIndex;
}

// 设置当前显示区域最上面一个条目的Index
int CTList::SetTopIndex(int iIndex)
{
	int iCount		= m_iItemCount;
	int iTopItem	= m_iTopIndex;

	// 进行数据合法性检查
	if( (iIndex < 0) || (iIndex > iCount-1) )
		return -1;

	int iOldTopIndex = iTopItem;
	// 设置列表框中最上面一项索引值
	m_iTopIndex = iIndex;

	// 刷新屏幕
	UpdateView(this);

	return iOldTopIndex;
}

// 得到当前选中项目的Index，如果没有选中的则返回-1
int CTList::GetCurSel()
{
	return m_iCurSel;
}

// 设置当前的选中项目
int CTList::SetCurSel( int iIndex )
{
	int iCount			= m_iItemCount;
	int iCurSel			= m_iCurSel;

	if( (iIndex < 0) || (iIndex > iCount-1 ) )
		return -1;

	// 移动焦点到index指定行的内容。。。
	int iOldIndex = iCurSel;
	m_iCurSel = iIndex;

	// 给父窗口发送TMSG_DATACHANGE消息
	_TMSG Msg;
	Msg.pWnd = m_pParent;
	Msg.message = TMSG_DATACHANGE;
	Msg.wParam = 0;
	Msg.lParam = 0;
	m_pApp->PostMessage(&Msg);

	return iOldIndex;
}

// 获得某一列表项的内容
BOOL CTList::GetString(int iIndex, char* pText)
{
	int iCount = m_iItemCount;

	if( (iIndex < 0) || (iIndex > iCount-1) )
		return FALSE;

	_LISTCONTENT* pNextItem = m_pContent;
	int i;
	for( i=0; i<iIndex; i++ )
	{
		if( pNextItem == NULL )
			return FALSE;
		else
			pNextItem = pNextItem->next;
	}

	// 拷贝iIndex所指定列表项的内容
	if( pNextItem != NULL )
	{
		strncpy( pText, pNextItem->text, LIST_TEXT_MAX_LENGTH-1 );
		return TRUE;
	}
	
	return FALSE;
}

// 设置某一列表项的内容
BOOL CTList::SetString(int iIndex, char* pText)
{
	int iCount = m_iItemCount;

	if( (iIndex < 0) || (iIndex > iCount-1) )
		return FALSE;

	_LISTCONTENT* pNextItem = m_pContent;
	int i;
	for( i=0; i<iIndex; i++ )
	{
		if( pNextItem == NULL )
			return FALSE;
		else
			pNextItem = pNextItem->next;
	}

	// 设置iIndex所指定列表项的内容
	if( pNextItem != NULL )
	{
		memset( pNextItem->text, 0x0, LIST_TEXT_MAX_LENGTH );
		strncpy( pNextItem->text, pText, LIST_TEXT_MAX_LENGTH-1 );
		return TRUE;
	}
	
	return FALSE;
}

// 获得某一列表项内容的长度
int CTList::GetStringLength( int iIndex )
{
	int iCount = m_iItemCount;

	if( (iIndex < 0) || (iIndex > iCount-1) )
		return FALSE;

	_LISTCONTENT* pNextItem = m_pContent;
	int i;
	for( i=0; i<iIndex; i++ )
	{
		if( pNextItem == NULL )
			return FALSE;
		else
			pNextItem = pNextItem->next;
	}

	// 测定iIndex所指定列表项内容的长度
	int iLength = 0;
	if( pNextItem != NULL )
		iLength = strlen( pNextItem->text );

	return iLength;
}

// 向列表框中添加一个串(加在末尾)
BOOL CTList::AddString(char* pText)
{
	int iCount = m_iItemCount;

	// 创建新的节点
	_LISTCONTENT* theNewOne = new _LISTCONTENT;
	strncpy( theNewOne->text, pText, LIST_TEXT_MAX_LENGTH-1);
	theNewOne->next = NULL;

	if (iCount == 0)
	{
		m_pContent = theNewOne;
		m_iItemCount ++;
		return TRUE;
	}

	// 查找最末一条列表项
	_LISTCONTENT* theNext = m_pContent;
	int i;
	for( i=0; i<iCount-1; i++ )
	{
		if( theNext->next == NULL )
			return FALSE;

		theNext = theNext->next;
	}

	// 最后一条的next指针必须为NULL，否则说明其他程序存在错误
	if( theNext->next != NULL )
		return FALSE;

	theNext->next = theNewOne;
	m_iItemCount ++;

	// 更新滚动条
	RenewScroll();
	return TRUE;
}

// 删除一个列表项
BOOL CTList::DeleteString(int iIndex)
{
	int iCount  = m_iItemCount;
	int iCurSel = m_iCurSel;

	if( (iIndex < 0) || (iIndex > iCount-1) )
		return FALSE;

	if( iCount == 1)
	{
		delete m_pContent;
		m_pContent = NULL;
		iCount --;
		if( iCurSel == iIndex )
			iCurSel = -1;

		// 更新
		m_iItemCount = iCount;
		m_iCurSel = iCurSel;
		RenewScroll();
		return TRUE;
	}

	if( iIndex == 0 )
	{
		_LISTCONTENT* pNextItem = m_pContent->next;
		delete m_pContent;
		m_pContent = pNextItem;
		iCount --;
		if( iCount == 0 )
			iCurSel = -1;

		// 更新
		m_iItemCount = iCount;
		m_iCurSel = iCurSel;		
		RenewScroll();
		return TRUE;
	}

	_LISTCONTENT* pNextItem = m_pContent;
	_LISTCONTENT* pLastItem = m_pContent;	// 用于存放要删除节点的上一个节点

	int i;
	for( i=0; i<iIndex; i++ )
	{
		if( pNextItem == NULL )
			return FALSE;
		else
		{
			pLastItem = pNextItem;
			pNextItem = pNextItem->next;
		}
	}

	// 删除iIndex所指定的列表项
	if( pNextItem != NULL )
	{
		pLastItem->next = pNextItem->next;
		delete pNextItem;
		iCount --;
		if( iCurSel >= iCount )
			iCurSel = (iCount -1);

		// 更新
		int iTopItem      = m_iTopIndex;
		int iCanDisplayed = m_iHeightOfLinage;

		if( iTopItem+iCanDisplayed > iCount )
		    iTopItem = iCount-iCanDisplayed;
		if( iTopItem < 0 )
		    iTopItem = 0;

		m_iItemCount = iCount;
		m_iTopIndex = iTopItem;
		m_iCurSel = iCurSel;
		RenewScroll();
		return TRUE;
	}

	return FALSE;
}

// 在指定位置插入一个串
BOOL CTList::InsertString(int iIndex, char* pText)
{
	int iCount			= m_iItemCount;
	int iCurSel			= m_iCurSel;

	if( iIndex < 0 )
		return FALSE;

	if( (iIndex > 0) && (iIndex > iCount-1) )
		iIndex = iCount-1;

	// 创建新的节点
	_LISTCONTENT* theNewOne = new _LISTCONTENT;
	strncpy( theNewOne->text, pText, LIST_TEXT_MAX_LENGTH-1);
	theNewOne->next = NULL;

	if( iCount == 0)
	{
		m_pContent = theNewOne;
		m_iItemCount ++;

		// 更新滚动条
		RenewScroll();
		return TRUE;
	}

	if( iIndex == 0 )
	{
		theNewOne->next = m_pContent;
		m_pContent = theNewOne;
		m_iItemCount ++;
		if( iCurSel == 0 )
			m_iCurSel ++;

		// 更新滚动条
		RenewScroll();
		return TRUE;
	}

	_LISTCONTENT* pNextItem = m_pContent;
	_LISTCONTENT* pLastItem = m_pContent;	// 用于存放要插入节点的上一个节点

	int i;
	for( i=0; i<iIndex; i++ )
	{
		if( pNextItem == NULL )
			return FALSE;
		else
		{
			pLastItem = pNextItem;
			pNextItem = pNextItem->next;
		}
	}

	// 在iIndex位置插入新的列表项
	if( pNextItem != NULL )
	{
		pLastItem->next = theNewOne;
		theNewOne->next = pNextItem;
		m_iItemCount ++;
		if( iCurSel == iIndex )
			m_iCurSel ++;

		// 更新滚动条
		RenewScroll();
		return TRUE;
	}
	
	return FALSE;
}

// 删除所有列表项
BOOL CTList::RemoveAll()
{
	int iCount	= m_iItemCount;

	_LISTCONTENT* theNext = NULL;
	int i;
	for (i=0; i<iCount; i++)
	{
		if (m_pContent == NULL)
		{
			m_iItemCount = 0;
			m_iTopIndex = 0;
			m_iCurSel = -1;
			// 更新滚动条
			RenewScroll();
			return FALSE;
		}

		theNext = m_pContent->next;
		if (m_pContent != NULL )
		{
			delete m_pContent;
		}
		m_pContent = theNext;
	}

	m_iItemCount = 0;
	m_iTopIndex = 0;
	m_iCurSel = -1;
	// 更新滚动条
	RenewScroll();
	return TRUE;
}

// 在列表项中查找一个串
int CTList::FindString(char* pText)
{
	int iCount = m_iItemCount;

	// 循环查找所有列表项
	_LISTCONTENT* theNext = m_pContent;
	int i;
	for( i=0; i<iCount; i++ )
	{
		if( strcmp(theNext->text, pText) == 0 )
			return i;

		if( theNext->next == NULL )
			return -1;

		theNext = theNext->next;
	}
	return -1;
}

// 在列表项中查找一个串，如果找到，则将它设置为选中，并显示在第一行
// (如果在最后一页，则令最后一行显示在列表框的最底端)
int CTList::SelectString(char* pText)
{
	int iCount          = m_iItemCount;
	int iTopItem		= m_iTopIndex;
	int iCanDisplayed	= m_iHeightOfLinage;

	// 从链表中查找指定的字符串
	int iIndex = FindString(pText);
	if( iIndex == -1 )	// 如果没有找到
		return -1;

	SetCurSel( iIndex );

	int iCurSel = m_iCurSel;
	
	if( iTopItem > iCurSel )
		iTopItem = iCurSel;

	if( iTopItem+iCanDisplayed-1 < iCurSel )
		iTopItem = iCurSel-iCanDisplayed+1;

	m_iTopIndex = iTopItem;

	return iIndex;
}

// 调整列表框的高度为整行
BOOL CTList::SetLinage(int iLinage)
{
	if( iLinage < 1 )
		return FALSE;

	m_iHeightOfLinage = iLinage;
	m_h = iLinage * ( HZK_H + 1 ) + 3;
	RenewScroll();
	return TRUE;
}

// 更新滚动条
void CTList::RenewScroll()
{
	if((m_wStyle & TWND_STYLE_NO_SCROLL) > 0)
		return;
		
	int iCount          = m_iItemCount;
	int iTopItem		= m_iTopIndex;
	int iCanDisplayed	= m_iHeightOfLinage;

	if( iCount > iCanDisplayed )
	{
		// 显示滚动条
		SetScrollBar( 1, iCount, iCanDisplayed, iTopItem );
		ShowScrollBar( 1, TRUE );
	}
	else
	{
		// 隐藏滚动条
		ShowScrollBar( 1, FALSE );
	}
	UpdateView( this );
}

#if defined(MOUSE_SUPPORT)
// 向上
void CTList::OnScrollUp ()
{
	int iCount			= m_iItemCount;
	int iTopItem		= m_iTopIndex;
	int iCurSel			= m_iCurSel;
	int iCanDisplayed	= m_iHeightOfLinage;

	if ( iTopItem > 0)
		iTopItem --;

	m_iTopIndex = iTopItem;
	// 更新滚动条
	RenewScroll();
}

// 向下
void CTList::OnScrollDown ()
{
	int iCount			= m_iItemCount;
	int iTopItem		= m_iTopIndex;
	int iCurSel			= m_iCurSel;
	int iCanDisplayed	= m_iHeightOfLinage;

	if (iTopItem + iCanDisplayed < iCount)
		iTopItem ++;

	m_iTopIndex = iTopItem;
	// 更新滚动条
	RenewScroll();
}

// 向上翻页
void CTList::OnScrollPageUp ()
{
	int iCount			= m_iItemCount;
	int iTopItem		= m_iTopIndex;
	int iCurSel			= m_iCurSel;
	int iCanDisplayed	= m_iHeightOfLinage;

	iTopItem -= iCanDisplayed;
	if (iTopItem < 0)
		iTopItem = 0;

	m_iTopIndex = iTopItem;
	// 更新滚动条
	RenewScroll();
}

// 向下翻页
void CTList::OnScrollPageDown ()
{
	int iCount			= m_iItemCount;
	int iTopItem		= m_iTopIndex;
	int iCurSel			= m_iCurSel;
	int iCanDisplayed	= m_iHeightOfLinage;

	iTopItem += iCanDisplayed;
	if (iTopItem + iCanDisplayed >= iCount)
		iTopItem = iCount - iCanDisplayed;

	m_iTopIndex = iTopItem;
	// 更新滚动条
	RenewScroll();
}

// 滚动条拖拽
void CTList::OnVScrollNewPos (int iNewPos)
{
	// 修改TopItem的值
	m_iTopIndex = iNewPos;

	// 更新滚动条
	RenewScroll();
}
#endif // defined(MOUSE_SUPPORT)
/* END */
