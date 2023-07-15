// TCheckBox.h: interface for the CTCheckBox class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TCHECKBOX_H__)
#define __TCHECKBOX_H__

class CTWindow;

class CTCheckBox : public CTWindow
{
private:
	int m_iCheckState;

public:
	CTCheckBox ();
	virtual ~CTCheckBox ();

	BOOL Create(CTWindow* pParent,
		WORD wWinType,
		WORD wStyle,
		WORD wStatus,
		int x,
		int y,
		int w,
		int h,
		int ID
	);
	
	// 虚函数，绘制按钮
	virtual void Paint (CLCD* pLCD);

	// 虚函数，消息处理
	// 消息处理过了，返回1，未处理返回0
	virtual int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// 坐标设备消息处理
	virtual int PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam);
#endif // defined(MOUSE_SUPPORT)

	// 设置选择状态
	BOOL SetCheck( int nCheck );

	// 得到选择状态
	int GetCheck();

private:
	// 处理选择状态改变
	void OnCheck ();
};

#endif // !defined(__TCHECKBOX_H__)
