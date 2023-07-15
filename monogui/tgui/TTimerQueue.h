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
	CTWindow*              pWnd;           // �趨��ʱ���Ĵ���
	int                     ID;             // ��ʱ�����

	ULONGLONG                lasttime;       // �ϴ�ʱ��

	int                     interval;       // ʱ��������λ�����룩
	struct _QTIMER*	next;                   // �����е���һ����ʱ��
}QTIMER;

class CTTimerQueue
{
private:
	int m_iCount;
	_QTIMER* m_pTimerQ;

public:
	CTTimerQueue();
	virtual ~CTTimerQueue();

	// ���һ����ʱ����
	// �����ǰ��ʱ���������Ѿ��ﵽTIMER_MAX���������Ŀ���򷵻�FALSE��
	// �������һ��ID�뵱ǰ��ʱ����ͬ�Ķ�ʱ������ֱ���޸ĸö�ʱ�����趨��
	BOOL SetTimer( CTWindow* pWindow, int iTimerID, int interval );

	// ɾ��һ����ʱ����
	// ����TimerIDɾ��
	BOOL KillTimer( int iTimerID );

	// ��鶨ʱ�����У�
	// �������ĳ����ʱ����ʱ�ˣ�������FindMessage���������Ϣ��������û��ͬһ����ʱ����������Ϣ�����û�У���ʹ��PostMessage��������Ϣ�����в���MSG_TIMER��Ϣ��
	// ���PostMessage����Ϣ���в�����Ϣʧ�ܣ���ú�������FALSE��
	BOOL CheckTimer( CTApp* pApp );

private:
	BOOL RemoveAll ();
};

#endif // !defined(__TTIMERQUEUE_H__)
