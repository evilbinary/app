// TImgMgt.cpp

#if defined(RUN_ENVIRONMENT_LINUX)
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#endif // defined(RUN_ENVIRONMENT_LINUX)

#include "tgui.h"

//////////////////////////////////////////////////////////////////////
CTImgMgt::CTImgMgt()
{
	m_nCount = 0;
	m_pHead  = NULL;
}

CTImgMgt::~CTImgMgt()
{
	RemoveAll();
}

// ��ʼ��
BOOL CTImgMgt::Init( const char* pathname )
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
		DebugPrintf("CTImgMgt::Init() opendir failure. PathName is: %s\n",pathname);
		return FALSE;
	}

	while( (dirp = readdir(dp)) != NULL )
	{
		// �����ļ���
		int nNameLen = strlen( dirp->d_name );

		// ����ļ���չ����.dtm
		// ����ļ����ɶԻ���ģ�岢�ҽ���ģ�����ӵ�������ȥ
		if( strncmp( ".img", (dirp->d_name+nNameLen-4), 4 ) == 0 )
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
				DebugPrintf("CTImgMgt::Init() lstat() failure: %s\n",cPathName);
				continue;
			}

			if( !S_ISREG(buf.st_mode) )
			{
				DebugPrintf("CTImgMgt::Init() Not regular file: %s\n",cPathName);
				continue;
			}

			// ��img�ļ������뵽����
			unsigned char* pcaImage = OpenImageFile( cPathName );
			if( pcaImage != NULL )
				Add( nID, pcaImage );
			else
				DebugPrintf("CTImgMgt::Init() OpenImageFile failure: %s\n",cPathName);
		}
	}

	closedir(dp);
	DebugPrintf("CTImgMgt::Init() Opened file count: %d\n",m_nCount);
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
			
			// ����ļ���չ����.img
			// ��ת����ʽ�����ӵ�������ȥ
			if( strncmp( ".img", (fd.cFileName+nNameLen-4), 4 ) == 0 )
			{
				memset( cFileName, 0x0, 256 );
				strncpy( cFileName, fd.cFileName, nNameLen-4 );
				nID = atoi( cFileName );
				if( nID == 0 )
					continue;      // �����֣��Ƿ����ļ���

				strncpy( cPathName, pathname, strlen(pathname)+1 );
				strncat( cPathName, "\\", 2 );
				strncat( cPathName, fd.cFileName, nNameLen+1 );

				// ��img�ļ������뵽����
				unsigned char* pcaImage = OpenImageFile( cPathName );
				if( pcaImage != NULL )
					Add( nID, pcaImage );
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

// ��һ���ڰ�ͼ���������
BOOL CTImgMgt::Add( int id, unsigned char* pcaImage )
{
	struct _QIMAGE* theNewOne = new _QIMAGE;
	theNewOne->nID  = id;
	theNewOne->pImg = pcaImage;
	theNewOne->next = NULL;

	if (m_nCount == 0)
	{
		m_pHead = theNewOne;
		m_nCount ++;
		return TRUE;
	}

	// ������ĩһ��
	struct _QIMAGE* theNext = m_pHead;
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

// ��ȡָ���ĺڰ�ͼ��
unsigned char* CTImgMgt::GetImage( int nImgID )
{
	if (m_nCount == 0)
		return NULL;

	_QIMAGE* pT = m_pHead;

	int i;
	for( i=0; i<m_nCount; i++ )
	{
		if( pT == NULL )
			return NULL;

		if( pT->nID == nImgID )
			return pT->pImg;

		pT = pT->next;
	}

	return NULL;
}

// ɾ��ȫ��ͼ��
BOOL CTImgMgt::RemoveAll()
{
	_QIMAGE* theNext;
	int i;

	for( i=0; i<m_nCount; i++ )
	{
		if( m_pHead == NULL )
		{
			m_nCount = 0;
			return FALSE;
		}

		theNext = m_pHead->next;
		delete m_pHead->pImg;
		delete m_pHead;
		m_pHead = theNext;
	}

	m_nCount = 0;
	return TRUE;
}

// ��һ���ڰ׵�Bitmapλͼ���õ����Ļ�����
unsigned char* CTImgMgt::OpenImageFile( const char* filename )
{
	int k = 0;
	DWORD dwActualSize = 0;
	DWORD dwFileLength = 0;
	unsigned char* pFile;

#if defined(RUN_ENVIRONMENT_LINUX)
	FILE* fp = fopen( filename, "r" );
	if( fp == NULL )
	{
		DebugPrintf("Image file %s can not open !", filename);
		return NULL;
	}
	
	// �õ��ļ�����
    if( fseek( fp, 0, SEEK_END ) )
	{
		fclose( fp );
		return NULL;
	}
	dwFileLength = ftell( fp );

	// ����һ�����ļ��ȳ�������
	pFile = new unsigned char[dwFileLength];
	memset(pFile, 0x0, dwFileLength);

	// ���ļ����ݶ�������
    if( fseek( fp, 0, SEEK_SET ) )
	{
		fclose( fp );
		delete [] pFile;
		return NULL;
	}
	fread( pFile, sizeof(char), dwFileLength, fp );
	fclose( fp );
#endif // defined(RUN_ENVIRONMENT_LINUX)

#if defined(RUN_ENVIRONMENT_WIN32)
	HANDLE hFile;
	hFile = CreateFile(filename,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,NULL);
	if(hFile==INVALID_HANDLE_VALUE)
		return NULL;

	// ���ļ��򿪵�һ��char������
	dwFileLength = GetFileSize (hFile, 0);

	pFile = new unsigned char[dwFileLength];
	memset(pFile, 0x0, dwFileLength);

	BOOL bReadSt;
	bReadSt = ReadFile( hFile, pFile, dwFileLength, &dwActualSize, NULL );
	CloseHandle( hFile );
	
	if( (!bReadSt) || (dwActualSize != dwFileLength) )
	{
		delete [] pFile;
		return NULL;
	}
#endif // defined(RUN_ENVIRONMENT_WIN32)

	return pFile;
}

/* END */
