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

	// 状态：1: 显示 0:不显示
	int  m_nStatus;
	char m_psTime[128];
	char m_psDate[128];
	unsigned char* m_pcaBuffer;

	ULONGLONG m_lLastTime;

#if defined(MOUSE_SUPPORT)
	int  m_iClockButtonX; // 时钟按钮的X
	int  m_iClockButtonY; // 时钟按钮的Y
#endif // defined(MOUSE_SUPPORT)

public:
	CTClock( CTApp* pApp );
	virtual ~CTClock();

public:
	void Enable( BOOL bEnable );

	BOOL IsEnable();

	void Check( CLCD* pLCD, CLCD* pBuf );

#if defined(MOUSE_SUPPORT)
	// 鼠标点击时钟按钮的处理
	BOOL PtProc( int x, int y );

	// 显示时钟按钮
	void ShowClockButton( CLCD* pLCD );

	// 设置时钟按钮的位置
	BOOL SetClockButton( int x, int y );

#endif // defined(MOUSE_SUPPORT)
};

#endif // !defined(__TCLOCK_H__)
