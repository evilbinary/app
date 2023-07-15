// TCheckBox.cpp: implementation of the CTCheckBox class.
//
//////////////////////////////////////////////////////////////////////
#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTCheckBox::CTCheckBox ()
{
	m_iCheckState = 0;
}

CTCheckBox::~CTCheckBox ()
{
}

BOOL CTCheckBox::Create( CTWindow* pParent,WORD wWndType,WORD wStyle,WORD wStatus,int x,int y,int w,int h,int ID)
{
	if( !CTWindow::Create(pParent,TWND_TYPE_CHECK_BOX,wStyle,wStatus,x,y,w,h,ID))
		return FALSE;

	return TRUE;
}

// 虚函数，绘制按钮
void CTCheckBox::Paint (CLCD* pLCD)
{
	// 如果不可见，则什么也不绘制
	if( !IsWindowVisible() )
		return;

	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// 计算文字的居中显示位置
	int ty = m_y + (m_h - 12) / 2;

	// 填充背景
	pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 0);

	// 根据选择状态绘制选择块
	if( m_iCheckState == 0 )
		pLCD->LCDDrawImage( fb,w,h,m_x+3,ty-1,13,13,0,pBitmap_Check_Box_Unselected,13,13,0,0);
	else if( m_iCheckState == 1 )
		pLCD->LCDDrawImage( fb,w,h,m_x+3,ty-1,13,13,0,pBitmap_Check_Box_Selected,13,13,0,0);

	// 计算文字的限定长度并绘制文字
	int iTextLength = GetDisplayLimit( m_cCaption, m_w-17 );
	pLCD->LCDTextOut( fb,w,h,m_x+17,ty,0,(unsigned char*)m_cCaption,iTextLength );

	// 焦点状态绘制虚线的外框
	if ((m_wStatus & TWND_STATUS_FOCUSED) > 0)
	{
		pLCD->LCDFillRect (fb,w,h,m_x,m_y,m_w,1,2);
		pLCD->LCDFillRect (fb,w,h,m_x,m_y,1,m_h,2);
		pLCD->LCDFillRect (fb,w,h,m_x,m_y+m_h,m_w,1,2);
		pLCD->LCDFillRect (fb,w,h,m_x+m_w,m_y,1,m_h,2);
	}
}

// 虚函数，消息处理
// 消息处理过了，返回1，未处理返回0
int CTCheckBox::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = 0;

	// 只处理三种消息：1、空格键；2、回车键；3、TMSG_PUSHDOWN消息
	if (iMsg == TMSG_KEYDOWN)
	{
		if( (wParam == TVK_SPACE) || (wParam == TVK_ENTER) )
		{
			if ((m_wStatus & TWND_STATUS_FOCUSED) > 0)
			{
				OnCheck ();
			}
			iReturn = 1;
		}
	}
	else if (iMsg == TMSG_PUSHDOWN)
	{
		if (pWnd == this)
		{
			OnCheck ();
		}
		iReturn = 1;
	}

	return iReturn;
}

#if defined(MOUSE_SUPPORT)
// 坐标设备消息处理
int CTCheckBox::PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = CTWindow::PtProc (pWnd, iMsg, wParam, lParam);

	if (iMsg == TMSG_LBUTTONDOWN)
	{
		if (PtInWindow (wParam, lParam) )
		{
			iReturn = 1;

			// 计算选择方块的位置
			int left   = m_x  + 3;
			int top    = m_y  + (m_h - 12) / 2 - 1;
			int right  = left + 13;
			int bottom = top  + 13;
			if( PtInRect( wParam, lParam, left, top, right, bottom ) )
				OnCheck();
		}
	}

	return iReturn;
}
#endif // defined(MOUSE_SUPPORT)

// 设置选择状态
BOOL CTCheckBox::SetCheck( int nCheck )
{
	if( (m_wStatus & TWND_STATUS_INVISIBLE) != 0 )
		return FALSE;

	if( (nCheck < 0) || (nCheck > 1) )
		return FALSE;

	m_iCheckState = nCheck;
	return TRUE;
}

// 得到选择状态
int CTCheckBox::GetCheck()
{
	return m_iCheckState;
}

// 按键被按下的处理
void CTCheckBox::OnCheck ()
{
	DebugPrintf( "OnCheck \n" );

	if( (m_wStatus & TWND_STATUS_INVISIBLE) != 0 )
		return;

	// 刷新显示，向父窗口发出通知消息，再次刷新显示
	if( m_iCheckState == 1 )
		m_iCheckState = 0;
	else
		m_iCheckState = 1;

	Paint(m_pApp->m_pLCD);

	// 向父窗口发送消息值为本ID的消息
	_TMSG msg;
	msg.pWnd    = m_pParent;
	msg.message = TMSG_NOTIFY_PARENT;
	msg.wParam  = m_ID;
	msg.lParam  = m_iCheckState;
	m_pApp->PostMessage (&msg);

	m_pApp->m_pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );
}
/* END */
