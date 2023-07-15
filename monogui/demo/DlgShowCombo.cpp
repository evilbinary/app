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

// 初始化
void CDlgShowCombo::Init()
{
	CTCombo* pCombo = (CTCombo*)FindChildByID( 102 );
	pCombo->AddString( "中国" );
	pCombo->AddString( "美国" );
	pCombo->AddString( "英国" );
	pCombo->AddString( "法国" );
	pCombo->AddString( "韩国" );
	pCombo->AddString( "俄罗斯" );
	pCombo->AddString( "意大利" );
	pCombo->AddString( "土耳其" );
	pCombo->AddString( "瑞典" );
	pCombo->AddString( "丹麦" );
	pCombo->AddString( "加拿大" );
	pCombo->AddString( "新西兰" );
}

// 消息处理过了，返回1，未处理返回0
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
					// 退出按钮
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
