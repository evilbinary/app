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
    m_Caret.bValid = FALSE;			// �Ƿ�ʹ�����ַ�
    m_Caret.x = 0;				// λ��
    m_Caret.y = 0;
    m_Caret.w = 0;				// ���
    m_Caret.h = 0;
    m_Caret.bFlash = FALSE;			// �Ƿ���˸
    m_Caret.bShow  = TRUE;			// (��һ�γ���Ӧ�ô�����ʾ״̬)
    m_Caret.lTimeInterval = 500;		// ��˸��ʱ����(һ�����500����)
    m_lLastTime = 0;
}

CTCaret::~CTCaret()
{
}

// ���ݴ��ڵ����ַ���Ϣ����ϵͳ���ַ��Ĳ�����
BOOL CTCaret::SetCaret (struct _CARET* pCaret)
{
	if (pCaret == NULL)
		return FALSE;

	memcpy (&m_Caret, pCaret, sizeof(_CARET));
	m_lLastTime = sys_clock ();
	return TRUE;
}

// �������ַ�����ʾ��
// ������ַ���ʱ����ʱ�ˣ������������ַ������ͼ�����ʵ���ʽ����FrameBuffer�Ķ�Ӧλ�ã�
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
					// ������ʾ
					DrawCaret(pLCD, pBuf);
					m_Caret.bShow = FALSE;
				}
				else
				{
					// ������ʾ
					DrawCaret(pLCD, pBuf);
					m_Caret.bShow = TRUE;
				}
			}
			else
			{
				// ������ʾ
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
