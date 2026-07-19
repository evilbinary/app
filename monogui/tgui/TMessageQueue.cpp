// TMessageQueue.cpp: implementation of the CTMessageQueue class.
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTMessageQueue::CTMessageQueue ()
{
	m_iCount = 0;
	m_pMsgQ = NULL;
}

CTMessageQueue::~CTMessageQueue ()
{
	// 清除所有消息
	RemoveAll ();
}

// 从消息队列中获取一条消息，取出的消息将被删除
// 如果是TMSG_QUIT消息，则返回0，消息队列空了返回-1，其他情况返回1；
int CTMessageQueue::GetMessage (_TMSG* pMsg)
{
	if (m_iCount == 0)
		return -1;

	if (m_pMsgQ == NULL)
		return -1;

	memcpy(pMsg, &(m_pMsgQ->Msg), sizeof(_TMSG));

	// 删除已经得到的消息
	_QMSG* theDelOne = m_pMsgQ;
	m_pMsgQ = m_pMsgQ->next;
	delete theDelOne;
	m_iCount --;


	if (pMsg->message == TMSG_QUIT)
		return 0;

	return 1;
}

// 向消息队列中添加一条消息；
// 如果消息队列满（消息数量达到了MESSAGE_MAX 所定义的数目），则返回失败；
BOOL CTMessageQueue::PostMessage (_TMSG* pMsg)
{
	if( m_iCount >= MESSAGE_MAX )
		return FALSE;

	if( pMsg->message == TMSG_PAINT )
	{
		if( FindMessage( pMsg ) )
			return FALSE;
	}

	// 添加一条消息
	_QMSG* theNewOne = new _QMSG;
	memcpy (&(theNewOne->Msg), pMsg, sizeof(_TMSG));
	theNewOne->next = NULL;

	if (m_iCount == 0)
	{
		m_pMsgQ = theNewOne;
		m_iCount ++;
		return TRUE;
	}

	// 查找最末一条消息
	_QMSG* theNext = m_pMsgQ;
	int i;
	for (i=0; i<m_iCount-1; i++)
	{
		if (theNext->next == NULL )
			return FALSE;

		theNext = theNext->next;
	}

	if (theNext->next != NULL)
		return FALSE;

	theNext->next = theNewOne;
	m_iCount ++;
	return TRUE;
}

// 在消息队列中查找指定类型的消息；
// 如果发现消息队列中有指定类型的消息，则返回TRUE；
// 该函数主要用在定时器处理上。CheckTimer函数首先检查消息队列中有没有相同的定时器消息，如果没有，再插入新的定时器消息
BOOL CTMessageQueue::FindMessage (_TMSG* pMsg)
{
	_QMSG* theNext = m_pMsgQ;
	int i;
	for (i=0; i<m_iCount-1; i++)
	{
		if (theNext->next == NULL )
			return FALSE;

		if ((theNext->Msg.pWnd    == pMsg->pWnd) &&
			(theNext->Msg.message == pMsg->message) &&
			(theNext->Msg.wParam  == pMsg->wParam) &&
			(theNext->Msg.lParam  == pMsg->lParam))
			return TRUE;

		theNext = theNext->next;
	}
	return FALSE;
}

// 删除队列中的所有消息
BOOL CTMessageQueue::RemoveAll ()
{
	_QMSG* theNext = NULL;
	int i;
	for (i=0; i<m_iCount; i++)
	{
		if (m_pMsgQ == NULL)
		{
			m_iCount = 0;
			return FALSE;
		}

		theNext = m_pMsgQ->next;
		if (m_pMsgQ != NULL )
		{
			delete m_pMsgQ;
		}
		m_pMsgQ = theNext;
	}
	m_iCount = 0;
	return TRUE;
}
/* END */
