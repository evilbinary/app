// TDialog.h: interface for the CTDialog class.
//
//////////////////////////////////////////////////////////////////////
#if !defined(__TDIALOG_H__)
#define __TDIALOG_H__

class CTWindow;
class CLCD;
class CTAccell;
class CTDlgTemplet;
struct _CARET;

class CTDialog : public CTWindow
{
public:
	int m_iDoModalReturn;   // 指出DoModal函数的返回值
	CTAccell* m_pAccell;   // 快捷键列表

public:
	CTDialog();
	virtual ~CTDialog();

	// 虚函数，绘制对话框
	virtual void Paint(CLCD* pLCD);

	// 虚函数，消息处理
	// 消息处理过了，返回1，未处理返回0
	virtual int Proc(CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// 坐标设备消息处理
	virtual int PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam);
#endif // defined(MOUSE_SUPPORT)

	// 进入模式状态
	virtual int DoModal();

	// 根据ID号，使用相应的模板文件创建对话框
	virtual BOOL CreateFromID( CTWindow* pWnd, int iID );

	// 从对话框模板文件创建对话框
	BOOL CreateFromFile( CTWindow* pParent, const char* filename );

	// 从对话框模板结构体创建对话框
	BOOL CreateFromTemplet( CTWindow* pParent, _DLGTEMPLET* pT );

	// 处理焦点切换
	virtual BOOL OnChangeFocus();

	// 关闭对话框
	void CloseDialog();

private:
	// 绘制对话框
	// 0,非焦点窗口；1,反白显示(焦点窗口)
	void DrawDialog(CLCD* pLCD, int iMode);
};

#endif // !defined(__TDIALOG_H__)
