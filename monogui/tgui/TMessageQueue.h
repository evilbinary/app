// TMessageQueue.h: interface for the CTMessageQueue class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TMESSAGEQUEUE_H__)
#define __TMESSAGEQUEUE_H__

#define MESSAGE_MAX 60

typedef struct _TMSG
{
	class  CTWindow*	pWnd;
	int    message;
	int    wParam;
	int    lParam;
}TMSG;

typedef struct _QMSG
{
	_TMSG			Msg;
	struct _QMSG*	next;
}QMSG;

class CTMessageQueue
{
private:
	int m_iCount;
	_QMSG* m_pMsgQ;

public:
	CTMessageQueue ();
	virtual ~CTMessageQueue ();

	// 从消息队列中获取一条消息，取出的消息将被删除
	// 如果是TMSG_QUIT消息，则返回0，消息队列空了返回-1，其他情况返回1；
	int GetMessage (_TMSG* pMsg);

	// 向消息队列中添加一条消息；
	// 如果消息队列满（消息数量达到了MESSAGE_MAX 所定义的数目），则返回失败；
	BOOL PostMessage (_TMSG* pMsg);

	// 在消息队列中查找指定类型的消息；
	// 如果发现消息队列中有指定类型的消息，则返回TRUE；
	// 该函数主要用在定时器处理上。CheckTimer函数首先检查消息队列中有没有相同的定时器消息，如果没有，再插入新的定时器消息
	BOOL FindMessage (_TMSG* pMsg);

	// 清除消息队列中所有的消息
	BOOL RemoveAll ();
};

#endif // !defined(__TMESSAGEQUEUE_H__)

