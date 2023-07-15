// TMessageBoxDialog.cpp: implementation of the TMessageBoxDialog class.
//
//////////////////////////////////////////////////////////////////////
#if defined(RUN_ENVIRONMENT_LINUX)
#include <string.h>
#endif

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTMessageBoxDialog::CTMessageBoxDialog ()
{
	memset (m_cInformation, 0x0, 256);
	m_wMessageBoxStyle = 0x0;
}

CTMessageBoxDialog::~CTMessageBoxDialog ()
{
}


// 虚函数，绘制对话框
void CTMessageBoxDialog::Paint(CLCD* pLCD)
{
	// 如果不可见，则什么也不绘制
	if( !IsWindowVisible() )
		return;

	// 首先调用基类的绘制函数重绘各控件
	CTDialog::Paint(pLCD);

	int w;
	int h;
	unsigned char* fb = pLCD->LCDGetFB (&w, &h);

	if (m_wMessageBoxStyle > 0)
	{
		unsigned char* pIcon = NULL;

		if ((m_wMessageBoxStyle & TB_ERROR) > 0)
		{
			pIcon = pBitmap_Icon_Error;
		}
		else if ((m_wMessageBoxStyle & TB_EXCLAMATION) > 0)
		{
			pIcon = pBitmap_Icon_Exclamation;
		}
		else if ((m_wMessageBoxStyle & TB_QUESTION) > 0)
		{
			pIcon = pBitmap_Icon_Question;
		}
		else if ((m_wMessageBoxStyle & TB_INFORMATION) > 0)
		{
			pIcon = pBitmap_Icon_Information;
		}

		if (pIcon != NULL)
		{
			pLCD->LCDDrawImage(fb,w,h,m_x+10,m_y+18,23,23,0,pIcon,23,23,0,0);
		}
	}
}

// 虚函数，消息处理
// 消息处理过了，返回1，未处理返回0
int CTMessageBoxDialog::Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam)
{
	if( !IsWindowEnabled() )
		return 0;

	// 屏蔽ESC键
	if ((iMsg == TMSG_KEYDOWN) && (wParam == TVK_ESCAPE))
		return 1;

	CTDialog::Proc( pWnd, iMsg, wParam, lParam );

	if ((pWnd == this) && (iMsg == TMSG_NOTIFY_PARENT))	// 按钮被按下
	{
		_TMSG msg;

		// 关闭MessageBox对话框
		msg.pWnd    = m_pParent;
		msg.message = TMSG_DELETECHILD;
		msg.wParam  = m_ID;
		msg.lParam  = (int)m_pOldParentActiveWindow;
		m_pApp->PostMessage( &msg );
		
		//发送消息,让默认的Active窗口获得焦点
		if( m_pOldParentActiveWindow != NULL )
		  {
		    msg.pWnd    = m_pOldParentActiveWindow;
		    msg.message = TMSG_SETFOCUS;
		    msg.wParam  = m_pOldParentActiveWindow->m_ID;
		    msg.lParam  = 0;
		    m_pApp->PostMessage( &msg );
		  }

		// 向父窗口发送消息，通知被按下的按钮
		msg.pWnd    = m_pParent;
		msg.message = TMSG_MESSAGEBOX_RETURN;
		msg.wParam  = m_ID;
		msg.lParam  = wParam;
		m_pApp->PostMessage( &msg );

		// 记录按下的按钮
		m_iDoModalReturn = wParam;
	}
	return 1;	// 禁止其他消息向父窗口传递
}

// 创建MessageBox
BOOL CTMessageBoxDialog::Create (CTWindow* pParent, char* cTitle, char* cText, WORD wMessageBoxStyle, int ID )
{
	memset (m_cInformation, 0x0, 256);
	strncpy (m_cInformation, cText, 255);
	m_wMessageBoxStyle = wMessageBoxStyle;

	WORD wStyle = TWND_STYLE_NORMAL;
	if ((wMessageBoxStyle & TB_ROUND_EDGE) > 0)
	{
		m_wMessageBoxStyle &= ~TB_ROUND_EDGE;	// 去掉圆角属性，绘制时可以减少判断
		wStyle |= TWND_STYLE_ROUND_EDGE;
	}
	if ((wMessageBoxStyle & TB_SOLID) > 0)
	{
		m_wMessageBoxStyle &= ~TB_SOLID;		// 去掉立体属性，绘制时可以减少判断
		wStyle |= TWND_STYLE_SOLID;
	}

	// 计算显示文字可能会占用的空间
	int iWidth = 0;
	int iHeight = 0;
	char cTemp[256];

	// 最多不能超过四行文本
	int i;
	for (i=0; i<4; i++)
	{
		memset (cTemp, 0x0, 256);
		if (GetLine (m_cInformation, cTemp, i))
		{
			int iDesplayLength = GetDisplayLength (cTemp, 255);
			if (iDesplayLength > iWidth)
				iWidth = iDesplayLength;

			iHeight += 14;
		}
	}

	if (iWidth > (SCREEN_W - 40))
		iWidth = SCREEN_W - 40;

	int iTextLeft = 10;
	int tw = iWidth + 24;
	int th = iHeight + 57;
	if (m_wMessageBoxStyle > 0)
	{
		tw += 30;
		iTextLeft += 30;
	}
	if (tw < 80)
		tw = 80;

	int tx = pParent->m_x + (pParent->m_w - tw) / 2;
	int ty = pParent->m_y + (pParent->m_h - th) / 2;

	// 创建MessageBox对话框
	BOOL bSuccess = CTDialog::Create (pParent, TWND_TYPE_DIALOG, wStyle, TWND_STATUS_FOCUSED, tx, ty, tw, th, ID );

	if( !bSuccess )
		return FALSE;

	// DebugPrintf( "TMessageBoxDialog: cTitle = %s \n", cTitle );
	SetText (cTitle, 80);

	// 为每一行文本创建一个CTStatic控件
	// 最多不能超过四行文本
	for (i=0; i<4; i++)
	{
		memset (cTemp, 0x0, 256);
		if (GetLine (m_cInformation, cTemp, i))
		{
			CTStatic* pStatic = new CTStatic ();
			pStatic->Create (this, TWND_TYPE_STATIC, TWND_STYLE_NO_BORDER, TWND_STATUS_INVALID, m_x+iTextLeft, m_y+21+i*14, iWidth+36, 14, 12);
			//DebugPrintf( "Debug: Static: cTemp = %s \n", cTemp );
			pStatic->SetText (cTemp, 32);
		}
	}

	// 创建按钮
	// 如果未指定按钮样式，则创建一个“确定”按钮
	// 如果指定了TB_YESNO样式，则创建“是”“否”两个按钮
	// 如果指定了TB_OKCANCEL样式，则创建“确定”“取消”两个按钮
	// 创建确定按钮
	if ((wMessageBoxStyle & TB_YESNO) > 0)
	{
		CTButton* pYesButton = new CTButton ();
		pYesButton->Create( this, TWND_TYPE_BUTTON, wStyle, TWND_STATUS_FOCUSED, m_x+m_w/2-50, m_y+m_h-27, 40, 20, TSID_YES );

		// 支持中英文两种按钮
#if defined(CHINESE_SUPPORT)
		if( (wMessageBoxStyle & TB_ENGLISH) == 0 )
			pYesButton->SetText( "是", 2 );
		else
#endif // defined(CHINESE_SUPPORT)

			pYesButton->SetText( "YES", 3 );

		CTButton* pNoButton = new CTButton ();
		pNoButton->Create( this, TWND_TYPE_BUTTON, wStyle, TWND_STATUS_FOCUSED, m_x+m_w/2+10, m_y+m_h-27, 40, 20, TSID_NO );

		// 支持中英文两种按钮
#if defined(CHINESE_SUPPORT)
		if( (wMessageBoxStyle & TB_ENGLISH) == 0 )
			pNoButton->SetText( "否", 2 );
		else
#endif // defined(CHINESE_SUPPORT)

			pNoButton->SetText( "NO", 2 );

		// 如果设定了MB_DEFAULT_NO样式，则将“否”设置为默认按钮
		if( (wMessageBoxStyle & TB_DEFAULT_NO) == 0 )
		{
			SetFocus( pYesButton );
		}
		else
		{
			SetFocus( pNoButton );
		}
	}
	else if ((wMessageBoxStyle & TB_OKCANCEL) > 0)
	{
		CTButton* pOkButton = new CTButton ();
		pOkButton->Create( this, TWND_TYPE_BUTTON, wStyle, TWND_STATUS_FOCUSED, m_x+m_w/2-70, m_y+m_h-27, 60, 20, TSID_OK );

		// 支持中英文两种按钮
#if defined(CHINESE_SUPPORT)
		if( (wMessageBoxStyle & TB_ENGLISH) == 0 )
			pOkButton->SetText( "确定", 4 );
		else
#endif // defined(CHINESE_SUPPORT)

			pOkButton->SetText( "OK", 2 );

		CTButton* pCancelButton = new CTButton ();
		pCancelButton->Create( this, TWND_TYPE_BUTTON, wStyle, TWND_STATUS_FOCUSED, m_x+m_w/2+10, m_y+m_h-27, 60, 20, TSID_CANCEL );

		// 支持中英文两种按钮
#if defined(CHINESE_SUPPORT)
		if( (wMessageBoxStyle & TB_ENGLISH) == 0 )
		      pCancelButton->SetText ("取消", 4);
		else
#endif // defined(CHINESE_SUPPORT)

		      pCancelButton->SetText( "CANCEL", 6 );

		// 如果设定了MB_DEFAULT_NO样式，则将“取消”设置为默认按钮
		if( (wMessageBoxStyle & TB_DEFAULT_NO) == 0 )
		{
			SetFocus( pOkButton );
		}
		else
		{
			SetFocus( pCancelButton );
		}
	}
	else
	{
		CTButton* pOkButton = new CTButton ();
		pOkButton->Create( this, TWND_TYPE_BUTTON, wStyle, TWND_STATUS_FOCUSED, m_x+m_w/2-30, m_y+m_h-27, 60, 20, TSID_OK );

		// 支持中英文两种按钮
#if defined(CHINESE_SUPPORT)
		if( (wMessageBoxStyle & TB_ENGLISH) == 0 )
			pOkButton->SetText( "确定", 4 );
		else
#endif // defined(CHINESE_SUPPORT)

			pOkButton->SetText( "OK", 2 );
	}

	return TRUE;
}
/* END */
