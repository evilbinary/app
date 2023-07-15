// TApp.h: interface for the CTApp class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TAPP_H__)
#define __TAPP_H__

class CLCD;
class CTMessageQueue;
class CTWindow;
class CTCaret;
class CTTimerQueue;
class CIMEWindow;
class CTKeyMapMgt;
class CTClock;
class CTSystemBar;
class CHKDevice;
class CTDtmMgt;
class CTImgMgt;

class CTApp
{
public:
	CLCD*              m_pLCD;             // ��ʾ�豸
	CLCD*              m_pLCDBuf;          // ����ʾ������
	CTMessageQueue*    m_pMsgQ;            // ��Ϣ����
	CTWindow*          m_pMainWnd;         // ������
	CTCaret*           m_pCaret;           // ���ַ�
	CTTimerQueue*      m_pTimerQ;          // ��ʱ������
	CTWindow*          m_pTopMostWnd;      // TopMost����
	CTKeyMapMgt*       m_pKeyMap;          // �������밴��ֵ���ձ�
	CTClock*           m_pClock;           // �ӱ���
	CTSystemBar*       m_pSysBar;          // ϵͳ״̬��

#if defined(CHINESE_SUPPORT)
	CIMEWindow*        m_pIMEWnd;          // ���뷨����
#endif // defined(CHINESE_SUPPORT)

#if defined(RESMGT_SUPPORT)
	CTDtmMgt*          m_pDtmMgt;          // dtm�ļ�������
	CTImgMgt*          m_pImgMgt;          // �ڰ�λͼ������
#endif // defined(RESMGT_SUPPORT)

	BOOL m_bClockKeyEnable;           // �Ƿ�ʹ��ʱ�Ӵ���

public:
	CTApp();
	virtual ~CTApp();

	// ��ָ���Ĵ��ڽ��л���
	virtual void PaintWindows( CTWindow* pWnd );

	virtual BOOL Create( CLCD* pLCD, CTWindow* pMainWnd );

	virtual BOOL Run();

	// ר���ڷ��滷��
	void RunInEmulator();

	// ��Ϣ���п�״̬�µĴ���
	virtual void Idle();

	// ��ʾ��������
	virtual void ShowStart();

	// ���д���
	virtual void OnIdle();

	// ����������ָʾ��⵽����������Ϣ
	virtual void Beep();

	// ��ȡ�����¼���������Ϣ���У�
	// ֱ�ӽ����̼�����Ϊ����������Ϣ���У�
	// ����ü�ֵ�ǰ������µ�ֵ(ֵС��128)
	// �����IAL���getch����ʵ�֡�iMsg = TMSG_KEYDOWN��WPARAM = ��ֵ
	virtual BOOL GetKeyboardEvent();

	void PostKeyMessage( unsigned char cKeyValue );

	// ����������������������ݣ����ڱȽϺ�ʱ�Ĳ�������ʱ
	BOOL CleanKeyBuffer();

	// ������Ϣ��
	// ���ֳ�TMSG_PAINT��Ϣ��������
	int DespatchMessage( _TMSG* pMsg );

#if defined(RUN_ENVIRONMENT_WIN32)
	// ģ��GetKeyboardEvent������TGUIϵͳ����Ϣ�����в���һ��������Ϣ
	// �ú���������windows�·���,��Win32���滻��DespatchMessage����
	void InsertMessageToTGUI( MSG* pMSG );
#endif // defined(RUN_ENVIRONMENT_WIN32)

// ����ĺ������ó�Ա�����Ӧ����ʵ��

#if defined(CHINESE_SUPPORT)
	// �����뷨����(����ʾ��������ϵ)
	BOOL OpenIME( CTWindow* pWnd );
#endif // defined(CHINESE_SUPPORT)

#if defined(CHINESE_SUPPORT)
	// �ر����뷨����(�ر���ʾ���Ͽ���ϵ)
	BOOL CloseIME( CTWindow* pWnd );
#endif // defined(CHINESE_SUPPORT)

	// ��ʾϵͳæ��Ϣ
	BOOL ShowHint( int iIcon, char* psInfo );

	// �ر�ϵͳ��ʾ
	BOOL CloseHint();

	// ֱ�ӵ�����Ϣ��������
	int SendMessage( _TMSG* pMsg );

	// ����Ϣ���������һ����Ϣ��
	// �����Ϣ����������Ϣ�����ﵽ��MESSAGE_MAX ���������Ŀ�����򷵻�ʧ�ܣ�
	BOOL PostMessage( _TMSG* pMsg );

	// ����Ϣ���������һ���˳���Ϣ��
	BOOL PostQuitMessage();

	// ����Ϣ�����в���ָ�����͵���Ϣ��
	// ���������Ϣ��������ָ�����͵���Ϣ���򷵻�TRUE��
	// �ú�����Ҫ���ڶ�ʱ�������ϡ�CheckTimer�������ȼ����Ϣ��������û����ͬ�Ķ�ʱ����Ϣ�����û�У��ٲ����µĶ�ʱ����Ϣ
	BOOL FindMessage( _TMSG* pMsg );

	// ���ݴ��ڵ����ַ���Ϣ����ϵͳ���ַ��Ĳ�����
	BOOL SetCaret( _CARET* pCaret );

	// ���һ����ʱ����
	// �����ǰ��ʱ���������Ѿ��ﵽTIMER_MAX���������Ŀ���򷵻�FALSE��
	// �������һ��ID�뵱ǰ��ʱ����ͬ�Ķ�ʱ������ֱ���޸ĸö�ʱ�����趨��
	BOOL SetTimer( CTWindow* pWindow, int iTimerID, int interval );

	// ɾ��һ����ʱ����
	// ����TimerIDɾ��
	BOOL KillTimer( int iTimerID );

#if defined(CHINESE_SUPPORT)
	// �����뷨�����Ƿ��ڴ�״̬
	BOOL IsIMEOpen();

	// ������д�������ز���
	BOOL EnableGraffiti( int nSerialPort );
#endif // defined(CHINESE_SUPPORT)

#if defined(RESMGT_SUPPORT)
	// ��ȡ�Ի���ģ����Դ
	struct _DLGTEMPLET* GetDlgTemplet( int nID );

	// ��ȡ�ڰ�ͼ����Դ
	unsigned char* GetImage( int nID );
#endif // defined(RESMGT_SUPPORT)

	// ����TopMost����
	BOOL SetTopMost( CTWindow* pWindow );

	// ����һ������ָ���Ƿ���Ч
	BOOL IsWindowValid( CTWindow* pWindow );

	// ʹ��/��ֹ��ʾʱ��
	// ע����ʾʱ��ʹ��ʱ��������ǰʱ�䡱����ʱ�Ӵ��ڡ�
	// ����ǰʱ�䡱������Ϣ�����ܷ��͸��κ�һ�����ڡ�
	// �����Լ�ʱ����Ҫ��ֹʼ�գ��Ա�֤�Լ촰�ڿ��Խ��յ�����ǰʱ�䡱�İ�����Ϣ
	BOOL ClockKeyEnable( bool bEnable );

#if defined(MOUSE_SUPPORT)
	// ����ʱ�Ӱ�ť��λ��
	BOOL SetClockButton( int x, int y );

	// ��GUIϵͳ���������Ϣ
	BOOL MouseMessage( int nType, int x, int y );

#endif // defined(MOUSE_SUPPORT)

	// ��GUIϵͳ���ݰ�����Ϣ
	BOOL KeyMessage( int nKeyValue );

	unsigned char getch();
};

#endif // !defined(__TAPP_H__)
