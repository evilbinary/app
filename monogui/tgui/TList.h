// TList.h: interface for the CTList class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TLIST_H__)
#define __TLIST_H__

#define LIST_TEXT_MAX_LENGTH	256		// 一行列表框显示内容的长度
#define LIST_ITEM_H       (HZK_H+1)		// 条目的高度

typedef struct _LISTCONTENT
{
	char text[ LIST_TEXT_MAX_LENGTH ];	// 保存列表框中一项文本内容
	struct _LISTCONTENT *next;			// 链表中下一个列表框中内容
}LISTCONTENT;							// 用于保存一条列表框中内容的数据结构

class CTList : public CTWindow  
{
private:
	int m_iItemCount;  // 用于存放当前有多少条列表项，初始为零
	int m_iTopIndex;   // 用于存放当前显示列表中最上面一条的Index值，初始为零
	int m_iCurSel;     // 用于存放当前选中的列表项，初始为－1

	struct _LISTCONTENT* m_pContent;	// 指向数据链表

public:
	int m_iHeightOfLinage;     // 列表框能够显示的行数

public:
	CTList ();
	virtual ~CTList ();

public:
	// 创建窗口
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

	// 绘制输入法窗口
	virtual void Paint (CLCD* pLCD);

	// 消息处理过了，返回1，未处理返回0
	virtual int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// 坐标设备消息处理
	virtual int PtProc(CTWindow* pWnd, int iMsg, int wParam, int lParam);

	// 测试坐标落于的条目，-1表示未落于任何条目
	int PtInItems( int x, int y );
#endif // defined(MOUSE_SUPPORT)

	// 获得条目的数量
	int GetCount();

	// 获得当前显示区域最上面一个条目的Index
	int GetTopIndex();

	// 设置当前显示区域最上面一个条目的Index
	int SetTopIndex(int iIndex);

	// 得到当前选中项目的Index，如果没有选中的则返回-1
	int GetCurSel();

	// 设置当前的选中项目
	int SetCurSel(int iIndex);

	// 获得某一列表项的内容
	BOOL GetString(int iIndex, char* pText);

	// 设置某一列表项的内容
	BOOL SetString(int iIndex, char* pText);

	// 获得某一列表项内容的长度
	int GetStringLength(int iIndex);

	// 向列表框中添加一个串(加在末尾)
	BOOL AddString(char* pText);

	// 删除一个列表项
	BOOL DeleteString(int iIndex);

	// 在指定位置插入一个串
	BOOL InsertString(int iIndex, char* pText);

	// 删除所有列表项
	BOOL RemoveAll();

	// 在列表项中查找一个串
	int FindString(char* pText);

	// 在列表项中查找一个串，如果找到，则将它设置为选中，并显示在第一行
	// (如果在最后一页，则不必放在第一行，详见软件文档)
	int SelectString(char* pText);

	// 调整列表框的高度为整行
	BOOL SetLinage(int iLinage);

	// 更新滚动条
	void RenewScroll();

#if defined(MOUSE_SUPPORT)
	// 与滚动条有关的函数
	virtual void OnScrollUp ();
	virtual void OnScrollDown ();
	virtual void OnScrollPageUp ();
	virtual void OnScrollPageDown ();
	virtual void OnVScrollNewPos (int iNewPos);
#endif // defined(MOUSE_SUPPORT)
};

#endif // !defined(__TLIST_H__)
