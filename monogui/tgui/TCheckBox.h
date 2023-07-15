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
	
	// �麯�������ư�ť
	virtual void Paint (CLCD* pLCD);

	// �麯������Ϣ����
	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// �����豸��Ϣ����
	virtual int PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam);
#endif // defined(MOUSE_SUPPORT)

	// ����ѡ��״̬
	BOOL SetCheck( int nCheck );

	// �õ�ѡ��״̬
	int GetCheck();

private:
	// ����ѡ��״̬�ı�
	void OnCheck ();
};

#endif // !defined(__TCHECKBOX_H__)
