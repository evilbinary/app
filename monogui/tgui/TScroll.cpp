// TScroll.cpp: implementation of the CTScroll class.
//
//////////////////////////////////////////////////////////////////////
#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTScroll::CTScroll ()
{
	m_iStatus = 0;		// 0：不显示；1：显示
	m_iMode	  = 0;		// 1：垂直滚动条；2：水平滚动条
	m_x       = 0;
	m_y       = 0;
	m_w       = 0;
	m_h       = 0;		// 位置
	m_iRange  = 2;		// 移动范围
	m_iSize   = 1;		// 中间按钮的大小
	m_iPos	  = 0;		// 当前位置

#if defined(MOUSE_SUPPORT)
	m_iOldPos = 0;
	m_iOldPt  = 0;
#endif // defined(MOUSE_SUPPORT)
}

CTScroll::~CTScroll ()
{
}

// 创建滚动条（应指定水平还是垂直）
BOOL CTScroll::Create (int iMode, int x, int y, int w, int h, int iRange, int iSize, int iPos)
{
	m_x		  = x;
	m_y		  = y;

	if (iMode == 1)	// 垂直滚动条
	{
		m_w = 13;
		m_h = h;
		if (m_h < 20)
			m_h = 20;
	}
	else if (iMode == 2)
	{
		m_w = w;
		m_h = 13;
		if (m_w < 20)
			m_w = 20;
	}
	else
	{
		return FALSE;
	}

	m_iStatus = 1;
	m_iMode	  = iMode;

	if (!SetRange (iRange))
		return FALSE;

	if (!SetSize (iSize))
		return FALSE;

	if (!SetPos (iPos))
		return FALSE;

	return TRUE;
}

// 绘制滚动条
void CTScroll::Paint (CLCD* pLCD)
{
	if (m_iStatus == 0)		// 不显示
		return;

	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	// 绘制边框
	pLCD->LCDHLine (fb, w, h, m_x, m_y, m_w, 1);
	pLCD->LCDHLine (fb, w, h, m_x, m_y+m_h-1, m_w, 1);
	pLCD->LCDVLine (fb, w, h, m_x, m_y, m_h, 1);
	pLCD->LCDVLine (fb, w, h, m_x+m_w-1, m_y, m_h, 1);

	// 填充中间的部分
	pLCD->LCDFillRect (fb, w, h, m_x+1, m_y+1, m_w-2, m_h-2, 0);

	// 绘制上下箭头
	if (m_iMode == 1)		// 垂直滚动条
	{
		pLCD->LCDDrawImage (fb, w, h, m_x+3, m_y+3, 7, 7, 0,
				pBitmap_Arror_Up, 7, 7, 0, 0);
		pLCD->LCDDrawImage (fb, w, h, m_x+3, m_y+m_h-10, 7, 7, 0,
				pBitmap_Arror_Down, 7, 7, 0, 0);
	}
	else if (m_iMode == 2)	// 水平滚动条
	{
		pLCD->LCDDrawImage (fb, w, h, m_x+3, m_y+3, 7, 7, 0,
				pBitmap_Arror_Left, 7, 7, 0, 0);
		pLCD->LCDDrawImage (fb, w, h, m_x+m_w-10, m_y+3, 7, 7, 0,
				pBitmap_Arror_Right, 7, 7, 0, 0);
	}

	// 绘制中间的按钮
	if (m_iMode == 1)	// 垂直
	{
		int iTopPos = m_y + 11;
		int iBottomPos = m_y + m_h - 11;
		int iHeight = iBottomPos - iTopPos;

		// 绘制黑白相间的底纹
		pLCD->LCDFillRect (fb, w, h, m_x+1, iTopPos, m_w-2, iHeight, 2);

		if (iHeight < 14)
		{
			// 没有空间绘制按钮
			return;
		}

		// 计算按钮的绘制位置和长宽
		int iBtnHeight = iHeight * m_iSize / m_iRange;	// 高度
		if (iBtnHeight < 11)		// 中间按钮至少11个像素的宽度
		{
			iBtnHeight = 11;
		}
		int iBtnTop    = iTopPos + (iHeight - iBtnHeight) * m_iPos / (m_iRange-m_iSize);
		int iBtnButtom = iBtnTop + iBtnHeight;

		// 绘制按钮
		pLCD->LCDDrawImage (fb, w, h, m_x+1, iBtnTop, 11, 4, 0,
				pBitmap_Scroll_Button, 11, 11, 0, 0);
		pLCD->LCDDrawImage (fb, w, h, m_x+1, iBtnButtom-4, 11, 4, 0,
				pBitmap_Scroll_Button, 11, 11, 0, 7);
		pLCD->LCDFillRect (fb, w, h, m_x+1, iBtnTop+4, m_w-2, iBtnButtom-iBtnTop-8, 0);
		pLCD->LCDVLine (fb, w, h, m_x+2, iBtnTop+4, iBtnButtom-iBtnTop-8, 1);
		pLCD->LCDVLine (fb, w, h, m_x+m_w-3, iBtnTop+4, iBtnButtom-iBtnTop-8, 1);

		// 绘制按钮上的两条线
		pLCD->LCDHLine (fb, w, h, m_x+4, (iBtnTop+iBtnButtom)/2-1, 5, 1);
		pLCD->LCDHLine (fb, w, h, m_x+4, (iBtnTop+iBtnButtom)/2+1, 5, 1);
	}
	else if (m_iMode == 2)	// 水平
	{
		int iLeftPos = m_x + 11;
		int iRightPos = m_x + m_w - 11;
		int iWidth = iRightPos - iLeftPos;

		// 绘制黑白相间的底纹
		pLCD->LCDFillRect (fb, w, h, iLeftPos, m_y+1, iWidth, m_h-2, 2);

		if (iWidth < 14)
		{
			// 没有空间绘制按钮
			return;
		}

		// 计算按钮的绘制位置和长宽
		int iBtnWidth = iWidth * m_iSize / m_iRange;	// 高度
		if (iBtnWidth < 11)		// 中间按钮至少11个像素的宽度
		{
			iBtnWidth = 11;
		}
		int iBtnLeft  = iLeftPos + (iWidth - iBtnWidth) * m_iPos / (m_iRange-m_iSize);
		int iBtnRight = iBtnLeft + iBtnWidth;

		// 绘制按钮
		pLCD->LCDDrawImage (fb, w, h, iBtnLeft, m_y+1, 4, 11, 0,
				pBitmap_Scroll_Button, 11, 11, 0, 0);
		pLCD->LCDDrawImage (fb, w, h, iBtnRight-4, m_y+1, 4, 11, 0,
				pBitmap_Scroll_Button, 11, 11, 7, 0);
		pLCD->LCDFillRect (fb, w, h, iBtnLeft+4, m_y+1, iBtnRight-iBtnLeft-8, m_h-2, 0);
		pLCD->LCDHLine (fb, w, h, iBtnLeft+4, m_y+2, iBtnRight-iBtnLeft-8, 1);
		pLCD->LCDHLine (fb, w, h, iBtnLeft+4, m_y+m_h-3, iBtnRight-iBtnLeft-8, 1);

		// 绘制按钮上的两条线
		pLCD->LCDVLine (fb, w, h, (iBtnLeft+iBtnRight)/2-1, m_y+4, 5, 1);
		pLCD->LCDVLine (fb, w, h, (iBtnLeft+iBtnRight)/2+1, m_y+4, 5, 1);
	}
}

// 设置滚动范围
BOOL CTScroll::SetRange (int iRange)
{
	if (iRange > 2)	// 如果小于2会导致除数为0错误
	{
		m_iRange = iRange;
		SetSize (m_iSize);
		SetPos (m_iPos);
		return TRUE;
	}
	return FALSE;
}

// 设置中间按钮的大小
BOOL CTScroll::SetSize (int iSize)
{
	if (iSize > 1)
	{
		m_iSize = iSize;
		if (m_iSize > m_iRange)
		{
			m_iSize = m_iRange;
		}
		return TRUE;
	}
	return FALSE;
}

// 设置当前位置
BOOL CTScroll::SetPos (int iPos)
{
	if (iPos >= 0)
	{
		m_iPos = iPos;
		if (m_iPos > (m_iRange-1))
		{
			m_iPos = m_iRange - 1;
		}
		return TRUE;
	}
	return FALSE;
}

#if defined(MOUSE_SUPPORT)
// 记录当前位置和当前坐标
BOOL CTScroll::RecordPos (int iPt)
{
	if (m_iStatus == 0)     // 不显示
		return FALSE;

	m_iOldPos = m_iPos;
	m_iOldPt = iPt;
	return TRUE;
}

// 判断鼠标落点
int CTScroll::TestPt (int x, int y)
{
	if (m_iStatus == 0)     // 不显示
		return 0;

	int L = m_x;
	int T = m_y;
	int R = m_x+m_w;
	int B = m_y+m_h;

	if ( !((x >= L) && (y >= T) && (x <= R) && (y <= B)) )
		return 0;

	if (m_iMode == 1)       // 垂直
	{
		int iTopPos = m_y + 11;
		int iBottomPos = m_y + m_h - 11;
		int iBtnTop = 0;
		int iBtnButtom = 0;

		int iHeight = iBottomPos - iTopPos;
		if (iHeight >= 14)
		{
			// 计算按钮的绘制位置和长宽
			int iBtnHeight = iHeight * m_iSize / m_iRange;	// 高度
			if (iBtnHeight < 11)		// 中间按钮至少11个像素的宽度
			{
				iBtnHeight = 11;
			}
			iBtnTop    = iTopPos + (iHeight - iBtnHeight) * m_iPos / (m_iRange-m_iSize);
			iBtnButtom = iBtnTop + iBtnHeight;
		}

		B = iTopPos;
		if( PtInRect( x,y,L,T,R,B ) )
			return SCROLL_PT_UP;
		T = iBottomPos;
		B = m_y+m_h;
		if( PtInRect( x,y,L,T,R,B ) )
			return SCROLL_PT_DOWN;
		if ((iBtnTop != 0) && (iBtnButtom != 0))
		{
			T = iTopPos;
			B = iBtnTop;
			if( PtInRect( x,y,L,T,R,B ) )
				return SCROLL_PT_PAGEUP;
			T = iBtnButtom;
			B = iBottomPos;
			if( PtInRect( x,y,L,T,R,B ) )
				return SCROLL_PT_PAGEDOWN;
			T = iBtnTop;
			B = iBtnButtom;
			if( PtInRect( x,y,L,T,R,B ) )
				return SCROLL_PT_BUTTON;
		}
	}
	else if (m_iMode == 2)  // 水平
	{
		int iLeftPos = m_x + 11;
		int iRightPos = m_x + m_w - 11;
		int iBtnLeft = 0;
		int iBtnRight = 0;

		int iWidth = iRightPos - iLeftPos;
		if (iWidth >= 14)
		{
			// 计算按钮的绘制位置和长宽
			int iBtnWidth = iWidth * m_iSize / m_iRange;	// 高度
			if (iBtnWidth < 11)		// 中间按钮至少11个像素的宽度
			{
				iBtnWidth = 11;
			}
			iBtnLeft  = iLeftPos + (iWidth - iBtnWidth) * m_iPos / (m_iRange-m_iSize);
			iBtnRight = iBtnLeft + iBtnWidth;
		}

		R = iLeftPos;
		if( PtInRect( x,y,L,T,R,B ) )
			return SCROLL_PT_UP;
		L = iRightPos;
		R = m_x+m_w;
		if( PtInRect( x,y,L,T,R,B ) )
			return SCROLL_PT_DOWN;
		if ((iBtnLeft != 0) && (iBtnRight != 0))
		{
			L = iLeftPos;
			R = iBtnLeft;
			if( PtInRect( x,y,L,T,R,B ) )
				return SCROLL_PT_PAGEUP;
			L = iBtnRight;
			R = iRightPos;
			if( PtInRect( x,y,L,T,R,B ) )
				return SCROLL_PT_PAGEDOWN;
			L = iBtnLeft;
			R = iBtnRight;
			if( PtInRect( x,y,L,T,R,B ) )
				return SCROLL_PT_BUTTON;
		}
	}

	return 0;
}

// 根据鼠标坐标计算对应的新位置
int CTScroll::TestNewPos (int x, int y)
{
	if (m_iStatus == 0)		// 不显示
		return -1;

	if (m_iMode == 1)		// 垂直滚动条
	{
		// 计算鼠标有效范围
		int L = m_x-30;
		int T = m_y-10;
		int R = m_x+m_w+30;
		int B = m_y+m_h+10;

		if( !PtInRect( x,y,L,T,R,B ) )
			return m_iOldPos;

		int iTopPos = m_y+11;
		int iBottomPos = m_y+m_h-11;
		int iHeight = iBottomPos - iTopPos;
		if (iHeight >= 14)
		{
			// 计算按钮的绘制位置和长宽
			int iBtnHeight = iHeight * m_iSize / m_iRange;	// 高度
			if (iBtnHeight < 11)		// 中间按钮至少11个像素的宽度
			{
				iBtnHeight = 11;
			}

			int iNewPos = m_iOldPos-(m_iOldPt-y)*(m_iRange-m_iSize)/(iHeight-iBtnHeight);

			if( iNewPos < 0 )
				iNewPos = 0;
			else if( iNewPos > (m_iRange-m_iSize) )
				iNewPos = m_iRange-m_iSize;

			return iNewPos;
		}
	}
	else if (m_iMode == 2)	// 水平滚动条
	{
		// 计算鼠标有效范围
		int L = m_x-10;
		int T = m_y-30;
		int R = m_x+m_w+10;
		int B = m_y+m_h+30;

		if( !PtInRect( x,y,L,T,R,B ) )
			return m_iOldPos;

		int iLeftPos = m_x+11;
		int iRightPos = m_x+m_w-11;
		int iWidth = iRightPos - iLeftPos;
		if (iWidth >= 14)
		{
			// 计算按钮的绘制位置和长宽
			int iBtnWidth = iWidth * m_iSize / m_iRange;	// 高度
			if (iBtnWidth < 11)		// 中间按钮至少11个像素的宽度
			{
				iBtnWidth = 11;
			}

			int iNewPos = m_iOldPos-(m_iOldPt-x)*(m_iRange-m_iSize)/(iWidth-iBtnWidth);

			if( iNewPos < 0 )
				iNewPos = 0;
			else if( iNewPos > (m_iRange-m_iSize) )
				iNewPos = m_iRange-m_iSize;

			return iNewPos;
		}
	}

	return -1;
}
#endif // defined(MOUSE_SUPPORT)
/* END */
