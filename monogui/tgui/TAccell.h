// TAccell.h: interface for the CTAccell class.
//
//////////////////////////////////////////////////////////////////////
#if defined(RUN_ENVIRONMENT_LINUX)
#include "TSystemCommon.h"
#endif // defined(RUN_ENVIRONMENT_LINUX)

#if !defined(__TACCELL_H__)
#define __TACCELL_H__

typedef struct _ACCELL
{
	int		iKeyValue;
	int		ID;
	struct _ACCELL*	next;
}ACCELL;

class CTKeyMapMgt;

class CTAccell
{
private:
	int m_iCount;
	_ACCELL* m_pAccell;

public:
	CTAccell();
	virtual ~CTAccell();

	// ��ʼ����ݼ��б�
	BOOL Create (char* cAccellTable, CTKeyMapMgt* pKeyMap);

	// ������ݼ��б�
	int Find (int iKeyValue);

private:
	// ɾ�����п�ݼ�����
	BOOL RemoveAll ();
};

#endif // !defined(__TACCELL_H__)
