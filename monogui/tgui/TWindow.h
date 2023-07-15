// TWindow.h: interface for the CTWindow class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TWINDOW_H__)
#define __TWINDOW_H__

class CLCD;
class CTAccell;
class CTCaret;
class CTScroll;
class CTApp;

class CTWindow
{
public:
	WORD m_wWndType;                // 窗口类型
	WORD m_wStyle;                  // 窗口的样式
	WORD m_wStatus;                 // 窗口的状态
	int m_x, m_y, m_w, m_h;         // 相对于桌面窗口(MainWindow)的绝对位置
	int m_iTabOrder;                // 为子窗口时的Tab序号
	CTScroll* m_pVScroll;           // 纵向滚动条
	CTScroll* m_pHScroll;           // 横向滚动条
	int m_ID;                       // 窗口的ID号
	char m_cCaption[256];           // 题头

	/* 应用程序和父窗口的指针 */
	CTApp*    m_pApp;               // 应用程序的指针
	CTWindow* m_pParent;            // 父窗口的指针

	/* 子窗口信息 */
	int m_iChildCount;              // 子窗口数目
	CTWindow* m_pChildren;          // 第一个子窗口(子窗口用双向链表存放)
	CTWindow* m_pActive;            // 处于当前焦点的子窗口(可以是NULL)
	CTWindow* m_pDefaultButton;     // 窗口的默认按钮

	/* 兄弟窗口 */
	CTWindow* m_pNext;              // 指向下一个兄弟窗口
	CTWindow* m_pPrev;              // 指向上一个兄弟窗口
	//
	CTWindow* m_pOldParentActiveWindow;    // 当窗口关闭时，应当恢复父窗口老的焦点

#if defined(MOUSE_SUPPORT)
	int m_iMouseMoving;				// 截获鼠标移动消息的标志
#endif // defined(MOUSE_SUPPORT)

public:
	CTWindow();
	virtual ~CTWindow();

	// 创建一个窗口；
	// 如果父窗口不是NULL，则把该窗口插入父窗口的字窗口链表中；
	// tab序号根据父窗口的字窗口数目确定；
	// 快捷键列表、脱字符、滚动条、题头文字可以在窗口创建后再进行设置；
	virtual BOOL Create
	(
		CTWindow* pParent,			// 父窗口指针
		WORD wWinType,				// 窗口类型
		WORD wStyle,				// 窗口的样式
		WORD wStatus,				// 窗口的状态
		int x,
		int y,
		int w,
		int h,						// 绝对位置
		int ID						// 窗口的ID号
	);

	// 虚函数，绘制窗口，只绘制附加的滚动条
	virtual void Paint (CLCD* pLCD);

	// 虚函数，消息处理
	// 消息处理过了，返回1，未处理返回0
	virtual int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// 虚函数，消息处理
	virtual int PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam);
#endif // defined(MOUSE_SUPPORT)

	// 改变窗口的位置和尺寸
	// 设置成功返回TRUE，失败返回FALSE
	BOOL SetPosition (int x, int y, int w, int h);

	// 获得窗口的位置和尺寸
	BOOL GetPosition (int* px, int* py, int* pw, int* ph);

	// 获得窗口的ID号
	int GetID ();

	// 设置窗口的题头
	BOOL SetText (char* pText, int iLength);

	// 获取窗口的题头
	BOOL GetText (char* pText);

	// 获得窗口题头字符串的长度
	int GetTextLength ();

	// 设置当前处于活动状态（处于焦点）的子窗口
	// 注意：应当将当前处于活动状态的子窗口改成非活动的
	// 返回实际获得焦点的控件的ID号
	int SetFocus (CTWindow* pWnd);

	// 在子窗口链表中查找相应ID的窗口
	CTWindow* FindChildByID (int id);

	// 在子窗口链表中查找相应tab号的窗口
	CTWindow* FindChildByTab (int iTab);

	// 在子窗口链表中查找原始默认按钮
	CTWindow* FindDefaultButton ();

	// 查找在位于一个子窗口垂直正下方的子窗口
	CTWindow* FindWindowDown( CTWindow* pWnd );

	// 查找在位于一个子窗口垂直正上方的子窗口
	CTWindow* FindWindowUp( CTWindow* pWnd );

	// 查找在位于一个子窗口水平正左方的子窗口
	CTWindow* FindWindowLeft( CTWindow* pWnd );

	// 查找在位于一个子窗口水平正右方的子窗口
	CTWindow* FindWindowRight( CTWindow* pWnd );
	  
	// 删除一个子窗口
	// 相应的要更新各个窗口的tab序号
	BOOL DeleteChild (CTWindow* pChild);

	// 设置窗口的滚动条
	// iMode = 1：设置右侧的垂直滚动条；
	// iMode = 2：设置下方的水平滚动条；
	// 滚动条的尺寸根据窗口尺寸进行设置
	// 如果要设置的滚动条并不存在，则创建
	BOOL SetScrollBar (int iMode, int iRange, int iSize, int iPos);

	// 控制滚动条的显示与消隐
	// iScroll = 1：设置垂直滚动条；iScroll = 2：设置水平滚动条；
	// 如果要设置的滚动条并不存在，则返回FALSE
	BOOL ShowScrollBar (int iScroll, BOOL bShow);

	// 向桌面发送重绘窗口的消息
	BOOL UpdateView (CTWindow* pWnd);

	// 窗口使能
 	BOOL EnableWindow( BOOL bEnable );

	// 判断窗口是否使能
	BOOL IsWindowEnabled();

	// 判断窗口是否可见
	BOOL IsWindowVisible();

	// 判断窗口是否焦点链上的窗口
	BOOL IsWindowActive();

#if defined(MOUSE_SUPPORT)
	// 判断一个点是否落在本窗口的范围之内
	BOOL PtInWindow (int x, int y);
#endif // defined(MOUSE_SUPPORT)

// 重要的消息处理函数

	// 定时器处理
	virtual void OnTimer (int iTimerID, int iInterval);

	// 窗口创建后的初始化处理
	virtual void OnInit();

	// 关闭窗口，可以关闭则返回TRUE，不可以关闭返回FALSE
	virtual BOOL OnClose ();

	// 子窗口得到焦点
	virtual void OnSetFocus (int id);

	// 子窗口失去焦点
	virtual void OnKillFocus (int id);

	// 子窗口数据改变
	virtual void OnDataChange (int id);

// 与滚动条有关的函数
#if defined(MOUSE_SUPPORT)
	virtual void OnScrollUp();
	virtual void OnScrollDown();
	virtual void OnScrollLeft();
	virtual void OnScrollRight();
	virtual void OnScrollPageUp();
	virtual void OnScrollPageDown();
	virtual void OnScrollPageLeft();
	virtual void OnScrollPageRight();
	virtual void OnVScrollNewPos(int iNewPos);
	virtual void OnHScrollNewPos(int iNewPos);
#endif // defined(MOUSE_SUPPORT)
};

#endif // !defined(__TWINDOW_H__)

