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
	BOOL	bValid;				// �Ƿ�ʹ�����ַ�
	int	x;						// λ��
	int	y;
	int	w;						// ���
	int	h;
	BOOL	bFlash;				// �Ƿ���˸
	BOOL	bShow;				// ��־��ǰ״̬Ϊ��ʾ��������
	ULONGLONG	lTimeInterval;	// ��˸��ʱ������һ�����500���룩
}CARET;

class CTCaret
{
public:
	_CARET m_Caret;
	ULONGLONG m_lLastTime;

public:
	CTCaret();
	virtual ~CTCaret();

	// ���ݴ��ڵ����ַ���Ϣ����ϵͳ���ַ��Ĳ�����
	BOOL SetCaret (_CARET* pCaret);

	// �������ַ�����ʾ��
	// ������ַ���ʱ����ʱ�ˣ������������ַ������ͼ�����ʵ���ʽ����FrameBuffer�Ķ�Ӧλ�ã�
	BOOL Check (CLCD* pLCD, CLCD* pBuf);

	// �������ַ�
	void CTCaret::DrawCaret (CLCD* pLCD, CLCD* pBuf);
};

#endif // !defined(__TCARET_H__)
