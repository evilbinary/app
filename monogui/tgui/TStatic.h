// TStatic.h: interface for the CTStatic class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TSTATIC_H__)
#define __TSTATIC_H__

class CTStatic  : public CTWindow
{
public:
	CTStatic ();
	virtual ~CTStatic ();

	// 定义图像按钮所需要的图片
	unsigned char* m_pcaImage;

public:
	// 创建编辑框
	BOOL Create
	(
		CTWindow* pParent,
		WORD wWndType,
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

	void Paint (CLCD* pLCD);
	int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);
};

#endif // !defined(__TSTATIC_H__)
