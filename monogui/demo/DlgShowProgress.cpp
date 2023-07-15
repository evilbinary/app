// DlgShowProgress.cpp
//////////////////////////////////////////////////////////////////////
#include "main.h"
//////////////////////////////////////////////////////////////////////
CDlgShowProgress::CDlgShowProgress()
{
}

CDlgShowProgress::~CDlgShowProgress()
{
}

// 初始化
void CDlgShowProgress::Init()
{
	m_pApp->PaintWindows( m_pApp->m_pMainWnd );
	CTProgress* pPgs = (CTProgress*)FindChildByID( 102 );
	pPgs->SetRange( 100 );
	int i;
	for( i=0; i<101; i++ )
	{
		pPgs->SetPos( i );

		// 延时一会儿
		Delay(100);
	}
	// 延时一会儿
	Delay(1000);

	// 关闭本窗口
	_TMSG msg;
	msg.pWnd = this;
	msg.message = TMSG_CLOSE;
	msg.wParam = 0;
	msg.lParam = 0;
	m_pApp->PostMessage( &msg );
	m_pApp->CleanKeyBuffer();
}
/* END */
