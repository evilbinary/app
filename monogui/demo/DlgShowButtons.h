// DlgShowButtons.h
//
//////////////////////////////////////////////////////////////////////

#if !defined(__DLGSHOWBUTTONS_H__)
#define __DLGSHOWBUTTONS_H__

class CDlgShowButtons : public CTDialog
{
public:
	CDlgShowButtons();
	virtual ~CDlgShowButtons();

	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam );
};

#endif // !defined(__DLGSHOWBUTTONS_H__)
