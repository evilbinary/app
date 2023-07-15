// TCombo.h: interface for the CTCombo class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TCOMBO_H__)
#define __TCOMBO_H__

#define COMBO_DISABLE_EDIT				0x00000001
#define COMBO_DISABLE_DROPDOWN			0x00000002
#define COMBO_UN_DROPPED				0
#define COMBO_DROPPED					1
#define COMBO_PUBHDOWN_BUTTON_WIDTH		11	// EDIT�Ҳ�С��ť��Ĭ�Ͽ��
#define COMBO_DEFAULT_LIST_HEIGHT		42	// LIST�ؼ���Ĭ�ϸ߶�(����)
#define COMBO_ID_EDIT					100	// EDIT�ؼ���ID��
#define COMBO_ID_LIST					101	// LIST�ؼ���ID��

class CTEdit;
class CTList;

class CTCombo : public CTWindow
{
private:
	int m_dwExtStatus;       // ��չ״̬��ָ����Combo��Edit��List�Ƿ�ʹ��
	int m_iDropDownStatus;   // �����б��ĵ���״̬

	CTEdit* m_pEdit;
	CTList* m_pList;

public:
	CTCombo();
	virtual ~CTCombo();

public:
	// ������Ͽ�
	virtual BOOL Create
	(
		CTWindow* pParent,
		WORD wWndType,
		WORD wStyle,
		WORD wStatus,
		int x,
		int y,
		int w,
		int h,
		int ID
	);

	// ������Ͽ�
	virtual void Paint(CLCD* pLCD);

	// ��Ͽ���Ϣ����
	virtual int Proc(CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// �����豸��Ϣ����
	virtual int PtProc(CTWindow* pWnd, int iMsg, int wParam, int lParam);
#endif // defined(MOUSE_SUPPORT)

	// ��ʾ�������������б��
	BOOL ShowDropDown( BOOL bShow );

	// �õ������б�����ʾ״̬
	BOOL GetDroppedState();

	// ���������б��ĸ߶�(��������)
	BOOL SetDroppedLinage( int iLinage );

	// ������߽�ֹ�Ա༭����������
	BOOL EnableEdit( BOOL bEnable );

	// ������߽�ֹ�����˵�����
	BOOL EnableDropDown( BOOL bEnable );

	// ���º������ڲ����༭��
	// �������ֵĳ���
	int LimitText( int iLength );

	// ��ձ༭��
	BOOL Clean();

	// ȡ�ñ༭�������
	BOOL GetText( char* pText );

	// ���ñ༭�������
	BOOL SetText( char* pText, int iLength );

	// ȡ�ñ༭�����ֵĳ���
	int GetTextLength();

// ���º������ڲ��������б��
	// �õ��б����Ŀ��
	int GetCount();

	// �õ��б��ǰѡ����Ŀ��Index�����û��ѡ�е��򷵻�-1
	int GetCurSel();

	// �����б��ǰ��ѡ����Ŀ
	int SetCurSel( int iIndex );

	// ���ĳһ�б��������
	BOOL GetString( int iIndex, char* pText );

	// ����ĳһ�б��������
	BOOL SetString( int iIndex, char* pText );

	// ���ĳһ�б������ݵĳ���
	int GetStringLength( int iIndex );

	// ���б�������һ����(����ĩβ)
	BOOL AddString( char* pText );

	// ɾ���б���һ���б���
	BOOL DeleteString( int iIndex );

	// ���б���ָ��λ�ò���һ����
	BOOL InsertString( int iIndex, char* pText );

	// ɾ���б��������б���
	BOOL RemoveAll();

	// ���б���в���һ����
	int FindString( char* pText );

	// �����б������ݸ��±༭��
	void SelectString( char* pText );

	// ��List��ǰ���ַ���������Edit��
	void SyncString();

private:
	// Edit�õ�����
	void EditSetFocus();

	// Editʧȥ����
	void EditKillFocus();

	// List�õ�����
	void ListSetFocus();

	// Listʧȥ����
	void ListKillFocus();

	// �ж��Ƿ������༭������
	BOOL CanEdit();

	// �ж��Ƿ���Ե����б��
	BOOL CanDropDown();
};

#endif // !defined(__TCOMBO_H__)
