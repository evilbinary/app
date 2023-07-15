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
	// ���������Ϣ
	RemoveAll ();
}

// ����Ϣ�����л�ȡһ����Ϣ��ȡ������Ϣ����ɾ��
// �����TMSG_QUIT��Ϣ���򷵻�0����Ϣ���п��˷���-1�������������1��
int CTMessageQueue::GetMessage (_TMSG* pMsg)
{
	if (m_iCount == 0)
		return -1;

	if (m_pMsgQ == NULL)
		return -1;

	memcpy(pMsg, &(m_pMsgQ->Msg), sizeof(_TMSG));

	// ɾ���Ѿ��õ�����Ϣ
	_QMSG* theDelOne = m_pMsgQ;
	m_pMsgQ = m_pMsgQ->next;
	delete theDelOne;
	m_iCount --;


	if (pMsg->message == TMSG_QUIT)
		return 0;

	return 1;
}

// ����Ϣ���������һ����Ϣ��
// �����Ϣ����������Ϣ�����ﵽ��MESSAGE_MAX ���������Ŀ�����򷵻�ʧ�ܣ�
BOOL CTMessageQueue::PostMessage (_TMSG* pMsg)
{
	if( m_iCount >= MESSAGE_MAX )
		return FALSE;

	if( pMsg->message == TMSG_PAINT )
	{
		if( FindMessage( pMsg ) )
			return FALSE;
	}

	// ���һ����Ϣ
	_QMSG* theNewOne = new _QMSG;
	memcpy (&(theNewOne->Msg), pMsg, sizeof(_TMSG));
	theNewOne->next = NULL;

	if (m_iCount == 0)
	{
		m_pMsgQ = theNewOne;
		m_iCount ++;
		return TRUE;
	}

	// ������ĩһ����Ϣ
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

// ����Ϣ�����в���ָ�����͵���Ϣ��
// ���������Ϣ��������ָ�����͵���Ϣ���򷵻�TRUE��
// �ú�����Ҫ���ڶ�ʱ�������ϡ�CheckTimer�������ȼ����Ϣ��������û����ͬ�Ķ�ʱ����Ϣ�����û�У��ٲ����µĶ�ʱ����Ϣ
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

// ɾ�������е�������Ϣ
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
