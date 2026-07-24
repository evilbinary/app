// TCaret.cpp: implementation of the CTCaret class.
//
//////////////////////////////////////////////////////////////////////
#if defined(RUN_ENVIRONMENT_LINUX)
#include <string.h>
#endif // defined(RUN_ENVIRONMENT_LINUX)

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTCaret::CTCaret()
{
    m_Caret.bValid = FALSE;			// 是否使用脱字符
    m_Caret.x = 0;				// 位置
    m_Caret.y = 0;
    m_Caret.w = 0;				// 宽高
    m_Caret.h = 0;
    m_Caret.bFlash = FALSE;			// 是否闪烁
    m_Caret.bShow  = TRUE;			// (第一次出现应该处于显示状态)
    m_Caret.lTimeInterval = 500;		// 闪烁的时间间隔(一般采用500毫秒)
    m_lLastTime = 0;
}

CTCaret::~CTCaret()
{
}

// 根据窗口的脱字符信息设置系统脱字符的参数；
BOOL CTCaret::SetCaret (struct _CARET* pCaret)
{
	if (pCaret == NULL)
		return FALSE;

	memcpy (&m_Caret, pCaret, sizeof(_CARET));
	m_lLastTime = sys_clock ();
	return TRUE;
}

// 更新脱字符的显示。
// 如果脱字符定时器到时了，则将主缓中脱字符区域的图像以适当方式送入FrameBuffer的对应位置；
BOOL CTCaret::Check(CLCD* pLCD, CLCD* pBuf)
{
	if( m_Caret.bValid )
	{
		ULONGLONG lNow = sys_clock();

		if ((lNow - m_lLastTime) >= m_Caret.lTimeInterval)
		{
			if(m_Caret.bFlash)
			{
				if(m_Caret.bShow)
				{
					// 反白显示
					DrawCaret(pLCD, pBuf);
					m_Caret.bShow = FALSE;
				}
				else
				{
					// 正常显示
					DrawCaret(pLCD, pBuf);
					m_Caret.bShow = TRUE;
				}
			}
			else
			{
				// 反白显示
				DrawCaret(pLCD, pBuf);
			}
			m_lLastTime = lNow;
		}
		return TRUE;
	}
	return FALSE;
}

void CTCaret::DrawCaret(CLCD* pLCD, CLCD* pBuf)
{
	if(m_Caret.bValid)
	{
		int ow;
		int oh;
		unsigned char* ofb = pLCD->LCDGetFB(&ow, &oh);
		int sw;
		int sh;
		unsigned char* sfb = pBuf->LCDGetFB(&sw, &sh);

		int iMode;
		if(m_Caret.bShow == TRUE)
		{
			iMode = LCD_MODE_INVERSE;
		}
		else
		{
			iMode = LCD_MODE_NORMAL;
		}

		pLCD->LCDBitBlt(ofb, ow, oh, m_Caret.x, m_Caret.y, m_Caret.w, m_Caret.h, iMode,
			sfb, sw, sh, m_Caret.x, m_Caret.y);
	}

	pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );
}
/* END */
