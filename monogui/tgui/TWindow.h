// TWindow.h: interface for the CTWindow class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TWINDOW_H__)
#define __TWINDOW_H__

class CLCD;
class CTAccell;
class CTCaret;
class CTScroll;
class CTApp;

class CTWindow
{
public:
	WORD m_wWndType;                // ��������
	WORD m_wStyle;                  // ���ڵ���ʽ
	WORD m_wStatus;                 // ���ڵ�״̬
	int m_x, m_y, m_w, m_h;         // ��������洰��(MainWindow)�ľ���λ��
	int m_iTabOrder;                // Ϊ�Ӵ���ʱ��Tab���
	CTScroll* m_pVScroll;           // ���������
	CTScroll* m_pHScroll;           // ���������
	int m_ID;                       // ���ڵ�ID��
	char m_cCaption[256];           // ��ͷ

	/* Ӧ�ó���͸����ڵ�ָ�� */
	CTApp*    m_pApp;               // Ӧ�ó����ָ��
	CTWindow* m_pParent;            // �����ڵ�ָ��

	/* �Ӵ�����Ϣ */
	int m_iChildCount;              // �Ӵ�����Ŀ
	CTWindow* m_pChildren;          // ��һ���Ӵ���(�Ӵ�����˫��������)
	CTWindow* m_pActive;            // ���ڵ�ǰ������Ӵ���(������NULL)
	CTWindow* m_pDefaultButton;     // ���ڵ�Ĭ�ϰ�ť

	/* �ֵܴ��� */
	CTWindow* m_pNext;              // ָ����һ���ֵܴ���
	CTWindow* m_pPrev;              // ָ����һ���ֵܴ���
	//
	CTWindow* m_pOldParentActiveWindow;    // �����ڹر�ʱ��Ӧ���ָ��������ϵĽ���

#if defined(MOUSE_SUPPORT)
	int m_iMouseMoving;				// �ػ�����ƶ���Ϣ�ı�־
#endif // defined(MOUSE_SUPPORT)

public:
	CTWindow();
	virtual ~CTWindow();

	// ����һ�����ڣ�
	// ��������ڲ���NULL����Ѹô��ڲ��븸���ڵ��ִ��������У�
	// tab��Ÿ��ݸ����ڵ��ִ�����Ŀȷ����
	// ��ݼ��б����ַ�������������ͷ���ֿ����ڴ��ڴ������ٽ������ã�
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

	// �麯�������ƴ��ڣ�ֻ���Ƹ��ӵĹ�����
	virtual void Paint (CLCD* pLCD);

	// �麯������Ϣ����
	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc (CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// �麯������Ϣ����
	virtual int PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam);
#endif // defined(MOUSE_SUPPORT)

	// �ı䴰�ڵ�λ�úͳߴ�
	// ���óɹ�����TRUE��ʧ�ܷ���FALSE
	BOOL SetPosition (int x, int y, int w, int h);

	// ��ô��ڵ�λ�úͳߴ�
	BOOL GetPosition (int* px, int* py, int* pw, int* ph);

	// ��ô��ڵ�ID��
	int GetID ();

	// ���ô��ڵ���ͷ
	BOOL SetText (char* pText, int iLength);

	// ��ȡ���ڵ���ͷ
	BOOL GetText (char* pText);

	// ��ô�����ͷ�ַ����ĳ���
	int GetTextLength ();

	// ���õ�ǰ���ڻ״̬�����ڽ��㣩���Ӵ���
	// ע�⣺Ӧ������ǰ���ڻ״̬���Ӵ��ڸĳɷǻ��
	// ����ʵ�ʻ�ý���Ŀؼ���ID��
	int SetFocus (CTWindow* pWnd);

	// ���Ӵ��������в�����ӦID�Ĵ���
	CTWindow* FindChildByID (int id);

	// ���Ӵ��������в�����Ӧtab�ŵĴ���
	CTWindow* FindChildByTab (int iTab);

	// ���Ӵ��������в���ԭʼĬ�ϰ�ť
	CTWindow* FindDefaultButton ();

	// ������λ��һ���Ӵ��ڴ�ֱ���·����Ӵ���
	CTWindow* FindWindowDown( CTWindow* pWnd );

	// ������λ��һ���Ӵ��ڴ�ֱ���Ϸ����Ӵ���
	CTWindow* FindWindowUp( CTWindow* pWnd );

	// ������λ��һ���Ӵ���ˮƽ���󷽵��Ӵ���
	CTWindow* FindWindowLeft( CTWindow* pWnd );

	// ������λ��һ���Ӵ���ˮƽ���ҷ����Ӵ���
	CTWindow* FindWindowRight( CTWindow* pWnd );
	  
	// ɾ��һ���Ӵ���
	// ��Ӧ��Ҫ���¸������ڵ�tab���
	BOOL DeleteChild (CTWindow* pChild);

	// ���ô��ڵĹ�����
	// iMode = 1�������Ҳ�Ĵ�ֱ��������
	// iMode = 2�������·���ˮƽ��������
	// �������ĳߴ���ݴ��ڳߴ��������
	// ���Ҫ���õĹ������������ڣ��򴴽�
	BOOL SetScrollBar (int iMode, int iRange, int iSize, int iPos);

	// ���ƹ���������ʾ������
	// iScroll = 1�����ô�ֱ��������iScroll = 2������ˮƽ��������
	// ���Ҫ���õĹ������������ڣ��򷵻�FALSE
	BOOL ShowScrollBar (int iScroll, BOOL bShow);

	// �����淢���ػ洰�ڵ���Ϣ
	BOOL UpdateView (CTWindow* pWnd);

	// ����ʹ��
 	BOOL EnableWindow( BOOL bEnable );

	// �жϴ����Ƿ�ʹ��
	BOOL IsWindowEnabled();

	// �жϴ����Ƿ�ɼ�
	BOOL IsWindowVisible();

	// �жϴ����Ƿ񽹵����ϵĴ���
	BOOL IsWindowActive();

#if defined(MOUSE_SUPPORT)
	// �ж�һ�����Ƿ����ڱ����ڵķ�Χ֮��
	BOOL PtInWindow (int x, int y);
#endif // defined(MOUSE_SUPPORT)

// ��Ҫ����Ϣ������

	// ��ʱ������
	virtual void OnTimer (int iTimerID, int iInterval);

	// ���ڴ�����ĳ�ʼ������
	virtual void OnInit();

	// �رմ��ڣ����Թر��򷵻�TRUE�������Թرշ���FALSE
	virtual BOOL OnClose ();

	// �Ӵ��ڵõ�����
	virtual void OnSetFocus (int id);

	// �Ӵ���ʧȥ����
	virtual void OnKillFocus (int id);

	// �Ӵ������ݸı�
	virtual void OnDataChange (int id);

// ��������йصĺ���
#if defined(MOUSE_SUPPORT)
	virtual void OnScrollUp();
	virtual void OnScrollDown();
	virtual void OnScrollLeft();
	virtual void OnScrollRight();
	virtual void OnScrollPageUp();
	virtual void OnScrollPageDown();
	virtual void OnScrollPageLeft();
	virtual void OnScrollPageRight();
	virtual void OnVScrollNewPos(int iNewPos);
	virtual void OnHScrollNewPos(int iNewPos);
#endif // defined(MOUSE_SUPPORT)
};

#endif // !defined(__TWINDOW_H__)

