// DlgShowEdit.h
//
//////////////////////////////////////////////////////////////////////

#if !defined(__DLGSHOWEDIT_H__)
#define __DLGSHOWEDIT_H__

class CDlgShowEdit : public CTDialog
{
public:
	CDlgShowEdit();
	virtual ~CDlgShowEdit();

	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam );

	virtual BOOL CreateFromID( CTWindow *pWnd, int iID );

	virtual void OnInit();
};

#endif // !defined(__DLGSHOWEDIT_H__)
