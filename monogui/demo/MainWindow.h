// MainWindow.h: interface for the CMainWindow class.
//
//////////////////////////////////////////////////////////////////////
#if !defined(__MAINWINDOW_H__)
#define __MAINWINDOW_H__

#define ID_BUTTON_QUIT  20
#define ID_BUTTON_ENTER 21

class CMainWindow : public CTDialog
{
public:
  	CMainWindow();
  	virtual ~CMainWindow();

public:
	// 虚函数，绘制窗口
	void Paint (CLCD* pLCD);

	// 虚函数，消息处理
	// 消息处理过了，返回1，未处理返回0
	virtual int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

	virtual void OnInit();
};

#endif // !defined(__MAINWINDOW_H__)
