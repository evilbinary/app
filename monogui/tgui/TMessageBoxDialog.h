// TMessageBoxDialog.h: interface for the CTMessageBoxDialog class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TMESSAGEBOXDIALOG_H__)
#define __TMESSAGEBOXDIALOG_H__

#define TB_ERROR            0x0001		// ��ʾ����ͼ��
#define TB_EXCLAMATION      0x0002		// ��ʾ��̾��ͼ��
#define TB_QUESTION         0x0004		// ��ʾ�ʺ�ͼ��
#define TB_INFORMATION      0x0008		// ��ʾ��Ϣͼ��
#define TB_BUSY             0x0010		// ��ʾ©��ͼ��
#define TB_PRINT            0x0020		// ��ʾ��ӡ��ͼ��
#define TB_ROUND_EDGE       0x0040		// Բ�Ƿ��
#define TB_SOLID            0x0080		// ������
#define TB_YESNO            0x0100		// ��ʾ���ǡ����񡱰�ť
#define TB_OKCANCEL         0x0200		// ��ʾ��ȷ������ȡ������ť
#define TB_DEFAULT_NO       0x0400
// ����Ի������ʽ��MB_YESNO��MB_OKCANCEL����Ի��򴴽���Ĭ�ϵĽ����ǡ��ǡ���ť
// ���ߡ�ȷ������ť�����ʹ�á��񡱺͡�ȡ����ΪĬ�ϰ�ť����ָ��MB_DEFAULT_NO��ʽ
#define TB_ENGLISH          0x0800
// ��ť������ʾΪӢ��  ȷ����OK  ȡ����CANCEL  �ǣ�YES  ��NO

class CTMessageBoxDialog : public CTDialog
{
private:
	char m_cInformation[256];	// ��Ϣ�ַ���
	WORD m_wMessageBoxStyle;	// �Ի�����ʽ

public:
	CTMessageBoxDialog ();
	virtual ~CTMessageBoxDialog ();

	// �麯�������ƶԻ���
	virtual void Paint(CLCD* pLCD);

	// �麯������Ϣ����
	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc(CTWindow* pWnd, int iMsg, int wParam, int lParam);

	// ����MessageBox
	BOOL Create(CTWindow* pParent, char* cTitle, char* cText, WORD wMessageBoxStyle, int ID);
};

#endif // !defined(__TMESSAGEBOXDIALOG_H__)
