// TAccell.cpp: implementation of the CTAccell class.
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTAccell::CTAccell()
{
	m_iCount = 0;
	m_pAccell = NULL;
}

CTAccell::~CTAccell()
{
	// 删除快捷键列表
	RemoveAll ();
}

// 初始化快捷键列表
BOOL CTAccell::Create (char* cAccellTable, CTKeyMapMgt* pKeyMap)
{
	RemoveAll ();
	char cTemp[256];

	_ACCELL* theNext = m_pAccell;

	int k = 0;
	BOOL bFirst = TRUE;

	while (1)
	{
		memset (cTemp, 0x0, 256);
		BOOL b = GetSlice (cAccellTable, cTemp, k);
		k ++;
		if (!b)
		{
			return TRUE;
		}
	
		int iIndex = pKeyMap->Find( cTemp );
		if( iIndex == -1 )
		{
			return FALSE;
		}
	       
		int iFirstPara = pKeyMap->GetValue( iIndex );
		if( iFirstPara == -1 )
		{
			return FALSE;
		}

		memset (cTemp, 0x0, 256);
		b = GetSlice (cAccellTable, cTemp, k);
		k ++;
		if (!b)
		{
			return FALSE;	// 不成组
		}

		int iSecondPara = atoi(cTemp);

		m_iCount ++;
		_ACCELL* theNewOne = new _ACCELL;
		theNewOne->iKeyValue = (int)iFirstPara;

		theNewOne->ID		 = iSecondPara;

		if (bFirst)		// 如果是第一次，新建表项的指针赋给m_pAccell
		{
			m_pAccell = theNewOne;
			theNext = m_pAccell;
			bFirst = FALSE;
		}
		else
		{
			theNext->next = theNewOne;
			theNext = theNewOne;
		}
	}
}

// 搜索快捷键列表
int CTAccell::Find (int iKeyValue)
{
	_ACCELL* theCurrent = m_pAccell;
	
	int i;

	for ( i = 0; i < m_iCount; i++ )
	{
		if( theCurrent->iKeyValue == iKeyValue )
		  {
		    return theCurrent->ID;
		  }
		theCurrent = theCurrent->next;
	}
	return 0x0;
}

BOOL CTAccell::RemoveAll ()
{
	_ACCELL* theNext = NULL;
	int i;
	for (i=0; i<m_iCount; i++)
	{
		if (m_pAccell == NULL)
		{
			m_iCount = 0;
			return FALSE;
		}

		theNext = m_pAccell->next;
		if (m_pAccell != NULL )
		{
			delete m_pAccell;
		}
		m_pAccell = theNext;
	}
	m_iCount = 0;
	return TRUE;
}
/* END */
