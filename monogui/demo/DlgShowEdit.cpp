// DlgShowEdit.cpp
//////////////////////////////////////////////////////////////////////
#include "main.h"
//////////////////////////////////////////////////////////////////////
CDlgShowEdit::CDlgShowEdit()
{
}

CDlgShowEdit::~CDlgShowEdit()
{
}

BOOL CDlgShowEdit::CreateFromID( CTWindow *pWnd, int iID )
{
	CTDialog::CreateFromID( pWnd, iID );

	return TRUE;
}

void CDlgShowEdit::OnInit()
{
}

// ��Ϣ������ˣ�����1��δ������0
int CDlgShowEdit::Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam )
{
	CTDialog::Proc( pWnd, iMsg, wParam, lParam );

	if( pWnd = this )
	{
		if( iMsg == TMSG_NOTIFY_PARENT )
		{
			switch( wParam )
			{
			case 104:
			case 105:
				{
					// �˳���ť
					_TMSG msg;
					msg.pWnd = this;
					msg.message = TMSG_CLOSE;
					msg.wParam = 0;
					msg.lParam = 0;
					m_pApp->PostMessage( &msg );
				}
				break;
			}
		}
	}

	return 1;
}
/* END */
