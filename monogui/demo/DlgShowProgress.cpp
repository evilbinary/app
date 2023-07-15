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

// ��ʼ��
void CDlgShowProgress::Init()
{
	m_pApp->PaintWindows( m_pApp->m_pMainWnd );
	CTProgress* pPgs = (CTProgress*)FindChildByID( 102 );
	pPgs->SetRange( 100 );
	int i;
	for( i=0; i<101; i++ )
	{
		pPgs->SetPos( i );

		// ��ʱһ���
		Delay(100);
	}
	// ��ʱһ���
	Delay(1000);

	// �رձ�����
	_TMSG msg;
	msg.pWnd = this;
	msg.message = TMSG_CLOSE;
	msg.wParam = 0;
	msg.lParam = 0;
	m_pApp->PostMessage( &msg );
	m_pApp->CleanKeyBuffer();
}
/* END */
