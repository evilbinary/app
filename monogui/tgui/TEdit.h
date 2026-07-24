// TEdit.h: interface for the CTEdit class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TEDIT_H__)
#define __TEDIT_H__

#define IME_FORBID	0
#define IME_CLOSE	1
#define IME_OPEN	2

class CTWindow;
class CIMEWindow;

class CTEdit : public CTWindow
{
private:
	int m_iSelStart;     // 插入起始位置
	int m_iSelEnd;       // 插入终止位置
	int m_iTextLimit;    // 字符串默认最大长度
	int m_iLeftIndex;    // 显示区域左端的第一个字符的位置

	struct _CARET m_Caret;	// 脱字符
	char m_cIMEStatus;		// 输入法状态；0禁用输入法；1输入法未开；2输入法已开

#if defined(CHINESE_SUPPORT)
	CIMEWindow* m_pIME;			// 输入法窗口的指针
#endif // defined(CHINESE_SUPPORT)

#if defined(MOUSE_SUPPORT)
	int m_iOldPos;            // 用于记录鼠标点选的初始位置
#endif // defined(MOUSE_SUPPORT)

public:
	CTEdit ();
	virtual ~CTEdit ();

	// 创建编辑框
	virtual BOOL Create
	(
		CTWindow* pParent,    // 父窗口指针
		WORD wWinType,        // 窗口类型
		WORD wStyle,          // 窗口的样式
		WORD wStatus,         // 窗口的状态
		int x,
		int y,
		int w,
		int h,                // 绝对位置
		int ID                // 窗口的ID号
	);

	// 虚函数，绘制编辑框
	virtual void Paint (CLCD* pLCD);

	// 虚函数，消息处理
	// 消息处理过了，返回1，未处理返回0
	virtual int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// 坐标设备消息处理
	virtual int PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

	// 测试坐标落于的位置，该测试不关心y值
	int PtInItems( int x );
#endif // defined(MOUSE_SUPPORT)

	// 设置当前选择区域的起始位置和终止位置
	// 如果位置跨越了汉字，则向后推一个字节
	BOOL SetSel (int iStart, int iEnd);

	// 获得当前选择区域的起始位置和终止位置
	BOOL GetSel (int* iStart, int* iEnd);

	// 向当前位置插入字符串
	// 如果当前位置是一个选择区，则替换当前选择区域的字符串，
	// 然后将选择区修改成一个插入位置
	// 如果当前位置是一个插入位置，则在此插入位置上插入字符串
	// 注意，如果总长度超越了长度限制，则截取合适长度的字串
	// 截取时应注意汉字的处理
	BOOL InsertCharacter (char* cString);

	// 删除当前位置前面的一个字符或者后面的一个字符
	// bMode: TRUE,删后面的;FALSE,删前面的
	BOOL DelOneCharacter (BOOL bMode);

	// 删除当前选中区域的内容
	BOOL DelCurSel ();

	// 限制输入字符串的最大长度
	BOOL LimitText (int iLength);

	// 清空字符串的内容
	BOOL Clean ();

	// 查看输入法窗口是否打开
	BOOL IsIMEOpen ();
private:
	// 更改显示区左端第一个字符的索引
	void RenewLeftPos ();

	// 根据当前的脱字符设置更新系统脱字符
	void RenewCaret ();

	// 取得当前显示区域最右端字符的索引
	int GetRightDisplayIndex ();

	// 将字母键转换成ASC码
	BOOL TVKToASC( char* psString, int nVK, int nMask );

};

#endif // !defined(__TEDIT_H__)
