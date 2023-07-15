// TCombo.cpp: implementation of the CTCombo class.
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTCombo::CTCombo()
{
	m_dwExtStatus = 0;
	m_iDropDownStatus = 0;
	m_pEdit = new CTEdit();
	m_pList = new CTList();
}

CTCombo::~CTCombo()
{
	// 由于Edit和List都是自己的子窗口，CTWindow将按照默认的规则进行销毁
	// 如果Edit和List没有经过Create挂载，则需要删除
	if( m_pEdit->m_pParent == NULL )
		delete m_pEdit;
	if( m_pList->m_pParent == NULL )
		delete m_pList;
}

// 创建组合框
BOOL CTCombo::Create( CTWindow* pParent,WORD wWndType,WORD wStyle,WORD wStatus,int x,int y,int w,int h,int ID )
{
	m_pEdit->Create( NULL, TWND_TYPE_EDIT, wStyle, TWND_STATUS_NORMAL, x, y, w-COMBO_PUBHDOWN_BUTTON_WIDTH-1, h, COMBO_ID_EDIT );
	m_pEdit->m_pApp = pParent->m_pApp;

	// List的位置要重新计算一下，看看向下弹还是向上弹
	int theY = y + h - 1;
	if( theY + COMBO_DEFAULT_LIST_HEIGHT > SCREEN_H )
		theY = y - COMBO_DEFAULT_LIST_HEIGHT + 1;

	m_pList->Create( NULL, TWND_TYPE_LIST, TWND_STYLE_SOLID, TWND_STATUS_NORMAL, x, theY, w, COMBO_DEFAULT_LIST_HEIGHT, COMBO_ID_LIST );
	m_pList->m_pApp = pParent->m_pApp;

	if (!CTWindow::Create (pParent,TWND_TYPE_COMBO,wStyle,wStatus,x,y,w,h,ID))
		return FALSE;

	return TRUE;
}

// 绘制组合框
void CTCombo::Paint( CLCD* pLCD )
{
	// 如果不可见，则什么也不绘制
	if( !IsWindowVisible() )
		return;

	m_pEdit->Paint( pLCD );

	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// 画出右侧的向下箭头及方框
	pLCD->LCDFillRect (fb, w, h, m_x+m_w-COMBO_PUBHDOWN_BUTTON_WIDTH-1, m_y, COMBO_PUBHDOWN_BUTTON_WIDTH, m_h, 0);
	pLCD->LCDHLine( fb, w, h, m_x+m_w-COMBO_PUBHDOWN_BUTTON_WIDTH-1, m_y, COMBO_PUBHDOWN_BUTTON_WIDTH, 1 );
	pLCD->LCDHLine( fb, w, h, m_x+m_w-COMBO_PUBHDOWN_BUTTON_WIDTH-1, m_y+m_h-1, COMBO_PUBHDOWN_BUTTON_WIDTH, 1 );
	pLCD->LCDVLine( fb, w, h, m_x+m_w-1, m_y, m_h, 1 );

	// 如果下拉列表没有打开，则绘制实心的箭头，否则绘制空心的箭头
	if( m_iDropDownStatus == COMBO_UN_DROPPED )
		pLCD->LCDDrawImage( fb, w, h, m_x+m_w-10, m_y+m_h-9, 7, 7, LCD_MODE_NORMAL, pBitmap_Arror_Down, 7, 7, 0, 0 );
	else
		pLCD->LCDDrawImage( fb, w, h, m_x+m_w-10, m_y+m_h-9, 7, 7, LCD_MODE_NORMAL, pBitmap_Hollow_Arror_Down, 7, 7, 0, 0 );
}

// 组合框消息处理
int CTCombo::Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam )
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = 0;

	if (iMsg == TMSG_KEYDOWN)
	{
		switch (wParam)
		{
		case TVK_ESCAPE:
			{
				// 如果List处于打开状态，则关闭List，并修改iReturn为1
				if( m_iDropDownStatus == COMBO_DROPPED )
				{
					ShowDropDown( FALSE );
					iReturn = 1;
				}
				// 如果List未打开，则交给Edit去处理(输入法需要响应该消息)
				else
				{
					if( CanEdit() )
					{
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );

						if( m_pEdit->IsIMEOpen() )
					                iReturn = 1;
					}
				}
			}
			break;
		case TVK_UP:
			{
				// 如果List处于打开状态，则将此消息交给List处理
				if( m_iDropDownStatus == COMBO_DROPPED )
				{
					iReturn = m_pList->Proc( pWnd, iMsg, wParam, lParam );
				}
				// 如果List未打开，则交给Edit去处理(输入法需要响应该消息)
				else
				{
					if( CanEdit() )
						iReturn = m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
					// 如果Edit未处理，则根据List更新Edit
					if( iReturn == 0 )
					{
						iReturn = m_pList->Proc( pWnd, iMsg, wParam, lParam );
						SyncString();
					}
				}
			}
			break;
		case TVK_DOWN:
			{
				// 如果List处于打开状态，则将此消息交给List处理
				if( m_iDropDownStatus == COMBO_DROPPED )
				{
					iReturn = m_pList->Proc( pWnd, iMsg, wParam, lParam );
				}
				// 如果List未打开，则交给Edit去处理(输入法需要响应该消息)
				else
				{
					if( CanEdit() )
						iReturn = m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
					// 如果Edit未处理，则根据List更新Edit
					if( iReturn == 0 )
					{
						iReturn = m_pList->Proc( pWnd, iMsg, wParam, lParam );
						SyncString();
					}
				}
			}
			break;
		case TVK_LEFT:
			{
				// 如果List处于关闭状态，则将此消息交给Edit处理
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						iReturn = m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
			}
			break;
		case TVK_RIGHT:
			{
				// 如果List处于关闭状态，则将此消息交给Edit处理
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						iReturn = m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
			}
			break;
		case TVK_HOME:
			{
				// 如果List处于关闭状态，则将此消息交给Edit处理
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
			}
			break;
		case TVK_END:
			{
				// 如果List处于关闭状态，则将此消息交给Edit处理
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
			}
			break;
		case TVK_BACK_SPACE:	// 删除当前插入位置前面的字符
			{
				// 如果List处于关闭状态，则将此消息交给Edit处理
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
			}
			break;
		case TVK_DELETE:		// 删除当前插入位置后面的字符
			{
				// 如果List处于关闭状态，则将此消息交给Edit处理
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
			}
			break;
		case TVK_F11:		// 打开关闭输入法
			{
				// 如果List处于关闭状态，则将此消息交给Edit处理
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
			}
			break;
		case TVK_F12:		// 切换输入法
			{
				// 如果List处于关闭状态，则将此消息交给Edit处理
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
			}
			break;
		case TVK_ENTER:			// 传送给输入法的确认键
			{
				// 如果List处于关闭状态，则将此消息交给Edit处理
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						iReturn = m_pEdit->Proc( pWnd, iMsg, wParam, lParam );

					if( iReturn == 0 )
					{
						// 如果Edit没有处理，则打开列表框
						if( ShowDropDown( TRUE ) )
							iReturn = 1;
					}
				}
				else
				{
					// 根据列表框的内容更新编辑框，并关闭列表框
					SyncString();
					ShowDropDown( FALSE );
					iReturn = 1;
				}
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
				// 如果List处于关闭状态，则将此消息交给Edit处理
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
			}
			break;
		default:
			{

#if defined(CHINESE_SUPPORT)
				// 如果List处于关闭状态，则将此消息交给Edit处理
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
#endif // defined(CHINESE_SUPPORT)

			}
		}
	}
	else if (iMsg == TMSG_SETFOCUS)
	{
		// 得到焦点，首先判断是否应自动弹出下拉列表
		if (pWnd == this)
		{
			if( (m_wStyle & TWND_STYLE_AUTO_DROPDOWN) > 0 )
			{
				// 弹出下拉菜单
				ShowDropDown( TRUE );
			}
			else
			{
				EditSetFocus();
			}
		}
	}
	else if (iMsg == TMSG_KILLFOCUS)
	{
		// 失去焦点，关闭List
		if (pWnd == this)
			ShowDropDown( FALSE );

		// 修改Edit和List的status，失去焦点
		EditKillFocus();
		ListKillFocus();
	}

	UpdateView (this);
	return iReturn;
}

#if defined(MOUSE_SUPPORT)
// 坐标设备消息处理
int CTCombo::PtProc(CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = CTWindow::PtProc (pWnd, iMsg, wParam, lParam);

	if( iMsg == TMSG_LBUTTONDOWN )
	{
		int x = wParam;
		int y = lParam;
		if( PtInWindow( wParam, lParam ))
		{
			if( m_pEdit->PtInWindow( x, y ) )
			{
				// 编辑框处理
				iReturn = m_pEdit->PtProc( pWnd, iMsg, wParam, lParam );
				if( m_iDropDownStatus == COMBO_DROPPED )
					ShowDropDown( FALSE );
			}
			else
			{
				// 弹出\关闭下拉列表框
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
					ShowDropDown( TRUE );
				else
					ShowDropDown( FALSE );
			}

			iReturn = 1;
		}
		else if( m_iDropDownStatus == COMBO_DROPPED )
		{
			// 下拉列表框处理
			iReturn = m_pList->PtProc( pWnd, iMsg, wParam, lParam );
			if( iReturn == 1 )
			{
				if( m_pList->PtInItems( x, y ) != -1 )
				{
					// 鼠标选中列表区关闭下拉列表
					SyncString();
					ShowDropDown( FALSE );
					iReturn = 1;
				}
			}
			else
				ShowDropDown( FALSE );
		}
	}
	else if((iMsg == TMSG_LBUTTONUP) ||
			(iMsg == TMSG_MOUSEMOVE))
	{
		iReturn = m_pEdit->PtProc( pWnd, iMsg, wParam, lParam );
		if( iReturn != 1 )
			iReturn = m_pList->PtProc( pWnd, iMsg, wParam, lParam );
	}

	return iReturn;
}
#endif // defined(MOUSE_SUPPORT)

// 显示或者隐藏下拉列表框
BOOL CTCombo::ShowDropDown( BOOL bShow )
{
	// 如果当前设置禁止弹出下拉列表框，则返回FALSE
	if( !CanDropDown() )
	{
		if( m_iDropDownStatus == COMBO_DROPPED )
		{
			// 隐藏下拉列表
			m_pList->m_wStatus |= TWND_STATUS_INVISIBLE;
			m_pApp->SetTopMost( NULL );
			m_iDropDownStatus = COMBO_UN_DROPPED;
		}
		return FALSE;
	}
	else
	{
		if( bShow )
		{
			// 显示下拉列表
			m_pList->m_wStatus &= ~TWND_STATUS_INVISIBLE;
			m_pApp->SetTopMost( m_pList );
			m_iDropDownStatus = COMBO_DROPPED;
			// 将下拉菜单设置为焦点
			ListSetFocus();
		}
		else
		{
			// 隐藏下拉列表
			m_pList->m_wStatus |= TWND_STATUS_INVISIBLE;
			m_pApp->SetTopMost( NULL );
			m_iDropDownStatus = COMBO_UN_DROPPED;
			// 使下拉菜单失去焦点
			ListKillFocus();
		}
	}

	return TRUE;
}

// 得到下拉列表框的显示状态
BOOL CTCombo::GetDroppedState()
{
	if( m_iDropDownStatus == COMBO_DROPPED )
		return TRUE;
	else
		return FALSE;
}

// 设置下拉列表框的高度(以行数计)
BOOL CTCombo::SetDroppedLinage( int iLinage )
{
	if( iLinage < 1 )
		return FALSE;

	// 设置下拉列表框可以显示出来的行数
	m_pList->m_iHeightOfLinage = iLinage;

	// 设置下拉列表框的高度
	int theH = ( HZK_H + 1 ) * iLinage + 3;

	// List的位置要重新计算一下，看看向下弹还是向上弹
	int theY = m_y + m_h - 1;
	if( theY + theH > SCREEN_H )
		theY = m_y - theH + 1;

	// 更新List的窗口大小
	m_pList->SetPosition( m_pList->m_x, theY, m_pList->m_w, theH );

	// 更新List的滚动条
	m_pList->RenewScroll();

	return TRUE;
}

// 允许或者禁止对编辑框输入文字
BOOL CTCombo::EnableEdit( BOOL bEnable )
{
	if( bEnable )
		m_dwExtStatus &= ~COMBO_DISABLE_EDIT;
	else
		m_dwExtStatus |= COMBO_DISABLE_EDIT;

	return TRUE;
}

// 允许或者禁止下拉菜单弹出
BOOL CTCombo::EnableDropDown( BOOL bEnable )
{
	if( bEnable )
		m_dwExtStatus &= ~COMBO_DISABLE_DROPDOWN;
	else
		m_dwExtStatus |= COMBO_DISABLE_DROPDOWN;

	return TRUE;
}

// 以下函数用于操作编辑框
// 限制文字的长度
int CTCombo::LimitText( int iLength )
{
	return m_pEdit->LimitText( iLength );
}

// 清空编辑框
BOOL CTCombo::Clean()
{
	return m_pEdit->Clean();
}

// 取得编辑框的文字
BOOL CTCombo::GetText( char* pText )
{
	return m_pEdit->GetText( pText );
}

// 设置编辑框的文字
BOOL CTCombo::SetText( char* pText, int iLength )
{
	return m_pEdit->SetText( pText, iLength );
}

// 取得编辑框文字的长度
int CTCombo::GetTextLength()
{
	return m_pEdit->GetTextLength();
}

// 以下函数用于操作下拉列表框
// 得到列表的条目数
int CTCombo::GetCount()
{
	return m_pList->GetCount();
}

// 得到列表框当前选中项目的Index，如果没有选中的则返回-1
int CTCombo::GetCurSel()
{
	return m_pList->GetCurSel();
}

// 设置列表框当前的选中项目
int CTCombo::SetCurSel( int iIndex )
{
	return m_pList->SetCurSel( iIndex );
}

// 获得某一列表项的内容
BOOL CTCombo::GetString( int iIndex, char* pText )
{
	return m_pList->GetString( iIndex, pText );
}

// 设置某一列表项的内容
BOOL CTCombo::SetString( int iIndex, char* pText )
{
	return m_pList->SetString( iIndex, pText );
}

// 获得某一列表项内容的长度
int CTCombo::GetStringLength( int iIndex )
{
	return m_pList->GetStringLength( iIndex );
}

// 向列表框中添加一个串(加在末尾)
BOOL CTCombo::AddString( char* pText )
{
	return m_pList->AddString( pText );
}

// 删除列表框的一个列表项
BOOL CTCombo::DeleteString( int iIndex )
{
	return m_pList->DeleteString( iIndex );
}

// 在列表框的指定位置插入一个串
BOOL CTCombo::InsertString( int iIndex, char* pText )
{
	return m_pList->InsertString( iIndex, pText );
}

// 删除列表框的所有列表项
BOOL CTCombo::RemoveAll()
{
	return m_pList->RemoveAll();
}

// 在列表框中查找一个串
int CTCombo::FindString( char* pText )
{
	return m_pList->FindString( pText );
}

// 根据列表框的内容更新编辑框
void CTCombo::SelectString( char* pText )
{
	int iIndex = m_pList->SelectString( pText );
	if( iIndex != -1 )
		SetText( pText, 255 );
}

// Edit得到焦点
void CTCombo::EditSetFocus()
{
	m_pEdit->m_wStatus |= TWND_STATUS_FOCUSED;

	_TMSG msg;
	msg.pWnd = m_pEdit;
	msg.message = TMSG_SETFOCUS;
	msg.wParam = 0;
	msg.lParam = 0;
	m_pApp->PostMessage( &msg );
}

// Edit失去焦点
void CTCombo::EditKillFocus()
{
	m_pEdit->m_wStatus &= ~TWND_STATUS_FOCUSED;
	_TMSG msg;
	msg.pWnd = m_pEdit;
	msg.message = TMSG_KILLFOCUS;
	msg.wParam = 0;
	msg.lParam = 0;
	m_pApp->SendMessage( &msg );
}

// List得到焦点
void CTCombo::ListSetFocus()
{
	m_pList->m_wStatus |= TWND_STATUS_FOCUSED;
	_TMSG msg;
	msg.pWnd = m_pList;
	msg.message = TMSG_SETFOCUS;
	msg.wParam = 0;
	msg.lParam = 0;
	m_pApp->PostMessage( &msg );
}

// List失去焦点
void CTCombo::ListKillFocus()
{
	m_pList->m_wStatus &= ~TWND_STATUS_FOCUSED;
	_TMSG msg;
	msg.pWnd = m_pList;
	msg.message = TMSG_KILLFOCUS;
	msg.wParam = 0;
	msg.lParam = 0;
	m_pApp->SendMessage( &msg );
}

// 将List当前的字符串拷贝到Edit中
void CTCombo::SyncString()
{
	char cText [256];
	memset( cText, 0x0, 256 );
	int iIndex = m_pList->GetCurSel();
	if( iIndex != -1 )
	{
		GetString( iIndex, cText );
		SetText( cText, 255 );
		
		// 如果本控件是父窗口的焦点，则设置Caret
		if (m_pParent != NULL)
		{
			CTWindow* pParentActive = m_pParent->m_pActive;
			if( pParentActive == this )
				m_pEdit->SetSel( 0, 255 );
		}
	}
}

// 判断是否可以向编辑框输入
BOOL CTCombo::CanEdit()
{
	if( (m_dwExtStatus & COMBO_DISABLE_EDIT) == 0 )
		return TRUE;
	else
		return FALSE;
}

// 判断是否可以弹出列表框
BOOL CTCombo::CanDropDown()
{
	if( (m_dwExtStatus & COMBO_DISABLE_DROPDOWN) == 0 )
		return TRUE;
	else
		return FALSE;
}
/* END */
