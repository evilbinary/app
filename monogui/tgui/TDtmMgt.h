// TDtmMgt.h

#if !defined(__TDTMMGT_H__)
#define __TDTMMGT_H__

// 定义链表节点的结构体
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

	// 初始化
	BOOL Init( const char* pathname );

	// 获取指定的对话框模板
	_DLGTEMPLET* GetDlgTemplet( int nDlgID );

	// 删除全部对话框模板
	BOOL RemoveAll();

private:
	// 添加一个新的对话框模板类
	BOOL Add( int id, CTDlgTemplet* pT );
};

#endif // !defined(__TDTMMGT_H__)
