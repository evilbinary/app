// DlgShowList.cpp
//////////////////////////////////////////////////////////////////////
#include "main.h"
//////////////////////////////////////////////////////////////////////
CDlgShowList::CDlgShowList()
{
}

CDlgShowList::~CDlgShowList()
{
}

// ��ʼ��
void CDlgShowList::Init()
{
	CTList* pList = (CTList*)FindChildByID( 102 );
	pList->AddString( "�й�" );
	pList->AddString( "����" );
	pList->AddString( "Ӣ��" );
/*
	pList->AddString( "����" );
	pList->AddString( "����" );
	pList->AddString( "����˹" );
	pList->AddString( "�����" );
	pList->AddString( "������" );
	pList->AddString( "���" );
	pList->AddString( "����" );
	pList->AddString( "���ô�" );
	pList->AddString( "������" );
*/
	pList = (CTList*)FindChildByID( 103 );
	pList->AddString( "����" );
	pList->AddString( "�Ϻ�" );
	pList->AddString( "�Ͼ�" );
	pList->AddString( "�ൺ" );
	pList->AddString( "����" );
	pList->AddString( "����" );
	pList->AddString( "����" );
	pList->AddString( "�ɶ�" );
	pList->AddString( "����" );
	pList->AddString( "����" );
	pList->AddString( "̩��" );
	pList->AddString( "���" );

}

// ��Ϣ������ˣ�����1��δ������0
int CDlgShowList::Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam )
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
