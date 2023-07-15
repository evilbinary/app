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

	// ����Ϣ�����л�ȡһ����Ϣ��ȡ������Ϣ����ɾ��
	// �����TMSG_QUIT��Ϣ���򷵻�0����Ϣ���п��˷���-1�������������1��
	int GetMessage (_TMSG* pMsg);

	// ����Ϣ���������һ����Ϣ��
	// �����Ϣ����������Ϣ�����ﵽ��MESSAGE_MAX ���������Ŀ�����򷵻�ʧ�ܣ�
	BOOL PostMessage (_TMSG* pMsg);

	// ����Ϣ�����в���ָ�����͵���Ϣ��
	// ���������Ϣ��������ָ�����͵���Ϣ���򷵻�TRUE��
	// �ú�����Ҫ���ڶ�ʱ�������ϡ�CheckTimer�������ȼ����Ϣ��������û����ͬ�Ķ�ʱ����Ϣ�����û�У��ٲ����µĶ�ʱ����Ϣ
	BOOL FindMessage (_TMSG* pMsg);

	// �����Ϣ���������е���Ϣ
	BOOL RemoveAll ();
};

#endif // !defined(__TMESSAGEQUEUE_H__)

