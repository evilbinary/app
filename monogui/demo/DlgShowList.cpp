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

// 初始化
void CDlgShowList::Init()
{
	CTList* pList = (CTList*)FindChildByID( 102 );
	pList->AddString( "中国" );
	pList->AddString( "美国" );
	pList->AddString( "英国" );
/*
	pList->AddString( "法国" );
	pList->AddString( "韩国" );
	pList->AddString( "俄罗斯" );
	pList->AddString( "意大利" );
	pList->AddString( "土耳其" );
	pList->AddString( "瑞典" );
	pList->AddString( "丹麦" );
	pList->AddString( "加拿大" );
	pList->AddString( "新西兰" );
*/
	pList = (CTList*)FindChildByID( 103 );
	pList->AddString( "北京" );
	pList->AddString( "上海" );
	pList->AddString( "南京" );
	pList->AddString( "青岛" );
	pList->AddString( "深圳" );
	pList->AddString( "福州" );
	pList->AddString( "吉林" );
	pList->AddString( "成都" );
	pList->AddString( "日照" );
	pList->AddString( "济南" );
	pList->AddString( "泰安" );
	pList->AddString( "天津" );

}

// 消息处理过了，返回1，未处理返回0
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
