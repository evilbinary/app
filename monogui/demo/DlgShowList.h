// DlgShowList.h
//
//////////////////////////////////////////////////////////////////////

#if !defined(__DLGSHOWLIST_H__)
#define __DLGSHOWLIST_H__

class CDlgShowList : public CTDialog
{
public:
	CDlgShowList();
	virtual ~CDlgShowList();

	// ��ʼ��
	void Init();

	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam );
};

#endif // !defined(__DLGSHOWLIST_H__)
