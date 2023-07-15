// TScroll.h: interface for the CTScroll class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TSCROLL_H__)
#define __TSCROLL_H__

#define SCROLL_WIDTH 13

#if defined(MOUSE_SUPPORT)
// 定义鼠标左键落点的测试结果
#define SCROLL_PT_UP           1
#define SCROLL_PT_PAGEUP       2
#define SCROLL_PT_DOWN         3
#define SCROLL_PT_PAGEDOWN     4
#define SCROLL_PT_BUTTON       5
#endif // defined(MOUSE_SUPPORT)

class CTScroll
{
private:
	int m_iRange;		// 移动范围
	int m_iSize;		// 中间按钮的大小
	int m_iPos;			// 当前位置
/* 例如：
 * 一个列表框有40条列表项，每页显示6个，当前最上面一条是第25条，则：
 * m_iRange = 40;    m_iSize = 6;    m_iPos = 24;
 */

#if defined(MOUSE_SUPPORT)
	int m_iOldPos;      // 记录鼠标点中按钮时的位置
	int m_iOldPt;       // 记录鼠标点中按钮时的坐标
						// (垂直滚动条记录y坐标，水平滚动条记录x坐标)
#endif // defined(MOUSE_SUPPORT)

public:
	int m_iStatus;		// 0：不显示；1：显示
	int m_iMode;		// 1：垂直滚动条；2：水平滚动条
	int m_x;
	int m_y;
	int m_w;
	int m_h;			// 大小位置(滚动条的宽度强制设为13)

public:
	CTScroll ();
	virtual ~CTScroll ();

	// 创建滚动条（应指定水平还是垂直）
	BOOL Create (int iMode, int x, int y, int w, int h, int iRange, int iSize, int iPos);

	// 绘制滚动条
	void Paint (CLCD* pLCD);

	// 设置滚动范围
	BOOL SetRange (int iRange);

	// 设置中间按钮的大小
	BOOL SetSize (int iSize);

	// 设置当前位置
	BOOL SetPos (int iPos);

#if defined(MOUSE_SUPPORT)
	// 记录当前位置和当前坐标
	BOOL RecordPos (int iPt);

	// 判断鼠标落点
	int TestPt (int x, int y);

	// 根据鼠标坐标计算对应的新位置
	int TestNewPos (int x, int y);
#endif // defined(MOUSE_SUPPORT)
};

#endif // !defined(__TSCROLL_H__)
