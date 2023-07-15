// TSystemBar.h

#if !defined(__TSYSTEMBAR_H__)
#define __TSYSTEMBAR_H__

#if !defined(BOOL)
#define BOOL int
#endif // !defined(BOOL)

#define SYSTEM_BAR_W      64
#define SYSTEM_BAR_H      16

class CTApp;

class CTSystemBar
{
private:
	int  m_nStatus;       // 显示状态：0:不显示；1:显示充电；2:显示电池；
	int  m_nBattery;      // 电池电量，0 ~ 100；
	BOOL m_bCaps;         // Caps状态；

public:
	CTSystemBar();
	virtual ~CTSystemBar();

public:
	// 显示系统状态条；
	void Show( CLCD* pLCD );

#if defined(MOUSE_SUPPORT)
	// 鼠标点击切换大小写状态处理
	BOOL PtProc( int x, int y );
#endif // defined(MOUSE_SUPPORT)

	// 设置状态：0:不显示；1:显示充电；2:显示电池；
	BOOL SetStatus( int nStatus );

	// 设置电池电量值，0 ~ 100；
	BOOL SetBattery( int nValue );

	// 得到当前电池电量值；
	int GetBattery();

	// 设置大小写状态指示；
	BOOL SetCaps( BOOL bValue );

	// 得到当前的大小写状态；
	BOOL GetCaps();
};

#endif // !defined(__TSYSTEMBAR_H__)
