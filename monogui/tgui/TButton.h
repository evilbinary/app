// TButton.h: interface for the CTButton class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TBUTTON_H__)
#define __TBUTTON_H__

class CTWindow;

class CTButton : public CTWindow
{
private:
	// 定义图像按钮所需要的图片
	// 从上到下分别是：
	// 1>Normal样式的图片
	// 2>Focus样式的图片
	// 3>PushDown样式的图片
	unsigned char* m_pcaImage;

public:
	CTButton ();
	virtual ~CTButton ();

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
	
	// 调入图片
	BOOL LoadImage( unsigned char* pcaImage );

	// 虚函数，绘制按钮
	virtual void Paint (CLCD* pLCD);

	// 虚函数，消息处理
	// 消息处理过了，返回1，未处理返回0
	virtual int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// 坐标设备消息处理
	virtual int PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam);
#endif // defined(MOUSE_SUPPORT)

private:
	// 绘制图像按钮
	void DrawImageButton( CLCD* pLCD );

	// 绘制普通风格的按钮
	void DrawCommonButton( CLCD* pLCD );

	// 按键被按下的处理
	void OnPushDown ();
};

#endif // !defined(__TBUTTON_H__)
