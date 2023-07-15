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

	// 初始化
	void Init();

	// 消息处理过了，返回1，未处理返回0
	virtual int Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam );
};

#endif // !defined(__DLGSHOWLIST_H__)
