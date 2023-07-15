// TProgress.h: interface for the CTProgress class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TPROGRESS_H__)
#define __TPROGRESS_H__

class CTProgress : public CTWindow
{
private:
	int m_iPosition;    // ����
	int m_iRange;       // ��ĸ

public:
	CTProgress();
	virtual ~CTProgress();

public:
	// �����༭��
	virtual BOOL Create	
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

	virtual void Paint(CLCD* pLCD);
	virtual int Proc(CTWindow* pWnd, int iMsg, int wParam, int lParam);

	// �������巶Χ
	BOOL SetRange(int iRange);

	// ���õ�ǰ����
	BOOL SetPos(int iPos);
};

#endif // !defined(__TPROGRESS_H__)
