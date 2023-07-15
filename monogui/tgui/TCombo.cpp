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
	// ����Edit��List�����Լ����Ӵ��ڣ�CTWindow������Ĭ�ϵĹ����������
	// ���Edit��Listû�о���Create���أ�����Ҫɾ��
	if( m_pEdit->m_pParent == NULL )
		delete m_pEdit;
	if( m_pList->m_pParent == NULL )
		delete m_pList;
}

// ������Ͽ�
BOOL CTCombo::Create( CTWindow* pParent,WORD wWndType,WORD wStyle,WORD wStatus,int x,int y,int w,int h,int ID )
{
	m_pEdit->Create( NULL, TWND_TYPE_EDIT, wStyle, TWND_STATUS_NORMAL, x, y, w-COMBO_PUBHDOWN_BUTTON_WIDTH-1, h, COMBO_ID_EDIT );
	m_pEdit->m_pApp = pParent->m_pApp;

	// List��λ��Ҫ���¼���һ�£��������µ��������ϵ�
	int theY = y + h - 1;
	if( theY + COMBO_DEFAULT_LIST_HEIGHT > SCREEN_H )
		theY = y - COMBO_DEFAULT_LIST_HEIGHT + 1;

	m_pList->Create( NULL, TWND_TYPE_LIST, TWND_STYLE_SOLID, TWND_STATUS_NORMAL, x, theY, w, COMBO_DEFAULT_LIST_HEIGHT, COMBO_ID_LIST );
	m_pList->m_pApp = pParent->m_pApp;

	if (!CTWindow::Create (pParent,TWND_TYPE_COMBO,wStyle,wStatus,x,y,w,h,ID))
		return FALSE;

	return TRUE;
}

// ������Ͽ�
void CTCombo::Paint( CLCD* pLCD )
{
	// ������ɼ�����ʲôҲ������
	if( !IsWindowVisible() )
		return;

	m_pEdit->Paint( pLCD );

	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// �����Ҳ�����¼�ͷ������
	pLCD->LCDFillRect (fb, w, h, m_x+m_w-COMBO_PUBHDOWN_BUTTON_WIDTH-1, m_y, COMBO_PUBHDOWN_BUTTON_WIDTH, m_h, 0);
	pLCD->LCDHLine( fb, w, h, m_x+m_w-COMBO_PUBHDOWN_BUTTON_WIDTH-1, m_y, COMBO_PUBHDOWN_BUTTON_WIDTH, 1 );
	pLCD->LCDHLine( fb, w, h, m_x+m_w-COMBO_PUBHDOWN_BUTTON_WIDTH-1, m_y+m_h-1, COMBO_PUBHDOWN_BUTTON_WIDTH, 1 );
	pLCD->LCDVLine( fb, w, h, m_x+m_w-1, m_y, m_h, 1 );

	// ��������б�û�д򿪣������ʵ�ĵļ�ͷ��������ƿ��ĵļ�ͷ
	if( m_iDropDownStatus == COMBO_UN_DROPPED )
		pLCD->LCDDrawImage( fb, w, h, m_x+m_w-10, m_y+m_h-9, 7, 7, LCD_MODE_NORMAL, pBitmap_Arror_Down, 7, 7, 0, 0 );
	else
		pLCD->LCDDrawImage( fb, w, h, m_x+m_w-10, m_y+m_h-9, 7, 7, LCD_MODE_NORMAL, pBitmap_Hollow_Arror_Down, 7, 7, 0, 0 );
}

// ��Ͽ���Ϣ����
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
				// ���List���ڴ�״̬����ر�List�����޸�iReturnΪ1
				if( m_iDropDownStatus == COMBO_DROPPED )
				{
					ShowDropDown( FALSE );
					iReturn = 1;
				}
				// ���Listδ�򿪣��򽻸�Editȥ����(���뷨��Ҫ��Ӧ����Ϣ)
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
				// ���List���ڴ�״̬���򽫴���Ϣ����List����
				if( m_iDropDownStatus == COMBO_DROPPED )
				{
					iReturn = m_pList->Proc( pWnd, iMsg, wParam, lParam );
				}
				// ���Listδ�򿪣��򽻸�Editȥ����(���뷨��Ҫ��Ӧ����Ϣ)
				else
				{
					if( CanEdit() )
						iReturn = m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
					// ���Editδ���������List����Edit
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
				// ���List���ڴ�״̬���򽫴���Ϣ����List����
				if( m_iDropDownStatus == COMBO_DROPPED )
				{
					iReturn = m_pList->Proc( pWnd, iMsg, wParam, lParam );
				}
				// ���Listδ�򿪣��򽻸�Editȥ����(���뷨��Ҫ��Ӧ����Ϣ)
				else
				{
					if( CanEdit() )
						iReturn = m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
					// ���Editδ���������List����Edit
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
				// ���List���ڹر�״̬���򽫴���Ϣ����Edit����
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						iReturn = m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
			}
			break;
		case TVK_RIGHT:
			{
				// ���List���ڹر�״̬���򽫴���Ϣ����Edit����
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						iReturn = m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
			}
			break;
		case TVK_HOME:
			{
				// ���List���ڹر�״̬���򽫴���Ϣ����Edit����
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
				// ���List���ڹر�״̬���򽫴���Ϣ����Edit����
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
			}
			break;
		case TVK_BACK_SPACE:	// ɾ����ǰ����λ��ǰ����ַ�
			{
				// ���List���ڹر�״̬���򽫴���Ϣ����Edit����
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
			}
			break;
		case TVK_DELETE:		// ɾ����ǰ����λ�ú�����ַ�
			{
				// ���List���ڹر�״̬���򽫴���Ϣ����Edit����
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
			}
			break;
		case TVK_F11:		// �򿪹ر����뷨
			{
				// ���List���ڹر�״̬���򽫴���Ϣ����Edit����
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
			}
			break;
		case TVK_F12:		// �л����뷨
			{
				// ���List���ڹر�״̬���򽫴���Ϣ����Edit����
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						m_pEdit->Proc( pWnd, iMsg, wParam, lParam );
				}
				iReturn = 1;
			}
			break;
		case TVK_ENTER:			// ���͸����뷨��ȷ�ϼ�
			{
				// ���List���ڹر�״̬���򽫴���Ϣ����Edit����
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
				{
					if( CanEdit() )
						iReturn = m_pEdit->Proc( pWnd, iMsg, wParam, lParam );

					if( iReturn == 0 )
					{
						// ���Editû�д�������б��
						if( ShowDropDown( TRUE ) )
							iReturn = 1;
					}
				}
				else
				{
					// �����б������ݸ��±༭�򣬲��ر��б��
					SyncString();
					ShowDropDown( FALSE );
					iReturn = 1;
				}
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
				// ���List���ڹر�״̬���򽫴���Ϣ����Edit����
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
				// ���List���ڹر�״̬���򽫴���Ϣ����Edit����
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
		// �õ����㣬�����ж��Ƿ�Ӧ�Զ����������б�
		if (pWnd == this)
		{
			if( (m_wStyle & TWND_STYLE_AUTO_DROPDOWN) > 0 )
			{
				// ���������˵�
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
		// ʧȥ���㣬�ر�List
		if (pWnd == this)
			ShowDropDown( FALSE );

		// �޸�Edit��List��status��ʧȥ����
		EditKillFocus();
		ListKillFocus();
	}

	UpdateView (this);
	return iReturn;
}

#if defined(MOUSE_SUPPORT)
// �����豸��Ϣ����
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
				// �༭����
				iReturn = m_pEdit->PtProc( pWnd, iMsg, wParam, lParam );
				if( m_iDropDownStatus == COMBO_DROPPED )
					ShowDropDown( FALSE );
			}
			else
			{
				// ����\�ر������б��
				if( m_iDropDownStatus == COMBO_UN_DROPPED )
					ShowDropDown( TRUE );
				else
					ShowDropDown( FALSE );
			}

			iReturn = 1;
		}
		else if( m_iDropDownStatus == COMBO_DROPPED )
		{
			// �����б����
			iReturn = m_pList->PtProc( pWnd, iMsg, wParam, lParam );
			if( iReturn == 1 )
			{
				if( m_pList->PtInItems( x, y ) != -1 )
				{
					// ���ѡ���б����ر������б�
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

// ��ʾ�������������б��
BOOL CTCombo::ShowDropDown( BOOL bShow )
{
	// �����ǰ���ý�ֹ���������б���򷵻�FALSE
	if( !CanDropDown() )
	{
		if( m_iDropDownStatus == COMBO_DROPPED )
		{
			// ���������б�
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
			// ��ʾ�����б�
			m_pList->m_wStatus &= ~TWND_STATUS_INVISIBLE;
			m_pApp->SetTopMost( m_pList );
			m_iDropDownStatus = COMBO_DROPPED;
			// �������˵�����Ϊ����
			ListSetFocus();
		}
		else
		{
			// ���������б�
			m_pList->m_wStatus |= TWND_STATUS_INVISIBLE;
			m_pApp->SetTopMost( NULL );
			m_iDropDownStatus = COMBO_UN_DROPPED;
			// ʹ�����˵�ʧȥ����
			ListKillFocus();
		}
	}

	return TRUE;
}

// �õ������б�����ʾ״̬
BOOL CTCombo::GetDroppedState()
{
	if( m_iDropDownStatus == COMBO_DROPPED )
		return TRUE;
	else
		return FALSE;
}

// ���������б��ĸ߶�(��������)
BOOL CTCombo::SetDroppedLinage( int iLinage )
{
	if( iLinage < 1 )
		return FALSE;

	// ���������б�������ʾ����������
	m_pList->m_iHeightOfLinage = iLinage;

	// ���������б��ĸ߶�
	int theH = ( HZK_H + 1 ) * iLinage + 3;

	// List��λ��Ҫ���¼���һ�£��������µ��������ϵ�
	int theY = m_y + m_h - 1;
	if( theY + theH > SCREEN_H )
		theY = m_y - theH + 1;

	// ����List�Ĵ��ڴ�С
	m_pList->SetPosition( m_pList->m_x, theY, m_pList->m_w, theH );

	// ����List�Ĺ�����
	m_pList->RenewScroll();

	return TRUE;
}

// ������߽�ֹ�Ա༭����������
BOOL CTCombo::EnableEdit( BOOL bEnable )
{
	if( bEnable )
		m_dwExtStatus &= ~COMBO_DISABLE_EDIT;
	else
		m_dwExtStatus |= COMBO_DISABLE_EDIT;

	return TRUE;
}

// ������߽�ֹ�����˵�����
BOOL CTCombo::EnableDropDown( BOOL bEnable )
{
	if( bEnable )
		m_dwExtStatus &= ~COMBO_DISABLE_DROPDOWN;
	else
		m_dwExtStatus |= COMBO_DISABLE_DROPDOWN;

	return TRUE;
}

// ���º������ڲ����༭��
// �������ֵĳ���
int CTCombo::LimitText( int iLength )
{
	return m_pEdit->LimitText( iLength );
}

// ��ձ༭��
BOOL CTCombo::Clean()
{
	return m_pEdit->Clean();
}

// ȡ�ñ༭�������
BOOL CTCombo::GetText( char* pText )
{
	return m_pEdit->GetText( pText );
}

// ���ñ༭�������
BOOL CTCombo::SetText( char* pText, int iLength )
{
	return m_pEdit->SetText( pText, iLength );
}

// ȡ�ñ༭�����ֵĳ���
int CTCombo::GetTextLength()
{
	return m_pEdit->GetTextLength();
}

// ���º������ڲ��������б��
// �õ��б����Ŀ��
int CTCombo::GetCount()
{
	return m_pList->GetCount();
}

// �õ��б��ǰѡ����Ŀ��Index�����û��ѡ�е��򷵻�-1
int CTCombo::GetCurSel()
{
	return m_pList->GetCurSel();
}

// �����б��ǰ��ѡ����Ŀ
int CTCombo::SetCurSel( int iIndex )
{
	return m_pList->SetCurSel( iIndex );
}

// ���ĳһ�б��������
BOOL CTCombo::GetString( int iIndex, char* pText )
{
	return m_pList->GetString( iIndex, pText );
}

// ����ĳһ�б��������
BOOL CTCombo::SetString( int iIndex, char* pText )
{
	return m_pList->SetString( iIndex, pText );
}

// ���ĳһ�б������ݵĳ���
int CTCombo::GetStringLength( int iIndex )
{
	return m_pList->GetStringLength( iIndex );
}

// ���б�������һ����(����ĩβ)
BOOL CTCombo::AddString( char* pText )
{
	return m_pList->AddString( pText );
}

// ɾ���б���һ���б���
BOOL CTCombo::DeleteString( int iIndex )
{
	return m_pList->DeleteString( iIndex );
}

// ���б���ָ��λ�ò���һ����
BOOL CTCombo::InsertString( int iIndex, char* pText )
{
	return m_pList->InsertString( iIndex, pText );
}

// ɾ���б��������б���
BOOL CTCombo::RemoveAll()
{
	return m_pList->RemoveAll();
}

// ���б���в���һ����
int CTCombo::FindString( char* pText )
{
	return m_pList->FindString( pText );
}

// �����б������ݸ��±༭��
void CTCombo::SelectString( char* pText )
{
	int iIndex = m_pList->SelectString( pText );
	if( iIndex != -1 )
		SetText( pText, 255 );
}

// Edit�õ�����
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

// Editʧȥ����
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

// List�õ�����
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

// Listʧȥ����
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

// ��List��ǰ���ַ���������Edit��
void CTCombo::SyncString()
{
	char cText [256];
	memset( cText, 0x0, 256 );
	int iIndex = m_pList->GetCurSel();
	if( iIndex != -1 )
	{
		GetString( iIndex, cText );
		SetText( cText, 255 );
		
		// ������ؼ��Ǹ����ڵĽ��㣬������Caret
		if (m_pParent != NULL)
		{
			CTWindow* pParentActive = m_pParent->m_pActive;
			if( pParentActive == this )
				m_pEdit->SetSel( 0, 255 );
		}
	}
}

// �ж��Ƿ������༭������
BOOL CTCombo::CanEdit()
{
	if( (m_dwExtStatus & COMBO_DISABLE_EDIT) == 0 )
		return TRUE;
	else
		return FALSE;
}

// �ж��Ƿ���Ե����б��
BOOL CTCombo::CanDropDown()
{
	if( (m_dwExtStatus & COMBO_DISABLE_DROPDOWN) == 0 )
		return TRUE;
	else
		return FALSE;
}
/* END */
