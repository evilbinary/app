// TDtmMgt.h

#if !defined(__TDTMMGT_H__)
#define __TDTMMGT_H__

// ��������ڵ�Ľṹ��
typedef struct _QDLGTEMPLET
{
	int                   nID;
	CTDlgTemplet*         pTemplet;
	struct _QDLGTEMPLET*  next;
}QDLGTEMPLET;

class CTDlgTemplet;

class CTDtmMgt
{
private:
	int m_nCount;
	struct _QDLGTEMPLET* m_pHead;

public:
	CTDtmMgt();
	virtual ~CTDtmMgt();

	// ��ʼ��
	BOOL Init( const char* pathname );

	// ��ȡָ���ĶԻ���ģ��
	_DLGTEMPLET* GetDlgTemplet( int nDlgID );

	// ɾ��ȫ���Ի���ģ��
	BOOL RemoveAll();

private:
	// ���һ���µĶԻ���ģ����
	BOOL Add( int id, CTDlgTemplet* pT );
};

#endif // !defined(__TDTMMGT_H__)
