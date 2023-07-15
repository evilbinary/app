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

	// 初始化快捷键列表
	BOOL Create (char* cAccellTable, CTKeyMapMgt* pKeyMap);

	// 搜索快捷键列表
	int Find (int iKeyValue);

private:
	// 删除所有快捷键表项
	BOOL RemoveAll ();
};

#endif // !defined(__TACCELL_H__)
