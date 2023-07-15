// TClockWindow.h

#if !defined(__TCLOCK_H__)
#define __TCLOCK_H__

#if !defined(BOOL)
#define BOOL int
#endif // !defined(BOOL)

#define CLOCK_X 39
#define CLOCK_Y 19

class CTApp;

class CTClock
{
private:
	CTApp* m_pApp;

	// ״̬��1: ��ʾ 0:����ʾ
	int  m_nStatus;
	char m_psTime[128];
	char m_psDate[128];
	unsigned char* m_pcaBuffer;

	ULONGLONG m_lLastTime;

#if defined(MOUSE_SUPPORT)
	int  m_iClockButtonX; // ʱ�Ӱ�ť��X
	int  m_iClockButtonY; // ʱ�Ӱ�ť��Y
#endif // defined(MOUSE_SUPPORT)

public:
	CTClock( CTApp* pApp );
	virtual ~CTClock();

public:
	void Enable( BOOL bEnable );

	BOOL IsEnable();

	void Check( CLCD* pLCD, CLCD* pBuf );

#if defined(MOUSE_SUPPORT)
	// �����ʱ�Ӱ�ť�Ĵ���
	BOOL PtProc( int x, int y );

	// ��ʾʱ�Ӱ�ť
	void ShowClockButton( CLCD* pLCD );

	// ����ʱ�Ӱ�ť��λ��
	BOOL SetClockButton( int x, int y );

#endif // defined(MOUSE_SUPPORT)
};

#endif // !defined(__TCLOCK_H__)
