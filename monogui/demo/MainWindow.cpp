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

// 虚函数，绘制窗口
void CMainWindow::Paint (CLCD* pLCD)
{
  int w;
  int h;
  unsigned char* fb = pLCD->LCDGetFB (&w, &h);

  // 绘制外边框
  pLCD->LCDLine( fb,w,h,   0,   0, 239,   0, 1 );
  pLCD->LCDLine( fb,w,h,   0,   0,   0, 127, 1 );
  pLCD->LCDLine( fb,w,h, 239,   0, 239, 127, 1 );
  pLCD->LCDLine( fb,w,h,   0, 127, 239, 127, 1 );

  // 显示商标
  pLCD->LCDShowBrand( fb,w,h, SCREEN_W/2-74, SCREEN_H/2-13 );  

  // 默认绘制
  CTWindow::Paint (pLCD);
}

// 虚函数，消息处理
// 消息处理过了，返回1，未处理返回0
int CMainWindow::Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam )
{
  // 先进行默认处理
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
	      // 生成“按钮演示”对话框
	      CDlgShowButtons* pDlg = new CDlgShowButtons();
	      _DLGTEMPLET* pT = m_pApp->GetDlgTemplet( 100 );
	      if( !pDlg->CreateFromTemplet( this, pT ) )
		{
		  delete pDlg;
		  WORD wStyle = TB_ERROR | TB_SOLID;
		  TMessageBox( this, "出错信息", "创建对话框失败！", wStyle, 10 );
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

// 创建一个按钮
void CMainWindow::OnInit()
{
  DebugPrintf("CMainWindow::OnInit()\n");

  // Set Window Title
  SetText( "This is the MainWindow", 20 );

  CTButton* pBtn = new CTButton;
  pBtn->Create( this, TWND_TYPE_BUTTON, TWND_STYLE_ROUND_EDGE, TWND_STATUS_NORMAL, 
		30, 100, 60, 20, ID_BUTTON_QUIT );
  pBtn->SetText( "退出", 20 );

  pBtn = new CTButton;
  pBtn->Create( this, TWND_TYPE_BUTTON, TWND_STYLE_ROUND_EDGE, TWND_STATUS_NORMAL, 
		140, 100, 60, 20, ID_BUTTON_ENTER );
  pBtn->SetText( "进入", 20 );
}

/* END */
