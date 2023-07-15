// TButton.cpp: implementation of the CTButton class.
//
//////////////////////////////////////////////////////////////////////
#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTButton::CTButton ()
{
	m_pcaImage = NULL;
}

CTButton::~CTButton ()
{
	m_pcaImage = NULL;
}

BOOL CTButton::Create( CTWindow* pParent,WORD wWndType,WORD wStyle,WORD wStatus,int x,int y,int w,int h,int ID)
{
	if( !CTWindow::Create(pParent,TWND_TYPE_BUTTON,wStyle,wStatus,x,y,w,h,ID))
		return FALSE;

	return TRUE;
}

// ����ͼƬ
BOOL CTButton::LoadImage( unsigned char* pcaImage )
{
	m_pcaImage = pcaImage;
	return TRUE;
}

// �麯�������ư�ť
void CTButton::Paint( CLCD* pLCD )
{
	// ������ɼ�����ʲôҲ������
	if( !IsWindowVisible() )
		return;

	if( m_pcaImage == NULL )
		DrawCommonButton( pLCD );
	else
		DrawImageButton( pLCD );
}

// ����ͼ��ť
void CTButton::DrawImageButton( CLCD* pLCD )
{
	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// �������ֵľ�����ʾλ��
	int tx = m_x + (m_w - 6 * GetTextLength()) / 2;
	int ty = m_y + (m_h - 12) / 2;

	// ���ݰ�ť��ͬ��״̬����
	// ͼƬ���ϵ��·ֱ��ǣ�
	// 1>Normal��ʽ��ͼƬ
	// 2>Focus��ʽ��ͼƬ
	// 3>PushDown��ʽ��ͼƬ

	// ͼƬ��Ȱ��ֽڶ���
	int iImageW = ((m_w + 7) / 8) * 8;

	// ���ư���״̬�İ�ť
	if ((m_wStatus & TWND_STATUS_PUSHED) > 0)
	{
		// ���Ƶ�ͼ
		pLCD->LCDBitBlt( fb,w,h,m_x,m_y,m_w,m_h,LCD_MODE_NORMAL,
						m_pcaImage,iImageW,m_h*3,0,m_h*2 );
		// ���Ʒ��׵�����
		// pLCD->LCDTextOut (fb,w,h,tx,ty,1,(unsigned char*)m_cCaption,255);
		return;
	}

	// �潹��״̬�İ�ť
	if ((m_wStatus & TWND_STATUS_FOCUSED) > 0)
	{
		// ���Ƶ�ͼ
		pLCD->LCDBitBlt( fb,w,h,m_x,m_y,m_w,m_h,LCD_MODE_NORMAL,
						m_pcaImage,iImageW,m_h*3,0,m_h );
		// ��������������
		// pLCD->LCDTextOut (fb,w,h,tx,ty,0,(unsigned char*)m_cCaption,255);
		return;
	}

	// ������ͨ��ʽ�İ�ť
	// ���Ƶ�ͼ
	pLCD->LCDBitBlt( fb,w,h,m_x,m_y,m_w,m_h,LCD_MODE_NORMAL,
					m_pcaImage,iImageW,m_h*3,0,0 );
	// ��������������
	// pLCD->LCDTextOut (fb,w,h,tx,ty,0,(unsigned char*)m_cCaption,255);
}

// ������ͨ���İ�ť
void CTButton::DrawCommonButton( CLCD* pLCD )
{
	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// �������ֵľ�����ʾλ��
	int tx = m_x + (m_w - 6 * GetTextLength()) / 2;
	int ty = m_y + (m_h - 12) / 2;

	// ���ݰ�ť��ͬ��״̬����
	// ����ǰ���״̬�����հ���״̬����
	if ((m_wStatus & TWND_STATUS_PUSHED) > 0)
	{
		if ((m_wStyle & TWND_STYLE_ROUND_EDGE) > 0)
		{
			// ����Բ�Ƿ��İ�ť
			pLCD->LCDFillRect (fb, w, h, m_x+6, m_y, m_w-12, m_h, 1);
			pLCD->LCDFillRect (fb, w, h, m_x, m_y+6, 6, m_h-12, 1);
			pLCD->LCDFillRect (fb, w, h, m_x+m_w-6, m_y+6, 6, m_h-12, 1);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y, 6, 6, LCD_MODE_INVERSE, pBitmap_Button_Normal,13, 13, 0, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_INVERSE, pBitmap_Button_Normal,13, 13, 6, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y+m_h-6, 6, 6, LCD_MODE_INVERSE, pBitmap_Button_Normal,13, 13, 0, 6);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y+m_h-6, 6, 6, LCD_MODE_INVERSE, pBitmap_Button_Normal,13, 13, 6, 6);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y, 6, 6, LCD_MODE_OR, pBitmap_Button_Normal,13, 13, 0, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_OR, pBitmap_Button_Normal,13, 13, 6, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y+m_h-6, 6, 6, LCD_MODE_OR, pBitmap_Button_Normal,13, 13, 0, 6);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y+m_h-6, 6, 6, LCD_MODE_OR, pBitmap_Button_Normal,13, 13, 6, 6);
		}
		else
		{
			// ������ͨ���İ�ť
			pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 1);
		}
		// ���Ʒ��׵�����
		pLCD->LCDTextOut (fb,w,h,tx,ty,1,(unsigned char*)m_cCaption,255);
		return;
	}

	// �����Ĭ�ϵģ�����ƿ�߰�ť���������Ĭ�ϣ��������ͨ��ť
	if ((m_wStatus & TWND_STATUS_DEFAULT) > 0)
	{
		// ����Ĭ�Ϸ��İ�ť
		if ((m_wStyle & TWND_STYLE_ROUND_EDGE) > 0)
		{
			// ����Բ�Ƿ��İ�ť���
			pLCD->LCDFillRect (fb, w, h, m_x+6, m_y+1, m_w-12, m_h-2, 0);
			pLCD->LCDFillRect (fb, w, h, m_x+1, m_y+6, 5, m_h-12, 0);
			pLCD->LCDFillRect (fb, w, h, m_x+m_w-6, m_y+6, 5, m_h-12, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Default,13, 13, 0, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y, 7, 6, LCD_MODE_NORMAL, pBitmap_Button_Default,13, 13, 6, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y+m_h-6, 6, 7, LCD_MODE_NORMAL, pBitmap_Button_Default,13, 13, 0, 6);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y+m_h-6, 7, 7, LCD_MODE_NORMAL, pBitmap_Button_Default,13, 13, 6, 6);
			pLCD->LCDHLine (fb, w, h, m_x+6, m_y, m_w-12, 1);
			pLCD->LCDVLine (fb, w, h, m_x, m_y+6, m_h-12, 1);
			pLCD->LCDFillRect(fb, w, h, m_x+6, m_y+m_h-1, m_w-12, 2, 1);
			pLCD->LCDFillRect(fb, w, h, m_x+m_w-1, m_y+6, 2, m_h-12, 1);
		}
		else
		{
			// ������ͨ���İ�ť���
			pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 0);
			pLCD->LCDHLine (fb, w, h, m_x, m_y, m_w, 1);
			pLCD->LCDHLine (fb, w, h, m_x, m_y+m_h-1, m_w, 1);
			pLCD->LCDVLine (fb, w, h, m_x, m_y, m_h, 1);
			pLCD->LCDVLine (fb, w, h, m_x+m_w-1, m_y, m_h, 1);
			pLCD->LCDHLine (fb, w, h, m_x+1, m_y+m_h, m_w, 1);
			pLCD->LCDVLine (fb, w, h, m_x+m_w, m_y+1, m_h, 1);
		}
	}
	else
	{
		// ������ͨ���İ�ť
		if ((m_wStyle & TWND_STYLE_ROUND_EDGE) > 0)
		{
			// ����Բ�Ƿ��İ�ť���
			pLCD->LCDFillRect (fb, w, h, m_x+6, m_y+1, m_w-12, m_h-2, 0);
			pLCD->LCDFillRect (fb, w, h, m_x+1, m_y+6, 5, m_h-12, 0);
			pLCD->LCDFillRect (fb, w, h, m_x+m_w-6, m_y+6, 5, m_h-12, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal,13, 13, 0, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal,13, 13, 6, 0);
			pLCD->LCDDrawImage(fb, w, h, m_x, m_y+m_h-6, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal,13, 13, 0, 6);
			pLCD->LCDDrawImage(fb, w, h, m_x+m_w-6, m_y+m_h-6, 6, 6, LCD_MODE_NORMAL, pBitmap_Button_Normal,13, 13, 6, 6);
			pLCD->LCDHLine (fb, w, h, m_x+6, m_y, m_w-12, 1);
			pLCD->LCDVLine (fb, w, h, m_x, m_y+6, m_h-12, 1);
			pLCD->LCDHLine(fb, w, h, m_x+6, m_y+m_h-1, m_w-12, 1);
			pLCD->LCDVLine(fb, w, h, m_x+m_w-1, m_y+6, m_h-12, 1);
		}
		else
		{
			// ������ͨ���İ�ť���
			pLCD->LCDFillRect (fb, w, h, m_x, m_y, m_w, m_h, 0);
			pLCD->LCDHLine (fb, w, h, m_x, m_y, m_w, 1);
			pLCD->LCDHLine (fb, w, h, m_x, m_y+m_h-1, m_w, 1);
			pLCD->LCDVLine (fb, w, h, m_x, m_y, m_h, 1);
			pLCD->LCDVLine (fb, w, h, m_x+m_w-1, m_y, m_h, 1);
		}
	}
	// ������ڽ��㣬�ٻ������߿�
	if ((m_wStatus & TWND_STATUS_FOCUSED) > 0)
	{
		if ((m_wStyle & TWND_STYLE_ROUND_EDGE) > 0)
		{
			// Բ�ǰ�ť�����߿�Ҫ�ܿ��Ľǵ�Բ��
			pLCD->LCDFillRect (fb,w,h,m_x+3,m_y+2,m_w-7,1,2);
			pLCD->LCDFillRect (fb,w,h,m_x+2,m_y+3,1,m_h-7,2);
			pLCD->LCDFillRect (fb,w,h,m_x+3,m_y+m_h-3,m_w-7,1,2);
			pLCD->LCDFillRect (fb,w,h,m_x+m_w-3,m_y+3,1,m_h-7,2);
		}
		else
		{
			pLCD->LCDFillRect (fb,w,h,m_x+2,m_y+2,m_w-5,1,2);
			pLCD->LCDFillRect (fb,w,h,m_x+2,m_y+2,1,m_h-5,2);
			pLCD->LCDFillRect (fb,w,h,m_x+2,m_y+m_h-3,m_w-5,1,2);
			pLCD->LCDFillRect (fb,w,h,m_x+m_w-3,m_y+2,1,m_h-5,2);
		}
	}
	// ��������������
	pLCD->LCDTextOut (fb,w,h,tx,ty,0,(unsigned char*)m_cCaption,255);
}

// �麯������Ϣ����
// ��Ϣ������ˣ�����1��δ������0
int CTButton::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = 0;

	// ��ťֻ����������Ϣ��1���ո����2���س���(ֻ�Դ���Ĭ��״̬�İ�ť��Ч)��3��HM_PUSHDOWN��Ϣ
	if (iMsg == TMSG_KEYDOWN)
	{
		if (wParam == TVK_SPACE)
		{
			if ((m_wStatus & TWND_STATUS_FOCUSED) > 0)
			{
				OnPushDown ();
			}
			iReturn = 1;
		}
		else if (wParam == TVK_ENTER)
		{
			if ((m_wStatus & TWND_STATUS_DEFAULT) > 0)
			{
				OnPushDown ();
			}
			iReturn = 1;
		}
	}
	else if (iMsg == TMSG_PUSHDOWN)
	{
		if (pWnd == this)
		{
			OnPushDown ();
		}
		iReturn = 1;
	}

	return iReturn;
}

#if defined(MOUSE_SUPPORT)
// �����豸��Ϣ����
int CTButton::PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	int iReturn = CTWindow::PtProc (pWnd, iMsg, wParam, lParam);

	if (iMsg == TMSG_LBUTTONDOWN)
	{
		if (PtInWindow (wParam, lParam) )
		{
			OnPushDown();
			iReturn = 1;
		}
	}

	return iReturn;
}
#endif // defined(MOUSE_SUPPORT)

// ���������µĴ���
void CTButton::OnPushDown ()
{
	// ˢ����ʾ���򸸴��ڷ���֪ͨ��Ϣ���ٴ�ˢ����ʾ
	m_wStatus |= TWND_STATUS_PUSHED;
	Paint(m_pApp->m_pLCD);

	// ǿ����ʾ��ť���µ�Ч��
	m_pApp->m_pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );

	// ��ʱ��ʹ��ť���µ�״̬���Ա������
	Delay( 200 );

	// �򸸴��ڷ�����ϢֵΪ��ID����Ϣ
	_TMSG msg;
	msg.pWnd    = m_pParent;
	msg.message = TMSG_NOTIFY_PARENT;
	msg.wParam  = m_ID;
	msg.lParam  = 0;
	m_pApp->PostMessage (&msg);

	m_wStatus &= ~TWND_STATUS_PUSHED;
	Paint(m_pApp->m_pLCD);

	// ǿ����ʾ��ť���µ�Ч��
	m_pApp->m_pLCD->LCDShow( 0, 0, SCREEN_W, SCREEN_H );
}
/* END */
