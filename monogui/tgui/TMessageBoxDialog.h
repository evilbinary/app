// TMessageBoxDialog.h: interface for the CTMessageBoxDialog class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TMESSAGEBOXDIALOG_H__)
#define __TMESSAGEBOXDIALOG_H__

#define TB_ERROR            0x0001		// 显示错误图标
#define TB_EXCLAMATION      0x0002		// 显示感叹号图标
#define TB_QUESTION         0x0004		// 显示问号图标
#define TB_INFORMATION      0x0008		// 显示信息图标
#define TB_BUSY             0x0010		// 显示漏壶图标
#define TB_PRINT            0x0020		// 显示打印机图标
#define TB_ROUND_EDGE       0x0040		// 圆角风格
#define TB_SOLID            0x0080		// 立体风格
#define TB_YESNO            0x0100		// 显示“是”“否”按钮
#define TB_OKCANCEL         0x0200		// 显示“确定”“取消”按钮
#define TB_DEFAULT_NO       0x0400
// 如果对话框的样式是MB_YESNO或MB_OKCANCEL，则对话框创建后默认的焦点是“是”按钮
// 或者“确定”按钮。如果使用“否”和“取消”为默认按钮，则指定MB_DEFAULT_NO样式
#define TB_ENGLISH          0x0800
// 按钮文字显示为英文  确定：OK  取消：CANCEL  是：YES  否：NO

class CTMessageBoxDialog : public CTDialog
{
private:
	char m_cInformation[256];	// 信息字符串
	WORD m_wMessageBoxStyle;	// 对话框样式

public:
	CTMessageBoxDialog ();
	virtual ~CTMessageBoxDialog ();

	// 虚函数，绘制对话框
	virtual void Paint(CLCD* pLCD);

	// 虚函数，消息处理
	// 消息处理过了，返回1，未处理返回0
	virtual int Proc(CTWindow* pWnd, int iMsg, int wParam, int lParam);

	// 创建MessageBox
	BOOL Create(CTWindow* pParent, char* cTitle, char* cText, WORD wMessageBoxStyle, int ID);
};

#endif // !defined(__TMESSAGEBOXDIALOG_H__)
