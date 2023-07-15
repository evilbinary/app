// TProgress.h: interface for the CTProgress class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TPROGRESS_H__)
#define __TPROGRESS_H__

class CTProgress : public CTWindow
{
private:
	int m_iPosition;    // 分子
	int m_iRange;       // 分母

public:
	CTProgress();
	virtual ~CTProgress();

public:
	// 创建编辑框
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

	// 设置整体范围
	BOOL SetRange(int iRange);

	// 设置当前进度
	BOOL SetPos(int iPos);
};

#endif // !defined(__TPROGRESS_H__)
