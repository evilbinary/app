// IMEWindow.h: interface for the CIMEWindow class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__IMEWINDOW_H__)
#define __IMEWINDOW_H__

// ����������뷨������
const unsigned char cIMEName[25] = {"ƴ����չСд��д������д"};
//const unsigned char cIMEName[49] = {"��׼ƴ��������չӢ��СдӢ�Ĵ�д��������д����"};

// ������ַ���
#define IME_PUNCTUATION_NUM 33		// ��33���ַ�
const unsigned char strPunctuation[] =
{
' ', 0x20,  '!', 0x20, '\"', 0x20,  '#', 0x20,  '$', 0x20,  '%', 0x20,  '&', 0x20, '\'', 0x20,  '(', 0x20,
')', 0x20,  '*', 0x20,  '+', 0x20,  ',', 0x20,  '-', 0x20,  '.', 0x20,  '/', 0x20,  ':', 0x20,  ';', 0x20,
'<', 0x20,  '=', 0x20,  '>', 0x20,  '?', 0x20,  '@', 0x20,  '[', 0x20, '\\', 0x20,  ']', 0x20,  '^', 0x20,
'_', 0x20,  '`', 0x20,  '{', 0x20,  '|', 0x20,  '}', 0x20,  '~', 0x20
};

#define IME_TIMER_ID			100	// ���뷨����ʹ�õĶ�ʱ��IDֵ
#define IME_KEY_PRESS_GAP		2000	// Ӣ�����뷨�а���������������
// ע��������ΰ���ͬһ�������������ʱ�䣬���л��ַ��������������µ��ַ�
#define IME_CURRENT_UPPER		0	// ��ǰ����Ϊ����
#define IME_CURRENT_LOWER		1	// ��ǰ����Ϊ����
#define IME_OUT_STRING_LIMIT	32	// Ӣ�����뷨�������ַ�������󳤶�

class CIMEDataBase;

class CIMEWindow : public CTWindow	// (��CTWindow����Ϊ���ܹ�������Ϣ)
{
public:
	int m_iCurIME;                  // ��ǰ���뷨��0:�������뷨��1:��׼ƴ����2:������չ��
                                        // 3:Ӣ��Сд��4:Ӣ�Ĵ�д��5:�����ţ�6:��д�����룻
	int m_iCurInputLine;		// ��ǰ�����У�0:�ϱߵ�ƴ���У�1:�±ߵĺ����С�

	int m_iGraffitiPort;            // ������д����ʹ�õĶ˿�
                                        // 0:δʹ����д�壻1:ʹ�ô���1��2:ʹ�ô���2��

	BOOL m_bGraffitiPortStatus;     // ��д��˿ڵĴ�״̬
	BOOL m_bGraffitiProcRun;        // �����߳��˳��ı���
	BOOL m_bCleanReadCom;           // ������ڻ�������������
	pthread_mutex_t m_mutexReadCom; // ���������߳����еĻ�����
	pthread_t m_threadReadCom;

private:
	CTWindow* m_pTargetWnd;         // ������ֵ�Ŀ�괰��
	CIMEDataBase* m_pIMEDB;         // ���뷨���ݿ�

	int m_iOutStrLength;		// ��Ч����ַ����ĳ���(ֻ����Ӣ�����뷨)

	// ��ѡ�񡢷�ҳ��(ֻ����ƴ�����뷨����չ���뷨�ͷ���)
	BOOL m_bAllSyInvalid;           // ����ƴ������Ч�ı�־(����ʾ��)
	int m_iSyLength;		// ����ƴ������ĸ��(����)
	int m_iSyValidCount;            // ������ʾ��ƴ������Ŀ(����)
	int m_iSyCount;                 // ��ѯ������ƴ������Ŀ
	int m_iCurSy;			// ��ǰѡ�е�ƴ��(���У�0-based)

	int m_iHzCount;			// ���ֵ���Ŀ(���У�ÿ����ʾ9��)
	int m_iCurPage;			// ��ǰҳ��(0-based)

	// �����е���������
	unsigned char m_cUpperLine [33];	// �������֣�32���ַ�
	unsigned char m_cLowerLine [37];	// �������֣�36���ַ�

	// ���뷨���ؼ�
	unsigned char* m_cSyReturn;
	unsigned char* m_cHzReturn;

	// Ӣ������İ����ȴ���ʱ��־
	int m_iTimeOut;		// 0δ��ʱ; 1��ʱ

public:
	CIMEWindow();
	virtual ~CIMEWindow();

	// �������뷨����
	virtual BOOL Create( CTApp* pApp );

	// �������뷨����
	virtual void Paint( CLCD* pLCD );

	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam );

	// �Ƿ���Դ�����ư���(������봰�ǿյģ��򲻴�����ư���������CTEdit����ȥ����)
	BOOL CanHandleControlKey();

	// �����뷨����(����ʾ��������ϵ)
	BOOL OpenIME( CTWindow* pWnd );

	// �ر����뷨����(�ر���ʾ���Ͽ���ϵ)
	BOOL CloseIME( CTWindow* pWnd );

	// �����뷨�����Ƿ��ڴ�״̬
	BOOL IsIMEOpen();

	// ������д�������ز���
	BOOL EnableGraffiti( int nSerialPort );

private:
	// ����ʱ����Ϣ(ֻ����Ӣ�����뷨)
	virtual void OnTimer( int iTimerID, int iInterval );

	// ��Ŀ�괰�ڷ����ַ�(�����Ӣ�ģ���ֻ��c1��c2Ϊ0)
	void SendChar( unsigned char c1, unsigned char c2 );

	// �������ּ�1~9ѡ���ַ�������
	unsigned char* LowerLineGetChar( int iKeyValue, int iLength );

	// ���ֲ�ͬ���뷨�Ĵ�����
	// ƴ�����뷨
	void PinYinProc( CTWindow* pWnd, int iMsg, int wParam, int lParam );

	// Ӣ�����뷨
	void YingWenProc( CTWindow* pWnd, int iMsg, int wParam, int lParam );

	// �������뷨
	void PunctuationProc( CTWindow* pWnd, int iMsg, int wParam, int lParam );

	// ��д���뷨(�ⲿ�豸ͨ���˿����뱸ѡ����)
	void GraffitiProc( CTWindow* pWnd, int iMsg, int wParam, int lParam );

	// ����m_cSyReturn��m_iSyLength��m_iSyCount�����������ַ���m_cUpperLine������m_iSyValidCount
	void RenewUpperLine();

	// �޸�ƴ���Ժ�ĺ�������
	BOOL RenewSy();

	// ����m_cHzReturn��m_iHzCount��m_iCurPage�����������ַ���cLowerLine
	void RenewLowerLine();

	// ���з�ҳ����ǰ��ҳ����
	void LowerLinePageUp();

	// ���з�ҳ�����ҳ����
	void LowerLinePageDown();

	// ��������Ϣת���ɰ����ַ�
	char MessageToKey( int iMessage );

	// ������ĸ���Ҷ�Ӧ�����ּ�
	char CharToKey( char cc );

	// �������ּ����Ҷ�Ӧ����ĸ(���ص�һ��)
	char KeyToChar( char cKey );

	// ���Ҹ���ĸ�������ּ������һ����ĸ
	char KeyNextChar( char cc );
};

#endif // !defined(__IMEWINDOW_H__)
