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

// �麯�������ư�ť
void CTCheckBox::Paint (CLCD* pLCD)
{
	// ������ɼ�����ʲôҲ������
	if( !IsWindowVisible() )
		return;

	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// �������ֵľ�����ʾλ��
	int ty = m_y + (m_h - 12) / 2;

	// ��䱳��
	pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 0);

	// ����ѡ��״̬����ѡ���
	if( m_iCheckState == 0 )
		pLCD->LCDDrawImage( fb,w,h,m_x+3,ty-1,13,13,0,pBitmap_Check_Box_Unselected,13,13,0,0);
	else if( m_iCheckState == 1 )
		pLCD->LCDDrawImage( fb,w,h,m_x+3,ty-1,13,13,0,pBitmap_Check_Box_Selected,13,13,0,0);

	// �������ֵ��޶����Ȳ���������
	int iTextLength = GetDisplayLimit( m_cCaption, m_w-17 );
	pLCD->LCDTextOut( fb,w,h,m_x+17,ty,0,(unsigned char*)m_cCaption,iTextLength );

	// ����״̬�������ߵ����
	if ((m_wStatus & TWND_STATUS_FOCUSED) > 0)
	{
		pLCD->LCDFillRect (fb,w,h,m_x,m_y,m_w,1,2);
		pLCD->LCDFillRect (fb,w,h,m_x,m_y,1,m_h,2);
		pLCD->LCDFillRect (fb,w,h,m_x,m_y+m_h,m_w,1,2);
		pLCD->LCDFillRect (fb,w,h,m_x+m_w,m_y,1,m_h,2);
	}
}

// �麯������Ϣ����
// ��Ϣ������ˣ�����1��δ������0
int CTCheckBox::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = 0;

	// ֻ����������Ϣ��1���ո����2���س�����3��TMSG_PUSHDOWN��Ϣ
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
// �����豸��Ϣ����
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

			// ����ѡ�񷽿��λ��
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

// ����ѡ��״̬
BOOL CTCheckBox::SetCheck( int nCheck )
{
	if( (m_wStatus & TWND_STATUS_INVISIBLE) != 0 )
		return FALSE;

	if( (nCheck < 0) || (nCheck > 1) )
		return FALSE;

	m_iCheckState = nCheck;
	return TRUE;
}

// �õ�ѡ��״̬
int CTCheckBox::GetCheck()
{
	return m_iCheckState;
}

// ���������µĴ���
void CTCheckBox::OnCheck ()
{
	DebugPrintf( "OnCheck \n" );

	if( (m_wStatus & TWND_STATUS_INVISIBLE) != 0 )
		return;

	// ˢ����ʾ���򸸴��ڷ���֪ͨ��Ϣ���ٴ�ˢ����ʾ
	if( m_iCheckState == 1 )
		m_iCheckState = 0;
	else
		m_iCheckState = 1;

	Paint(m_pApp->m_pLCD);

	// �򸸴��ڷ�����ϢֵΪ��ID����Ϣ
	_TMSG msg;
	msg.pWnd    = m_pParent;
	msg.message = TMSG_NOTIFY_PARENT;
	msg.wParam  = m_ID;
	msg.lParam  = m_iCheckState;
	m_pApp->PostMessage (&msg);

	m_pApp->m_pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );
}
/* END */
