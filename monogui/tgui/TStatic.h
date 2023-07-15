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

	// ����ͼ��ť����Ҫ��ͼƬ
	unsigned char* m_pcaImage;

public:
	// �����༭��
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

	// ����ͼƬ
	BOOL LoadImage( unsigned char* pcaImage );

	void Paint (CLCD* pLCD);
	int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);
};

#endif // !defined(__TSTATIC_H__)
