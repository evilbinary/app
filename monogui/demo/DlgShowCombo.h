// DlgShowCombo
//
//////////////////////////////////////////////////////////////////////

#if !defined(__DLGSHOWCOMBO_H__)
#define __DLGSHOWCOMBO_H__

class CDlgShowCombo : public CTDialog
{
public:
	CDlgShowCombo();
	virtual ~CDlgShowCombo();

	// ��ʼ��
	void Init();

	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam );
};

#endif // !defined(__DLGSHOWCOMBO_H__)
