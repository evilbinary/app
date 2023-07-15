// TCaret.h: interface for the CTCaret class.
//
//////////////////////////////////////////////////////////////////////
#if defined(RUN_ENVIRONMENT_LINUX)
#include "TSystemCommon.h"
#endif // defined(RUN_ENVIRONMENT_LINUX)

#if !defined(__TCARET_H__)
#define __TCARET_H__

typedef struct _CARET
{
	BOOL	bValid;				// 是否使用脱字符
	int	x;						// 位置
	int	y;
	int	w;						// 宽高
	int	h;
	BOOL	bFlash;				// 是否闪烁
	BOOL	bShow;				// 标志当前状态为显示或者隐藏
	ULONGLONG	lTimeInterval;	// 闪烁的时间间隔（一般采用500毫秒）
}CARET;

class CTCaret
{
public:
	_CARET m_Caret;
	ULONGLONG m_lLastTime;

public:
	CTCaret();
	virtual ~CTCaret();

	// 根据窗口的脱字符信息设置系统脱字符的参数；
	BOOL SetCaret (_CARET* pCaret);

	// 更新脱字符的显示。
	// 如果脱字符定时器到时了，则将主缓中脱字符区域的图像以适当方式送入FrameBuffer的对应位置；
	BOOL Check (CLCD* pLCD, CLCD* pBuf);

	// 绘制脱字符
	void CTCaret::DrawCaret (CLCD* pLCD, CLCD* pBuf);
};

#endif // !defined(__TCARET_H__)
