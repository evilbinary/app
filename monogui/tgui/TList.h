// TList.h: interface for the CTList class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TLIST_H__)
#define __TLIST_H__

#define LIST_TEXT_MAX_LENGTH	256		// һ���б����ʾ���ݵĳ���
#define LIST_ITEM_H       (HZK_H+1)		// ��Ŀ�ĸ߶�

typedef struct _LISTCONTENT
{
	char text[ LIST_TEXT_MAX_LENGTH ];	// �����б����һ���ı�����
	struct _LISTCONTENT *next;			// ��������һ���б��������
}LISTCONTENT;							// ���ڱ���һ���б�������ݵ����ݽṹ

class CTList : public CTWindow  
{
private:
	int m_iItemCount;  // ���ڴ�ŵ�ǰ�ж������б����ʼΪ��
	int m_iTopIndex;   // ���ڴ�ŵ�ǰ��ʾ�б���������һ����Indexֵ����ʼΪ��
	int m_iCurSel;     // ���ڴ�ŵ�ǰѡ�е��б����ʼΪ��1

	struct _LISTCONTENT* m_pContent;	// ָ����������

public:
	int m_iHeightOfLinage;     // �б���ܹ���ʾ������

public:
	CTList ();
	virtual ~CTList ();

public:
	// ��������
	virtual BOOL Create
	(
		CTWindow* pParent,			// ������ָ��
		WORD wWinType,				// ��������
		WORD wStyle,				// ���ڵ���ʽ
		WORD wStatus,				// ���ڵ�״̬
		int x,
		int y,
		int w,
		int h,						// ����λ��
		int ID						// ���ڵ�ID��
	);

	// �������뷨����
	virtual void Paint (CLCD* pLCD);

	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// �����豸��Ϣ����
	virtual int PtProc(CTWindow* pWnd, int iMsg, int wParam, int lParam);

	// �����������ڵ���Ŀ��-1��ʾδ�����κ���Ŀ
	int PtInItems( int x, int y );
#endif // defined(MOUSE_SUPPORT)

	// �����Ŀ������
	int GetCount();

	// ��õ�ǰ��ʾ����������һ����Ŀ��Index
	int GetTopIndex();

	// ���õ�ǰ��ʾ����������һ����Ŀ��Index
	int SetTopIndex(int iIndex);

	// �õ���ǰѡ����Ŀ��Index�����û��ѡ�е��򷵻�-1
	int GetCurSel();

	// ���õ�ǰ��ѡ����Ŀ
	int SetCurSel(int iIndex);

	// ���ĳһ�б��������
	BOOL GetString(int iIndex, char* pText);

	// ����ĳһ�б��������
	BOOL SetString(int iIndex, char* pText);

	// ���ĳһ�б������ݵĳ���
	int GetStringLength(int iIndex);

	// ���б�������һ����(����ĩβ)
	BOOL AddString(char* pText);

	// ɾ��һ���б���
	BOOL DeleteString(int iIndex);

	// ��ָ��λ�ò���һ����
	BOOL InsertString(int iIndex, char* pText);

	// ɾ�������б���
	BOOL RemoveAll();

	// ���б����в���һ����
	int FindString(char* pText);

	// ���б����в���һ����������ҵ�����������Ϊѡ�У�����ʾ�ڵ�һ��
	// (��������һҳ���򲻱ط��ڵ�һ�У��������ĵ�)
	int SelectString(char* pText);

	// �����б��ĸ߶�Ϊ����
	BOOL SetLinage(int iLinage);

	// ���¹�����
	void RenewScroll();

#if defined(MOUSE_SUPPORT)
	// ��������йصĺ���
	virtual void OnScrollUp ();
	virtual void OnScrollDown ();
	virtual void OnScrollPageUp ();
	virtual void OnScrollPageDown ();
	virtual void OnVScrollNewPos (int iNewPos);
#endif // defined(MOUSE_SUPPORT)
};

#endif // !defined(__TLIST_H__)
