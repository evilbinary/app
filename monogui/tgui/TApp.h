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
	CLCD*              m_pLCD;             // 显示设备
	CLCD*              m_pLCDBuf;          // 主显示缓冲区
	CTMessageQueue*    m_pMsgQ;            // 消息队列
	CTWindow*          m_pMainWnd;         // 主窗口
	CTCaret*           m_pCaret;           // 脱字符
	CTTimerQueue*      m_pTimerQ;          // 定时器队列
	CTWindow*          m_pTopMostWnd;      // TopMost窗口
	CTKeyMapMgt*       m_pKeyMap;          // 按键宏与按键值对照表
	CTClock*           m_pClock;           // 钟表窗口
	CTSystemBar*       m_pSysBar;          // 系统状态条

#if defined(CHINESE_SUPPORT)
	CIMEWindow*        m_pIMEWnd;          // 输入法窗口
#endif // defined(CHINESE_SUPPORT)

#if defined(RESMGT_SUPPORT)
	CTDtmMgt*          m_pDtmMgt;          // dtm文件管理类
	CTImgMgt*          m_pImgMgt;          // 黑白位图管理类
#endif // defined(RESMGT_SUPPORT)

	BOOL m_bClockKeyEnable;           // 是否使能时钟窗口

public:
	CTApp();
	virtual ~CTApp();

	// 对指定的窗口进行绘制
	virtual void PaintWindows( CTWindow* pWnd );

	virtual BOOL Create( CLCD* pLCD, CTWindow* pMainWnd );

	virtual BOOL Run();

	// 专用于仿真环境
	void RunInEmulator();

	// 消息队列空状态下的处理
	virtual void Idle();

	// 显示开机画面
	virtual void ShowStart();

	// 空闲处理
	virtual void OnIdle();

	// 蜂鸣器鸣叫指示检测到按键按键消息
	virtual void Beep();

	// 获取键盘事件，插入消息队列；
	// 直接将键盘键码作为参数插入消息队列；
	// 如果该键值是按键按下的值(值小于128)
	// 则调用IAL层的getch函数实现。iMsg = TMSG_KEYDOWN；WPARAM = 键值
	virtual BOOL GetKeyboardEvent();

	void PostKeyMessage( unsigned char cKeyValue );

	// 清除按键缓冲区残留的内容，用于比较耗时的操作结束时
	BOOL CleanKeyBuffer();

	// 发送消息；
	// 区分出TMSG_PAINT消息单独处理；
	int DespatchMessage( _TMSG* pMsg );

#if defined(RUN_ENVIRONMENT_WIN32)
	// 模仿GetKeyboardEvent函数向TGUI系统的消息队列中插入一个键盘消息
	// 该函数仅用于windows下仿真,在Win32下替换掉DespatchMessage函数
	void InsertMessageToTGUI( MSG* pMSG );
#endif // defined(RUN_ENVIRONMENT_WIN32)

// 下面的函数调用成员类的相应函数实现

#if defined(CHINESE_SUPPORT)
	// 打开输入法窗口(打开显示，创建联系)
	BOOL OpenIME( CTWindow* pWnd );
#endif // defined(CHINESE_SUPPORT)

#if defined(CHINESE_SUPPORT)
	// 关闭输入法窗口(关闭显示，断开联系)
	BOOL CloseIME( CTWindow* pWnd );
#endif // defined(CHINESE_SUPPORT)

	// 显示系统忙信息
	BOOL ShowHint( int iIcon, char* psInfo );

	// 关闭系统提示
	BOOL CloseHint();

	// 直接调用消息处理函数；
	int SendMessage( _TMSG* pMsg );

	// 向消息队列中添加一条消息；
	// 如果消息队列满（消息数量达到了MESSAGE_MAX 所定义的数目），则返回失败；
	BOOL PostMessage( _TMSG* pMsg );

	// 向消息队列中添加一条退出消息；
	BOOL PostQuitMessage();

	// 在消息队列中查找指定类型的消息；
	// 如果发现消息队列中有指定类型的消息，则返回TRUE；
	// 该函数主要用在定时器处理上。CheckTimer函数首先检查消息队列中有没有相同的定时器消息，如果没有，再插入新的定时器消息
	BOOL FindMessage( _TMSG* pMsg );

	// 根据窗口的脱字符信息设置系统脱字符的参数；
	BOOL SetCaret( _CARET* pCaret );

	// 添加一个定时器；
	// 如果当前定时器的数量已经达到TIMER_MAX所定义的数目，则返回FALSE；
	// 如果发现一个ID与当前定时器相同的定时器，则直接修改该定时器的设定；
	BOOL SetTimer( CTWindow* pWindow, int iTimerID, int interval );

	// 删除一个定时器；
	// 根据TimerID删除
	BOOL KillTimer( int iTimerID );

#if defined(CHINESE_SUPPORT)
	// 看输入法窗口是否处于打开状态
	BOOL IsIMEOpen();

	// 设置手写输入的相关参数
	BOOL EnableGraffiti( int nSerialPort );
#endif // defined(CHINESE_SUPPORT)

#if defined(RESMGT_SUPPORT)
	// 获取对话框模板资源
	struct _DLGTEMPLET* GetDlgTemplet( int nID );

	// 获取黑白图像资源
	unsigned char* GetImage( int nID );
#endif // defined(RESMGT_SUPPORT)

	// 设置TopMost窗口
	BOOL SetTopMost( CTWindow* pWindow );

	// 检验一个窗口指针是否有效
	BOOL IsWindowValid( CTWindow* pWindow );

	// 使能/禁止显示时钟
	// 注：显示时钟使能时，按“当前时间”键打开时钟窗口。
	// “当前时间”按键消息将不能发送给任何一个窗口。
	// 机器自检时，需要禁止始终，以保证自检窗口可以接收到“当前时间”的按键消息
	BOOL ClockKeyEnable( bool bEnable );

#if defined(MOUSE_SUPPORT)
	// 设置时钟按钮的位置
	BOOL SetClockButton( int x, int y );

	// 向GUI系统传递鼠标消息
	BOOL MouseMessage( int nType, int x, int y );

#endif // defined(MOUSE_SUPPORT)

	// 向GUI系统传递按键消息
	BOOL KeyMessage( int nKeyValue );

	unsigned char getch();
};

#endif // !defined(__TAPP_H__)
