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

// ��Ϣ������ˣ�����1��δ������0
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
		// �༭��չʾ��ť
		CDlgShowEdit* pDlg = new CDlgShowEdit();
		_DLGTEMPLET* pT = m_pApp->GetDlgTemplet( 102 );
		if( !pDlg->CreateFromTemplet( this, pT ) )
		  {
		    delete pDlg;
		    WORD wStyle = TB_ERROR | TB_SOLID;
		    TMessageBox( this, "������Ϣ", "�����Ի���ʧ�ܣ�", wStyle, 30 );
		  }
	      }
	      break;
	    case 103:
	      {
		// ��Ͽ�չʾ��ť
		CDlgShowCombo* pDlg = new CDlgShowCombo();
		_DLGTEMPLET* pT = m_pApp->GetDlgTemplet( 103 );
		if( !pDlg->CreateFromTemplet( this, pT ) )
		  {
		    delete pDlg;
		    WORD wStyle = TB_ERROR | TB_SOLID;
		    TMessageBox( this, "������Ϣ", "�����Ի���ʧ�ܣ�", wStyle, 40 );
		  }
		else
		  {
		    pDlg->Init();
		  }
	      }
	      break;
	    case 104:
	      {
		// ������չʾ��ť
		CDlgShowProgress* pDlg = new CDlgShowProgress();
		_DLGTEMPLET* pT = m_pApp->GetDlgTemplet( 104 );
		if( !pDlg->CreateFromTemplet( this, pT ) )
		  {
		    delete pDlg;
		    WORD wStyle = TB_ERROR | TB_SOLID;
		    TMessageBox( this, "������Ϣ", "�����Ի���ʧ�ܣ�", wStyle, 50 );
		  }
		else
		  {
		    pDlg->Init();
		  }
	      }
	      break;
	    case 105:
	      {
		// �б��չʾ��ť
		CDlgShowList* pDlg = new CDlgShowList();
		_DLGTEMPLET* pT = m_pApp->GetDlgTemplet( 105 );
		if( !pDlg->CreateFromTemplet( this, pT ) )
		  {
		    delete pDlg;
		    WORD wStyle = TB_ERROR | TB_SOLID;
		    TMessageBox( this, "������Ϣ", "�����Ի���ʧ�ܣ�", wStyle, 60 );
		  }
		else
		  {
		    pDlg->Init();
		  }
	      }
	      break;
	    case 106:
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
