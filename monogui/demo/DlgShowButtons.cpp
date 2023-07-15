// DlgShowButtons.cpp
//////////////////////////////////////////////////////////////////////
#include "main.h"
//////////////////////////////////////////////////////////////////////
CDlgShowButtons::CDlgShowButtons()
{
}

CDlgShowButtons::~CDlgShowButtons()
{
}

// 消息处理过了，返回1，未处理返回0
int CDlgShowButtons::Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam )
{
  CTDialog::Proc( pWnd, iMsg, wParam, lParam );
  
  if( pWnd = this )
    {
      if( iMsg == TMSG_NOTIFY_PARENT )
	{
	  switch( wParam )
	    {
	    case 102:
	      {
		// 编辑框展示按钮
		CDlgShowEdit* pDlg = new CDlgShowEdit();
		_DLGTEMPLET* pT = m_pApp->GetDlgTemplet( 102 );
		if( !pDlg->CreateFromTemplet( this, pT ) )
		  {
		    delete pDlg;
		    WORD wStyle = TB_ERROR | TB_SOLID;
		    TMessageBox( this, "出错信息", "创建对话框失败！", wStyle, 30 );
		  }
	      }
	      break;
	    case 103:
	      {
		// 组合框展示按钮
		CDlgShowCombo* pDlg = new CDlgShowCombo();
		_DLGTEMPLET* pT = m_pApp->GetDlgTemplet( 103 );
		if( !pDlg->CreateFromTemplet( this, pT ) )
		  {
		    delete pDlg;
		    WORD wStyle = TB_ERROR | TB_SOLID;
		    TMessageBox( this, "出错信息", "创建对话框失败！", wStyle, 40 );
		  }
		else
		  {
		    pDlg->Init();
		  }
	      }
	      break;
	    case 104:
	      {
		// 进度条展示按钮
		CDlgShowProgress* pDlg = new CDlgShowProgress();
		_DLGTEMPLET* pT = m_pApp->GetDlgTemplet( 104 );
		if( !pDlg->CreateFromTemplet( this, pT ) )
		  {
		    delete pDlg;
		    WORD wStyle = TB_ERROR | TB_SOLID;
		    TMessageBox( this, "出错信息", "创建对话框失败！", wStyle, 50 );
		  }
		else
		  {
		    pDlg->Init();
		  }
	      }
	      break;
	    case 105:
	      {
		// 列表框展示按钮
		CDlgShowList* pDlg = new CDlgShowList();
		_DLGTEMPLET* pT = m_pApp->GetDlgTemplet( 105 );
		if( !pDlg->CreateFromTemplet( this, pT ) )
		  {
		    delete pDlg;
		    WORD wStyle = TB_ERROR | TB_SOLID;
		    TMessageBox( this, "出错信息", "创建对话框失败！", wStyle, 60 );
		  }
		else
		  {
		    pDlg->Init();
		  }
	      }
	      break;
	    case 106:
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
