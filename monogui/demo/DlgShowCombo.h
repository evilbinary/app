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

	// 初始化
	void Init();

	// 消息处理过了，返回1，未处理返回0
	virtual int Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam );
};

#endif // !defined(__DLGSHOWCOMBO_H__)
