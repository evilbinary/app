// MainWindow.cpp: implementation of the CMainWindow class.
//
//////////////////////////////////////////////////////////////////////
#include "main.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CMainWindow::CMainWindow()
{
}

CMainWindow::~CMainWindow()
{
}

// �麯�������ƴ���
void CMainWindow::Paint (CLCD* pLCD)
{
  int w;
  int h;
  unsigned char* fb = pLCD->LCDGetFB (&w, &h);

  // ������߿�
  pLCD->LCDLine( fb,w,h,   0,   0, 239,   0, 1 );
  pLCD->LCDLine( fb,w,h,   0,   0,   0, 127, 1 );
  pLCD->LCDLine( fb,w,h, 239,   0, 239, 127, 1 );
  pLCD->LCDLine( fb,w,h,   0, 127, 239, 127, 1 );

  // ��ʾ�̱�
  pLCD->LCDShowBrand( fb,w,h, SCREEN_W/2-74, SCREEN_H/2-13 );  

  // Ĭ�ϻ���
  CTWindow::Paint (pLCD);
}

// �麯������Ϣ����
// ��Ϣ������ˣ�����1��δ������0
int CMainWindow::Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam )
{
  // �Ƚ���Ĭ�ϴ���
  CTDialog::Proc( pWnd, iMsg, wParam, lParam );
  
  switch( iMsg )
    {
    case TMSG_NOTIFY_PARENT:
      {
	switch( wParam )
	  {
	  case ID_BUTTON_QUIT:
	    {
	      DebugPrintf("Message: ID_BUTTON_QUIT\n");
	      m_pApp->PostQuitMessage();
	    }
	    break;
	  case ID_BUTTON_ENTER:
	    {
	      // ���ɡ���ť��ʾ���Ի���
	      CDlgShowButtons* pDlg = new CDlgShowButtons();
	      _DLGTEMPLET* pT = m_pApp->GetDlgTemplet( 100 );
	      if( !pDlg->CreateFromTemplet( this, pT ) )
		{
		  delete pDlg;
		  WORD wStyle = TB_ERROR | TB_SOLID;
		  TMessageBox( this, "������Ϣ", "�����Ի���ʧ�ܣ�", wStyle, 10 );
		}
	    }
	    break;
	  default:
	    {
	    }
	  }
      }
      break;
    default:
      {
      }
    }
  
  return 1;
}

// ����һ����ť
void CMainWindow::OnInit()
{
  DebugPrintf("CMainWindow::OnInit()\n");

  // Set Window Title
  SetText( "This is the MainWindow", 20 );

  CTButton* pBtn = new CTButton;
  pBtn->Create( this, TWND_TYPE_BUTTON, TWND_STYLE_ROUND_EDGE, TWND_STATUS_NORMAL, 
		30, 100, 60, 20, ID_BUTTON_QUIT );
  pBtn->SetText( "�˳�", 20 );

  pBtn = new CTButton;
  pBtn->Create( this, TWND_TYPE_BUTTON, TWND_STYLE_ROUND_EDGE, TWND_STATUS_NORMAL, 
		140, 100, 60, 20, ID_BUTTON_ENTER );
  pBtn->SetText( "����", 20 );
}

/* END */
