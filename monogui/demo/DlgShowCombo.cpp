// DlgShowCombo.cpp
//////////////////////////////////////////////////////////////////////
#include "main.h"
//////////////////////////////////////////////////////////////////////
CDlgShowCombo::CDlgShowCombo()
{
}

CDlgShowCombo::~CDlgShowCombo()
{
}

// ��ʼ��
void CDlgShowCombo::Init()
{
	CTCombo* pCombo = (CTCombo*)FindChildByID( 102 );
	pCombo->AddString( "�й�" );
	pCombo->AddString( "����" );
	pCombo->AddString( "Ӣ��" );
	pCombo->AddString( "����" );
	pCombo->AddString( "����" );
	pCombo->AddString( "����˹" );
	pCombo->AddString( "�����" );
	pCombo->AddString( "������" );
	pCombo->AddString( "���" );
	pCombo->AddString( "����" );
	pCombo->AddString( "���ô�" );
	pCombo->AddString( "������" );
}

// ��Ϣ������ˣ�����1��δ������0
int CDlgShowCombo::Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam )
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
