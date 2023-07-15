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
	// �麯�������ƴ���
	void Paint (CLCD* pLCD);

	// �麯������Ϣ����
	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

	virtual void OnInit();
};

#endif // !defined(__MAINWINDOW_H__)
