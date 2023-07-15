// TTimerQueue.cpp: implementation of the CTTimerQueue class.
//
//////////////////////////////////////////////////////////////////////
#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTTimerQueue::CTTimerQueue ()
{
	m_iCount = 0;
	m_pTimerQ = NULL;
}

CTTimerQueue::~CTTimerQueue ()
{
	// 删除定时器列表
	RemoveAll ();
}

// 添加一个定时器；
// 如果当前定时器的数量已经达到TSYSTEM_TIMER_MAX所定义的数目，则返回FALSE；
// 如果发现一个ID与当前定时器相同的定时器，则直接修改该定时器的设定；
BOOL CTTimerQueue::SetTimer (CTWindow* pWindow, int iTimerID, int interval)
{
	if (m_iCount >= TSYSTEM_TIMER_MAX)
		return FALSE;

	ULONGLONG lNow = sys_clock ();

	_QTIMER* theCurrent = m_pTimerQ;
	int i;
	for (i=0; i<m_iCount; i++)
	{
		if (theCurrent == NULL)
			return FALSE;

		if (theCurrent->ID == iTimerID)		// 发现一个ID与当前定时器相同的定时器
		{
			theCurrent->pWnd     = pWindow;
			theCurrent->interval = interval;
			theCurrent->lasttime = lNow;
			return TRUE;
		}

		if (i == (m_iCount - 1))	// 检索到最后一个定时器，仍未发现与现有定时器相同的
		{
			_QTIMER* theNewOne      = new _QTIMER;
			theNewOne->pWnd         = pWindow;
			theNewOne->ID           = iTimerID;
			theNewOne->interval     = interval;
			theNewOne->lasttime     = lNow;
			theNewOne->next         = NULL;
			theCurrent->next        = theNewOne;
			m_iCount ++;
			return TRUE;
		}

		// 尚未检索到最后一个定时器
		theCurrent = theCurrent->next;
	}

	// 创建第一个定时器
	m_pTimerQ           = new _QTIMER;
	m_pTimerQ->pWnd	    = pWindow;
	m_pTimerQ->ID       = iTimerID;
	m_pTimerQ->interval = interval;
	m_pTimerQ->lasttime = lNow;
	m_pTimerQ->next     = NULL;
	m_iCount            = 1;
	return TRUE;
}

// 删除一个定时器；
// 根据TimerID删除
BOOL CTTimerQueue::KillTimer (int iTimerID)
{
	_QTIMER* theCur  = m_pTimerQ;
	_QTIMER* thePrev = m_pTimerQ;
	_QTIMER* theNext = NULL;
	int i;
	for (i=0; i<m_iCount; i++)
	{
		if (theCur == NULL)
		{
			return FALSE;
		}

		if (theCur->ID == iTimerID)		// 发现一个ID与当前定时器相同的定时器
		{
			theNext = theCur->next;
			delete theCur;
			m_iCount --;

			if (i == 0)		// 第一个
			{
				m_pTimerQ = theNext;
				return TRUE;
			}
			else
			{
				thePrev->next = theNext;
				return TRUE;
			}
		}

		thePrev = theCur;
		theCur  = theCur->next;
	}

	return FALSE;
}

// 检查定时器队列；
// 如果发现某个定时器到时了，首先用FindMessage函数检查消息队列中有没有同一个定时器发出的消息，如果没有，则使用PostMessage函数向消息队列中插入MSG_TIMER消息；
// 如果PostMessage向消息队列插入消息失败，则该函数返回FALSE；
BOOL CTTimerQueue::CheckTimer( CTApp* pApp )
{
	if (m_iCount == 0)	// 定时器队列空
		return TRUE;

	ULONGLONG lNow = sys_clock ();

	_QTIMER* theNext = m_pTimerQ;
	int i;
	for (i=0; i<m_iCount; i++)
	{
		if (theNext == NULL)	// 定时器队列有问题
			return FALSE;

		if ((lNow - theNext->lasttime) >= theNext->interval)
		{
			theNext->lasttime = lNow;
			_TMSG message;
			message.pWnd    = theNext->pWnd;
			message.message = TMSG_TIMER;
			message.wParam  = theNext->ID;
			message.lParam  = theNext->interval;
			if (!pApp->FindMessage (&message))
			{
				// 如果在消息队列中发现了完全相同的定时器消息
				// 则表明上一个消息还没有被处理完，不再插入新的消息
				if (!pApp->PostMessage (&message))
					return FALSE;	// 发送消息失败
			}
		}
		theNext = theNext->next;
	}
	return TRUE;	// 所有定时器都没有到达指定时间
}

// 删除所有定时器
BOOL CTTimerQueue::RemoveAll ()
{
	_QTIMER* theNext = NULL;
	int i;
	for (i=0; i<m_iCount; i++)
	{
		if (m_pTimerQ == NULL)
		{
			m_iCount = 0;
			return FALSE;
		}

		theNext = m_pTimerQ->next;
		delete m_pTimerQ;
		m_pTimerQ = theNext;
	}
	m_iCount = 0;
	return TRUE;
}
/* END */
