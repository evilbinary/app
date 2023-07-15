// TButton.h: interface for the CTButton class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TBUTTON_H__)
#define __TBUTTON_H__

class CTWindow;

class CTButton : public CTWindow
{
private:
	// ����ͼ��ť����Ҫ��ͼƬ
	// ���ϵ��·ֱ��ǣ�
	// 1>Normal��ʽ��ͼƬ
	// 2>Focus��ʽ��ͼƬ
	// 3>PushDown��ʽ��ͼƬ
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
	
	// ����ͼƬ
	BOOL LoadImage( unsigned char* pcaImage );

	// �麯�������ư�ť
	virtual void Paint (CLCD* pLCD);

	// �麯������Ϣ����
	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// �����豸��Ϣ����
	virtual int PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam);
#endif // defined(MOUSE_SUPPORT)

private:
	// ����ͼ��ť
	void DrawImageButton( CLCD* pLCD );

	// ������ͨ���İ�ť
	void DrawCommonButton( CLCD* pLCD );

	// ���������µĴ���
	void OnPushDown ();
};

#endif // !defined(__TBUTTON_H__)
