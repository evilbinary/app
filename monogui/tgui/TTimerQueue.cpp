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
	// ɾ����ʱ���б�
	RemoveAll ();
}

// ���һ����ʱ����
// �����ǰ��ʱ���������Ѿ��ﵽTSYSTEM_TIMER_MAX���������Ŀ���򷵻�FALSE��
// �������һ��ID�뵱ǰ��ʱ����ͬ�Ķ�ʱ������ֱ���޸ĸö�ʱ�����趨��
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

		if (theCurrent->ID == iTimerID)		// ����һ��ID�뵱ǰ��ʱ����ͬ�Ķ�ʱ��
		{
			theCurrent->pWnd     = pWindow;
			theCurrent->interval = interval;
			theCurrent->lasttime = lNow;
			return TRUE;
		}

		if (i == (m_iCount - 1))	// ���������һ����ʱ������δ���������ж�ʱ����ͬ��
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

		// ��δ���������һ����ʱ��
		theCurrent = theCurrent->next;
	}

	// ������һ����ʱ��
	m_pTimerQ           = new _QTIMER;
	m_pTimerQ->pWnd	    = pWindow;
	m_pTimerQ->ID       = iTimerID;
	m_pTimerQ->interval = interval;
	m_pTimerQ->lasttime = lNow;
	m_pTimerQ->next     = NULL;
	m_iCount            = 1;
	return TRUE;
}

// ɾ��һ����ʱ����
// ����TimerIDɾ��
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

		if (theCur->ID == iTimerID)		// ����һ��ID�뵱ǰ��ʱ����ͬ�Ķ�ʱ��
		{
			theNext = theCur->next;
			delete theCur;
			m_iCount --;

			if (i == 0)		// ��һ��
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

// ��鶨ʱ�����У�
// �������ĳ����ʱ����ʱ�ˣ�������FindMessage���������Ϣ��������û��ͬһ����ʱ����������Ϣ�����û�У���ʹ��PostMessage��������Ϣ�����в���MSG_TIMER��Ϣ��
// ���PostMessage����Ϣ���в�����Ϣʧ�ܣ���ú�������FALSE��
BOOL CTTimerQueue::CheckTimer( CTApp* pApp )
{
	if (m_iCount == 0)	// ��ʱ�����п�
		return TRUE;

	ULONGLONG lNow = sys_clock ();

	_QTIMER* theNext = m_pTimerQ;
	int i;
	for (i=0; i<m_iCount; i++)
	{
		if (theNext == NULL)	// ��ʱ������������
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
				// �������Ϣ�����з�������ȫ��ͬ�Ķ�ʱ����Ϣ
				// �������һ����Ϣ��û�б������꣬���ٲ����µ���Ϣ
				if (!pApp->PostMessage (&message))
					return FALSE;	// ������Ϣʧ��
			}
		}
		theNext = theNext->next;
	}
	return TRUE;	// ���ж�ʱ����û�е���ָ��ʱ��
}

// ɾ�����ж�ʱ��
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
