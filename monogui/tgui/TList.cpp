// TList.cpp: implementation of the CTList class.
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTList::CTList()
{
	m_iItemCount = 0;      // ���ڴ�ŵ�ǰ�ж������б����ʼΪ��
	m_iTopIndex  = 0;      // ���ڴ�ŵ�ǰ��ʾ�б���������һ����Indexֵ����ʼΪ��
	m_iCurSel    = -1;     // ���ڴ�ŵ�ǰѡ�е��б����ʼΪ��1
	m_pContent = NULL;
}

CTList::~CTList()
{
	RemoveAll();
}

// �����б��
BOOL CTList::Create
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
	if (!CTWindow::Create (pParent,TWND_TYPE_LIST,wStyle,TWND_STATUS_NORMAL,x,y,w,h,ID))
		return FALSE;

	m_iHeightOfLinage = (m_h - 3) / LIST_ITEM_H;		// һҳ�ܹ���ʾ����Ŀ��
	RenewScroll();
	return TRUE;
}

// �����б��
void CTList::Paint(CLCD* pLCD)
{
	// ������ɼ�����ʲôҲ������
	if( !IsWindowVisible() )
		return;

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

	// ���ָ��������Ч�����������Ӱ
	if ((m_wStyle & TWND_STYLE_SOLID) > 0)
	{
		pLCD->LCDHLine (fb, w, h, m_x+1, m_y+m_h, m_w, 1);
		pLCD->LCDVLine (fb, w, h, m_x+m_w, m_y+1, m_h, 1);
	}

	// �����б�����
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

		// ȡ��Ҫ��ʾ���ַ���
		GetString( iTopItem+i, cTemp );
		int iTextLength = GetStringLength( iTopItem+i );
		// ���������ʾ�ĳ���
		int iWidth = m_w - 4;
		if( (m_pVScroll != NULL) && ((m_wStyle & TWND_STYLE_NO_SCROLL)==0) )
		{
			if( m_pVScroll->m_iStatus == 1)              // ������������ʾ״̬
				iWidth = iWidth - SCROLL_WIDTH + 1;      // ��ȥ�������Ŀ��
		}

		int iLengthLimit = GetDisplayLimit_1( cTemp, iWidth);

		if( iTopItem+i != iCurSel )
		{
			pLCD->LCDTextOut_1( fb, w, h, m_x+2, m_y+LIST_ITEM_H*i+2, LCD_MODE_NORMAL, (unsigned char*)cTemp, iLengthLimit );
		}
		else
		{
			// ������ؼ����ڽ��㣬�򷴰���ʾѡ����Ŀ
			// ������ǽ��㣬�������߿�Ȧ��ѡ����Ŀ
			if( (m_wStatus & TWND_STATUS_FOCUSED) > 0 )
			{
				pLCD->LCDFillRect( fb, w, h, m_x+2, m_y+LIST_ITEM_H*i+2, iWidth, HZK_H, 1 );
				pLCD->LCDTextOut_1( fb, w, h, m_x+2, m_y+LIST_ITEM_H*i+2, LCD_MODE_INVERSE, (unsigned char*)cTemp, iLengthLimit );
			}
			else
			{
				pLCD->LCDTextOut_1( fb, w, h, m_x+2, m_y+LIST_ITEM_H*i+2, LCD_MODE_NORMAL, (unsigned char*)cTemp, iLengthLimit );
				pLCD->LCDFillRect (fb,w,h,m_x+2,m_y+LIST_ITEM_H*i+1,iWidth+1,1,1);            // ��
				pLCD->LCDFillRect (fb,w,h,m_x+1,m_y+LIST_ITEM_H*i+1,1,HZK_H+2,1);             // ��
				pLCD->LCDFillRect (fb,w,h,m_x+2,m_y+LIST_ITEM_H*(i+1)+1,iWidth+1,1,1);        // ��
				pLCD->LCDFillRect (fb,w,h,m_x+iWidth+2,m_y+LIST_ITEM_H*i+1,1,HZK_H+2,1);      // ��
			}
		}
	}

	// �ȵ��û���Ļ��ƺ���(���ڻ��ƹ�����)
	CTWindow::Paint(pLCD);
}

// ��Ϣ����
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
				// �����ƶ�ѡ��
				iCurSel -= 1;
				if( iCurSel < 0 )
					iCurSel = 0;

				SetCurSel (iCurSel);

				if( iTopItem > iCurSel )
					iTopItem = iCurSel;
				if( iTopItem+iCanDisplayed-1 < iCurSel )
					iTopItem = iCurSel-iCanDisplayed+1;

				m_iTopIndex = iTopItem;

				// ���¹�����
				RenewScroll();
				iReturn = 1;
			}
			break;
		case TVK_DOWN:
			{
				// �����ƶ�ѡ��
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

				// ���¹�����
				RenewScroll();
				iReturn = 1;
			}
			break;
		case TVK_PAGE_UP:
			{
				// ��ǰ��ҳ
				iCurSel -= iCanDisplayed;
				if( iCurSel < 0 )
					iCurSel = 0;

				SetCurSel (iCurSel);

				if( iTopItem > iCurSel )
					iTopItem = iCurSel;
				if( iTopItem+iCanDisplayed-1 < iCurSel )
					iTopItem = iCurSel-iCanDisplayed+1;

				m_iTopIndex = iTopItem;

				// ���¹�����
				RenewScroll();
				iReturn = 1;
			}
			break;
		case TVK_PAGE_DOWN:
			{
				// ���ҳ
				// �����ƶ�ѡ��
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

				// ���¹�����
				RenewScroll();
				iReturn = 1;
			}
			break;
		case TVK_ENTER:			// ���͸����뷨��ȷ�ϼ�
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
		// �õ����㣬���û�д��ڴ���ѡ��״̬����ĿӦ������һ������Ϊ��ǰѡ��
		// �����ƶ���ʾ�б���ǰѡ������ں��ʵ�λ��
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
// �����豸��Ϣ����
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

// �����������ڵ���Ŀ��-1��ʾδ�����κ���Ŀ
int CTList::PtInItems( int x, int y )
{
	int iCount        = m_iItemCount;
	int iTopItem      = m_iTopIndex;
	int iCurSel       = m_iCurSel;
	int iCanDiaplayed = m_iHeightOfLinage;

	int iRight = m_x + m_w;
	if( (m_pVScroll != NULL) && ((m_wStyle & TWND_STYLE_NO_SCROLL)==0) )
	{
		if( m_pVScroll->m_iStatus == 1)    // ������������ʾ״̬
			iRight -= SCROLL_WIDTH;        // ��ȥ�������Ŀ��
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

// �����Ŀ������
int CTList::GetCount()
{
	return m_iItemCount;
}

// ��õ�ǰ��ʾ����������һ����Ŀ��Index
int CTList::GetTopIndex()
{
	return m_iTopIndex;
}

// ���õ�ǰ��ʾ����������һ����Ŀ��Index
int CTList::SetTopIndex(int iIndex)
{
	int iCount		= m_iItemCount;
	int iTopItem	= m_iTopIndex;

	// �������ݺϷ��Լ��
	if( (iIndex < 0) || (iIndex > iCount-1) )
		return -1;

	int iOldTopIndex = iTopItem;
	// �����б����������һ������ֵ
	m_iTopIndex = iIndex;

	// ˢ����Ļ
	UpdateView(this);

	return iOldTopIndex;
}

// �õ���ǰѡ����Ŀ��Index�����û��ѡ�е��򷵻�-1
int CTList::GetCurSel()
{
	return m_iCurSel;
}

// ���õ�ǰ��ѡ����Ŀ
int CTList::SetCurSel( int iIndex )
{
	int iCount			= m_iItemCount;
	int iCurSel			= m_iCurSel;

	if( (iIndex < 0) || (iIndex > iCount-1 ) )
		return -1;

	// �ƶ����㵽indexָ���е����ݡ�����
	int iOldIndex = iCurSel;
	m_iCurSel = iIndex;

	// �������ڷ���TMSG_DATACHANGE��Ϣ
	_TMSG Msg;
	Msg.pWnd = m_pParent;
	Msg.message = TMSG_DATACHANGE;
	Msg.wParam = 0;
	Msg.lParam = 0;
	m_pApp->PostMessage(&Msg);

	return iOldIndex;
}

// ���ĳһ�б��������
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

	// ����iIndex��ָ���б��������
	if( pNextItem != NULL )
	{
		strncpy( pText, pNextItem->text, LIST_TEXT_MAX_LENGTH-1 );
		return TRUE;
	}
	
	return FALSE;
}

// ����ĳһ�б��������
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

	// ����iIndex��ָ���б��������
	if( pNextItem != NULL )
	{
		memset( pNextItem->text, 0x0, LIST_TEXT_MAX_LENGTH );
		strncpy( pNextItem->text, pText, LIST_TEXT_MAX_LENGTH-1 );
		return TRUE;
	}
	
	return FALSE;
}

// ���ĳһ�б������ݵĳ���
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

	// �ⶨiIndex��ָ���б������ݵĳ���
	int iLength = 0;
	if( pNextItem != NULL )
		iLength = strlen( pNextItem->text );

	return iLength;
}

// ���б�������һ����(����ĩβ)
BOOL CTList::AddString(char* pText)
{
	int iCount = m_iItemCount;

	// �����µĽڵ�
	_LISTCONTENT* theNewOne = new _LISTCONTENT;
	strncpy( theNewOne->text, pText, LIST_TEXT_MAX_LENGTH-1);
	theNewOne->next = NULL;

	if (iCount == 0)
	{
		m_pContent = theNewOne;
		m_iItemCount ++;
		return TRUE;
	}

	// ������ĩһ���б���
	_LISTCONTENT* theNext = m_pContent;
	int i;
	for( i=0; i<iCount-1; i++ )
	{
		if( theNext->next == NULL )
			return FALSE;

		theNext = theNext->next;
	}

	// ���һ����nextָ�����ΪNULL������˵������������ڴ���
	if( theNext->next != NULL )
		return FALSE;

	theNext->next = theNewOne;
	m_iItemCount ++;

	// ���¹�����
	RenewScroll();
	return TRUE;
}

// ɾ��һ���б���
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

		// ����
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

		// ����
		m_iItemCount = iCount;
		m_iCurSel = iCurSel;		
		RenewScroll();
		return TRUE;
	}

	_LISTCONTENT* pNextItem = m_pContent;
	_LISTCONTENT* pLastItem = m_pContent;	// ���ڴ��Ҫɾ���ڵ����һ���ڵ�

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

	// ɾ��iIndex��ָ�����б���
	if( pNextItem != NULL )
	{
		pLastItem->next = pNextItem->next;
		delete pNextItem;
		iCount --;
		if( iCurSel >= iCount )
			iCurSel = (iCount -1);

		// ����
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

// ��ָ��λ�ò���һ����
BOOL CTList::InsertString(int iIndex, char* pText)
{
	int iCount			= m_iItemCount;
	int iCurSel			= m_iCurSel;

	if( iIndex < 0 )
		return FALSE;

	if( (iIndex > 0) && (iIndex > iCount-1) )
		iIndex = iCount-1;

	// �����µĽڵ�
	_LISTCONTENT* theNewOne = new _LISTCONTENT;
	strncpy( theNewOne->text, pText, LIST_TEXT_MAX_LENGTH-1);
	theNewOne->next = NULL;

	if( iCount == 0)
	{
		m_pContent = theNewOne;
		m_iItemCount ++;

		// ���¹�����
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

		// ���¹�����
		RenewScroll();
		return TRUE;
	}

	_LISTCONTENT* pNextItem = m_pContent;
	_LISTCONTENT* pLastItem = m_pContent;	// ���ڴ��Ҫ����ڵ����һ���ڵ�

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

	// ��iIndexλ�ò����µ��б���
	if( pNextItem != NULL )
	{
		pLastItem->next = theNewOne;
		theNewOne->next = pNextItem;
		m_iItemCount ++;
		if( iCurSel == iIndex )
			m_iCurSel ++;

		// ���¹�����
		RenewScroll();
		return TRUE;
	}
	
	return FALSE;
}

// ɾ�������б���
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
			// ���¹�����
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
	// ���¹�����
	RenewScroll();
	return TRUE;
}

// ���б����в���һ����
int CTList::FindString(char* pText)
{
	int iCount = m_iItemCount;

	// ѭ�����������б���
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

// ���б����в���һ����������ҵ�����������Ϊѡ�У�����ʾ�ڵ�һ��
// (��������һҳ���������һ����ʾ���б�����׶�)
int CTList::SelectString(char* pText)
{
	int iCount          = m_iItemCount;
	int iTopItem		= m_iTopIndex;
	int iCanDisplayed	= m_iHeightOfLinage;

	// �������в���ָ�����ַ���
	int iIndex = FindString(pText);
	if( iIndex == -1 )	// ���û���ҵ�
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

// �����б��ĸ߶�Ϊ����
BOOL CTList::SetLinage(int iLinage)
{
	if( iLinage < 1 )
		return FALSE;

	m_iHeightOfLinage = iLinage;
	m_h = iLinage * ( HZK_H + 1 ) + 3;
	RenewScroll();
	return TRUE;
}

// ���¹�����
void CTList::RenewScroll()
{
	if((m_wStyle & TWND_STYLE_NO_SCROLL) > 0)
		return;
		
	int iCount          = m_iItemCount;
	int iTopItem		= m_iTopIndex;
	int iCanDisplayed	= m_iHeightOfLinage;

	if( iCount > iCanDisplayed )
	{
		// ��ʾ������
		SetScrollBar( 1, iCount, iCanDisplayed, iTopItem );
		ShowScrollBar( 1, TRUE );
	}
	else
	{
		// ���ع�����
		ShowScrollBar( 1, FALSE );
	}
	UpdateView( this );
}

#if defined(MOUSE_SUPPORT)
// ����
void CTList::OnScrollUp ()
{
	int iCount			= m_iItemCount;
	int iTopItem		= m_iTopIndex;
	int iCurSel			= m_iCurSel;
	int iCanDisplayed	= m_iHeightOfLinage;

	if ( iTopItem > 0)
		iTopItem --;

	m_iTopIndex = iTopItem;
	// ���¹�����
	RenewScroll();
}

// ����
void CTList::OnScrollDown ()
{
	int iCount			= m_iItemCount;
	int iTopItem		= m_iTopIndex;
	int iCurSel			= m_iCurSel;
	int iCanDisplayed	= m_iHeightOfLinage;

	if (iTopItem + iCanDisplayed < iCount)
		iTopItem ++;

	m_iTopIndex = iTopItem;
	// ���¹�����
	RenewScroll();
}

// ���Ϸ�ҳ
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
	// ���¹�����
	RenewScroll();
}

// ���·�ҳ
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
	// ���¹�����
	RenewScroll();
}

// ��������ק
void CTList::OnVScrollNewPos (int iNewPos)
{
	// �޸�TopItem��ֵ
	m_iTopIndex = iNewPos;

	// ���¹�����
	RenewScroll();
}
#endif // defined(MOUSE_SUPPORT)
/* END */
