// TDtmMgt.cpp

#if defined(RUN_ENVIRONMENT_LINUX)
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
// #include <stat.h>
#endif // defined(RUN_ENVIRONMENT_LINUX)

#include "tgui.h"

//////////////////////////////////////////////////////////////////////
CTDtmMgt::CTDtmMgt()
{
	m_nCount = 0;
	m_pHead  = NULL;
}

CTDtmMgt::~CTDtmMgt()
{
	RemoveAll();
}

// ��ʼ��
BOOL CTDtmMgt::Init( const char* pathname )
{
	RemoveAll();

#if defined(RUN_ENVIRONMENT_LINUX)
	BOOL bRet = TRUE;
	DIR* dp;
	struct dirent* dirp;
	struct stat buf;
	int  nID;
	char cFileName[256];
	char cPathName[256];

	if( (dp = opendir(pathname)) == NULL )
	{
		DebugPrintf("CTDtmMgt::Init() opendir failure. PathName is: %s\n",pathname);
		return FALSE;
	}

	while( (dirp = readdir(dp)) != NULL )
	{
		// �����ļ���
		int nNameLen = strlen( dirp->d_name );

		// ����ļ���չ����.dtm
		// ����ļ����ɶԻ���ģ�岢�ҽ���ģ�����ӵ�������ȥ
		if( strncmp( ".dtm", (dirp->d_name+nNameLen-4), 4 ) == 0 )
		{
			memset( cFileName, 0x0, 256 );
			strncpy( cFileName, dirp->d_name, nNameLen-4 );
			nID = atoi( cFileName );
			if( nID == 0 )
				continue;      // �����֣��Ƿ����ļ���

			strncpy( cPathName, pathname, strlen(pathname)+1 );
			strncat( cPathName, "/", 2 );
			strncat( cPathName, dirp->d_name, nNameLen+1 );

			// ������ǳ����ļ��򲻴�
			if( lstat( cPathName, &buf ) < 0 )
			{
				DebugPrintf("CTDtmMgt::Init() lstat() failure: %s\n",cPathName);
				continue;
			}

			if( !S_ISREG(buf.st_mode) )
			{
				DebugPrintf("CTDtmMgt::Init() Not regular file: %s\n",cPathName);
				continue;
			}

			// ��ģ���ļ������뵽����
			CTDlgTemplet* pT = new CTDlgTemplet();
			if( pT->OpenDlgTemplet( cPathName ) )
				Add( nID, pT );
			else
				delete pT;
		}
	}

	closedir(dp);
	DebugPrintf("CTDtmMgt::Init() Opened file count: %d\n",m_nCount);
#endif // defined(RUN_ENVIRONMENT_LINUX)

#if defined(RUN_ENVIRONMENT_WIN32)
	BOOL bRet = TRUE;
	WIN32_FIND_DATA fd;
	int  nID;
	char cFileName[256];
	char cPathName[256];

	strncpy( cPathName, pathname, strlen(pathname)+1 );
	strncat( cPathName, "\\*", 3 );
	HANDLE hFile = FindFirstFile( cPathName, &fd );

	while( (hFile != INVALID_HANDLE_VALUE) && bRet )
	{
		if( (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 )
		{
			int nNameLen = strlen( fd.cFileName );
			
			// ����ļ���չ����.dtm
			// ����ļ����ɶԻ���ģ�岢�ҽ���ģ�����ӵ�������ȥ
			if( strncmp( ".dtm", (fd.cFileName+nNameLen-4), 4 ) == 0 )
			{
				memset( cFileName, 0x0, 256 );
				strncpy( cFileName, fd.cFileName, nNameLen-4 );
				nID = atoi( cFileName );
				if( nID == 0 )
					continue;      // �����֣��Ƿ����ļ���

				strncpy( cPathName, pathname, strlen(pathname)+1 );
				strncat( cPathName, "\\", 2 );
				strncat( cPathName, fd.cFileName, nNameLen+1 );

				// ��ģ���ļ������뵽����
				CTDlgTemplet* pT = new CTDlgTemplet();
				if( pT->OpenDlgTemplet( cPathName ) )
					Add( nID, pT );
				else
					delete pT;
			}
		}

		bRet = FindNextFile( hFile, &fd );
		int iErr = GetLastError();
		if( iErr == ERROR_NO_MORE_FILES )
			DebugPrintf("ERROR_NO_MORE_FILES");
	}

#endif // defined(RUN_ENVIRONMENT_WIN32)

	// һ���ļ�Ҳû�д򿪣�����ʧ��
	if( m_nCount == 0 )
		return FALSE;

	return TRUE;
}

// ��һ���Ի���ģ�����������
BOOL CTDtmMgt::Add( int id, CTDlgTemplet* pT )
{
	struct _QDLGTEMPLET* theNewOne = new _QDLGTEMPLET;
	theNewOne->nID = id;
	theNewOne->pTemplet = pT;
	theNewOne->next = NULL;

	if (m_nCount == 0)
	{
		m_pHead = theNewOne;
		m_nCount ++;
		return TRUE;
	}

	// ������ĩһ��
	struct _QDLGTEMPLET* theNext = m_pHead;
	int i;
	for( i=0; i<m_nCount-1; i++ )
	{
		if( theNext->next == NULL )
			return FALSE;

		theNext = theNext->next;
	}

	if (theNext->next != NULL)
		return FALSE;

	theNext->next = theNewOne;
	m_nCount ++;
	return TRUE;
}

// ��ȡָ���ĶԻ���ģ��
_DLGTEMPLET* CTDtmMgt::GetDlgTemplet( int nDlgID )
{
	if (m_nCount == 0)
		return NULL;

	_QDLGTEMPLET* pT = m_pHead;

	int i;
	for( i=0; i<m_nCount; i++ )
	{
		if( pT == NULL )
			return NULL;

		if( pT->nID == nDlgID )
			return pT->pTemplet->GetTemplet();

		pT = pT->next;
	}

	return NULL;
}

// ɾ��ȫ���Ի���ģ��
BOOL CTDtmMgt::RemoveAll()
{
	_QDLGTEMPLET* theNext;
	int i;

	for( i=0; i<m_nCount; i++ )
	{
		if( m_pHead == NULL )
		{
			m_nCount = 0;
			return FALSE;
		}

		theNext = m_pHead->next;
		delete m_pHead->pTemplet;
		delete m_pHead;
		m_pHead = theNext;
	}

	m_nCount = 0;
	return TRUE;
}

/* END */
