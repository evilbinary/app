// TEdit.h: interface for the CTEdit class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TEDIT_H__)
#define __TEDIT_H__

#define IME_FORBID	0
#define IME_CLOSE	1
#define IME_OPEN	2

class CTWindow;
class CIMEWindow;

class CTEdit : public CTWindow
{
private:
	int m_iSelStart;     // ������ʼλ��
	int m_iSelEnd;       // ������ֹλ��
	int m_iTextLimit;    // �ַ���Ĭ����󳤶�
	int m_iLeftIndex;    // ��ʾ������˵ĵ�һ���ַ���λ��

	struct _CARET m_Caret;	// ���ַ�
	char m_cIMEStatus;		// ���뷨״̬��0�������뷨��1���뷨δ����2���뷨�ѿ�

#if defined(CHINESE_SUPPORT)
	CIMEWindow* m_pIME;			// ���뷨���ڵ�ָ��
#endif // defined(CHINESE_SUPPORT)

#if defined(MOUSE_SUPPORT)
	int m_iOldPos;            // ���ڼ�¼����ѡ�ĳ�ʼλ��
#endif // defined(MOUSE_SUPPORT)

public:
	CTEdit ();
	virtual ~CTEdit ();

	// �����༭��
	virtual BOOL Create
	(
		CTWindow* pParent,    // ������ָ��
		WORD wWinType,        // ��������
		WORD wStyle,          // ���ڵ���ʽ
		WORD wStatus,         // ���ڵ�״̬
		int x,
		int y,
		int w,
		int h,                // ����λ��
		int ID                // ���ڵ�ID��
	);

	// �麯�������Ʊ༭��
	virtual void Paint (CLCD* pLCD);

	// �麯������Ϣ����
	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// �����豸��Ϣ����
	virtual int PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

	// �����������ڵ�λ�ã��ò��Բ�����yֵ
	int PtInItems( int x );
#endif // defined(MOUSE_SUPPORT)

	// ���õ�ǰѡ���������ʼλ�ú���ֹλ��
	// ���λ�ÿ�Խ�˺��֣��������һ���ֽ�
	BOOL SetSel (int iStart, int iEnd);

	// ��õ�ǰѡ���������ʼλ�ú���ֹλ��
	BOOL GetSel (int* iStart, int* iEnd);

	// ��ǰλ�ò����ַ���
	// �����ǰλ����һ��ѡ���������滻��ǰѡ��������ַ�����
	// Ȼ��ѡ�����޸ĳ�һ������λ��
	// �����ǰλ����һ������λ�ã����ڴ˲���λ���ϲ����ַ���
	// ע�⣬����ܳ��ȳ�Խ�˳������ƣ����ȡ���ʳ��ȵ��ִ�
	// ��ȡʱӦע�⺺�ֵĴ���
	BOOL InsertCharacter (char* cString);

	// ɾ����ǰλ��ǰ���һ���ַ����ߺ����һ���ַ�
	// bMode: TRUE,ɾ�����;FALSE,ɾǰ���
	BOOL DelOneCharacter (BOOL bMode);

	// ɾ����ǰѡ�����������
	BOOL DelCurSel ();

	// ���������ַ�������󳤶�
	BOOL LimitText (int iLength);

	// ����ַ���������
	BOOL Clean ();

	// �鿴���뷨�����Ƿ��
	BOOL IsIMEOpen ();
private:
	// ������ʾ����˵�һ���ַ�������
	void RenewLeftPos ();

	// ���ݵ�ǰ�����ַ����ø���ϵͳ���ַ�
	void RenewCaret ();

	// ȡ�õ�ǰ��ʾ�������Ҷ��ַ�������
	int GetRightDisplayIndex ();

	// ����ĸ��ת����ASC��
	BOOL TVKToASC( char* psString, int nVK, int nMask );

};

#endif // !defined(__TEDIT_H__)
