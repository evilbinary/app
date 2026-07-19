// TTimerQueue.h: interface for the CTTimerQueue class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TTIMERQUEUE_H__)
#define __TTIMERQUEUE_H__

class CTWindow;
class CTApp;

#define TSYSTEM_TIMER_MAX 6

typedef struct _QTIMER
{
	CTWindow*              pWnd;           // 设定定时器的窗口
	int                     ID;             // 定时器序号

	ULONGLONG                lasttime;       // 上次时间

	int                     interval;       // 时间间隔（单位：毫秒）
	struct _QTIMER*	next;                   // 链表中的下一个定时器
}QTIMER;

class CTTimerQueue
{
private:
	int m_iCount;
	_QTIMER* m_pTimerQ;

public:
	CTTimerQueue();
	virtual ~CTTimerQueue();

	// 添加一个定时器；
	// 如果当前定时器的数量已经达到TIMER_MAX所定义的数目，则返回FALSE；
	// 如果发现一个ID与当前定时器相同的定时器，则直接修改该定时器的设定；
	BOOL SetTimer( CTWindow* pWindow, int iTimerID, int interval );

	// 删除一个定时器；
	// 根据TimerID删除
	BOOL KillTimer( int iTimerID );

	// 检查定时器队列；
	// 如果发现某个定时器到时了，首先用FindMessage函数检查消息队列中有没有同一个定时器发出的消息，如果没有，则使用PostMessage函数向消息队列中插入MSG_TIMER消息；
	// 如果PostMessage向消息队列插入消息失败，则该函数返回FALSE；
	BOOL CheckTimer( CTApp* pApp );

private:
	BOOL RemoveAll ();
};

#endif // !defined(__TTIMERQUEUE_H__)
