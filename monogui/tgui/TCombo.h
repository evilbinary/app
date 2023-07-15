// TCombo.h: interface for the CTCombo class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TCOMBO_H__)
#define __TCOMBO_H__

#define COMBO_DISABLE_EDIT				0x00000001
#define COMBO_DISABLE_DROPDOWN			0x00000002
#define COMBO_UN_DROPPED				0
#define COMBO_DROPPED					1
#define COMBO_PUBHDOWN_BUTTON_WIDTH		11	// EDIT右侧小按钮的默认宽度
#define COMBO_DEFAULT_LIST_HEIGHT		42	// LIST控件的默认高度(三行)
#define COMBO_ID_EDIT					100	// EDIT控件的ID号
#define COMBO_ID_LIST					101	// LIST控件的ID号

class CTEdit;
class CTList;

class CTCombo : public CTWindow
{
private:
	int m_dwExtStatus;       // 扩展状态，指出该Combo的Edit和List是否使能
	int m_iDropDownStatus;   // 下拉列表框的弹出状态

	CTEdit* m_pEdit;
	CTList* m_pList;

public:
	CTCombo();
	virtual ~CTCombo();

public:
	// 创建组合框
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

	// 绘制组合框
	virtual void Paint(CLCD* pLCD);

	// 组合框消息处理
	virtual int Proc(CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// 坐标设备消息处理
	virtual int PtProc(CTWindow* pWnd, int iMsg, int wParam, int lParam);
#endif // defined(MOUSE_SUPPORT)

	// 显示或者隐藏下拉列表框
	BOOL ShowDropDown( BOOL bShow );

	// 得到下拉列表框的显示状态
	BOOL GetDroppedState();

	// 设置下拉列表框的高度(以行数计)
	BOOL SetDroppedLinage( int iLinage );

	// 允许或者禁止对编辑框输入文字
	BOOL EnableEdit( BOOL bEnable );

	// 允许或者禁止下拉菜单弹出
	BOOL EnableDropDown( BOOL bEnable );

	// 以下函数用于操作编辑框
	// 限制文字的长度
	int LimitText( int iLength );

	// 清空编辑框
	BOOL Clean();

	// 取得编辑框的文字
	BOOL GetText( char* pText );

	// 设置编辑框的文字
	BOOL SetText( char* pText, int iLength );

	// 取得编辑框文字的长度
	int GetTextLength();

// 以下函数用于操作下拉列表框
	// 得到列表的条目数
	int GetCount();

	// 得到列表框当前选中项目的Index，如果没有选中的则返回-1
	int GetCurSel();

	// 设置列表框当前的选中项目
	int SetCurSel( int iIndex );

	// 获得某一列表项的内容
	BOOL GetString( int iIndex, char* pText );

	// 设置某一列表项的内容
	BOOL SetString( int iIndex, char* pText );

	// 获得某一列表项内容的长度
	int GetStringLength( int iIndex );

	// 向列表框中添加一个串(加在末尾)
	BOOL AddString( char* pText );

	// 删除列表框的一个列表项
	BOOL DeleteString( int iIndex );

	// 在列表框的指定位置插入一个串
	BOOL InsertString( int iIndex, char* pText );

	// 删除列表框的所有列表项
	BOOL RemoveAll();

	// 在列表框中查找一个串
	int FindString( char* pText );

	// 根据列表框的内容更新编辑框
	void SelectString( char* pText );

	// 将List当前的字符串拷贝到Edit中
	void SyncString();

private:
	// Edit得到焦点
	void EditSetFocus();

	// Edit失去焦点
	void EditKillFocus();

	// List得到焦点
	void ListSetFocus();

	// List失去焦点
	void ListKillFocus();

	// 判断是否可以向编辑框输入
	BOOL CanEdit();

	// 判断是否可以弹出列表框
	BOOL CanDropDown();
};

#endif // !defined(__TCOMBO_H__)
