// TKeyMapMgt.cpp

#include "tgui.h"

// �����Ա����
CTKeyMapMgt::CTKeyMapMgt()
{
}

CTKeyMapMgt::~CTKeyMapMgt()
{
}

BOOL CTKeyMapMgt::Load( char* psFileName )
{
#if defined(RUN_ENVIRONMENT_LINUX)
  // ���ļ�
  FILE* fp = fopen( psFileName, "r" );
  if( fp == NULL )
    {
      return FALSE;
    }
  
  // �õ��ļ�����
  if( fseek( fp, 0, SEEK_END ) )
    {
      fclose( fp );
      return FALSE;
    }
  
  unsigned long lFileLen = ftell( fp );
  
  // ����һ�����ļ��ȳ�������
  char* pcaFile = new char[lFileLen];
  
  // ���ļ����ݶ�������
  if( fseek( fp, 0, SEEK_SET ) )
    {
      fclose( fp );
      return FALSE;
    }
  
  fread( pcaFile, sizeof(char), lFileLen, fp );
  
  // �ر��ļ�
  fclose( fp );
#endif // defined(RUN_ENVIRONMENT_LINUX)

#if defined(RUN_ENVIRONMENT_WIN32)
	// ���ļ�
	HANDLE hFile;
	hFile = CreateFile( psFileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, NULL );
	if( hFile == INVALID_HANDLE_VALUE )
		return FALSE;

	DWORD dwFileSize = GetFileSize( hFile, 0 );

	// ����һ���������ļ�����
	char* pcaFile = new char[ dwFileSize ];

	// ���ļ����ݶ�������
	DWORD dwActuallSize;
	BOOL bReadSt;
	bReadSt = ReadFile( hFile, pcaFile, dwFileSize, &dwActuallSize, NULL );
	CloseHandle( hFile );

	if( !bReadSt || ( dwActuallSize != dwFileSize) )
	{
		delete [] pcaFile;
		return FALSE;
	}
#endif // defined(RUN_ENVIRONMENT_WIN32)

	// ���ڴ��һ�����ݵĻ�����
	char psTemp[256];
	char psKeyName[32];
	char psKeyValue[8];

	m_nCount = 0;
	// ���н����ļ�
	int k = 0;
	while( GetLine( pcaFile, psTemp, k ) )
	{
		// �����ͷ���ǡ�#define ��������
		if( strncmp( psTemp, "#define", 7 ) != 0 )
		{
			k ++;
			continue;
		}

		memset( psKeyName, 0x0, 32 );
		memset( psKeyValue, 0x0, 8 );

		int t = 7;
		int cur = 0;

		int nMaxLen = strlen( psTemp );

		// �ҳ�������
		while( (psTemp[t] == ' ')  ||
			   (psTemp[t] == '\t') ||
			   (psTemp[t] == '/')  ||
			   (psTemp[t] == '\0') )
		{
			t ++;
			if( t > nMaxLen )
				break;
		}

		if( t > nMaxLen )
		{
			k ++;
			continue;
		}

		while( (psTemp[t] != ' ')  &&
			   (psTemp[t] != '\t') &&
			   (psTemp[t] != '/')  &&
			   (psTemp[t] != '\0') )
		{
			psKeyName[cur] = psTemp[t];
			t ++;
			cur ++;

			if( t > nMaxLen )
				break;
		}

		if( t > nMaxLen )
		{
			k ++;
			continue;
		}

		psKeyName[cur] = '\0';

		// �ҳ���ֵ
		cur = 0;

		while( (psTemp[t] == ' ')  ||
			   (psTemp[t] == '\t') ||
			   (psTemp[t] == '/')  ||
			   (psTemp[t] == '\0') )
		{
			t ++;
			if( t > nMaxLen )
				break;
		}

		if( t > nMaxLen )
		{
			k ++;
			continue;
		}

		while( (psTemp[t] != ' ')  &&
			   (psTemp[t] != '\t') &&
			   (psTemp[t] != '/')  &&
			   (psTemp[t] != '\0') )
		{
			psKeyValue[cur] = psTemp[t];
			t ++;
			cur ++;

			if( t > nMaxLen )
				break;
		}

		if( t > nMaxLen )
		{
			k ++;
			continue;
		}

		psKeyValue[cur] = '\0';

		// ����������ߺ�ָ����Ϊ���������һ��ѭ��
		if( strlen( psKeyName ) == 0 )
		{
			k ++;
			continue;
		}

		if( strlen( psKeyValue ) == 0 )
		{
			k ++;
			continue;
		}

		int nKeyValue = 0;
		// ����ֵת��Ϊ��ֵ
		// ����ʮ����������ʮ������
		if( (strncmp( psKeyValue, "0X", 2 ) == 0) ||
			(strncmp( psKeyValue, "0x", 2 ) == 0) )
		{
			// ʮ��������
			char cc = psKeyValue[2];

			if( cc>='0' && cc<='9' )
			{
				nKeyValue = (cc - '0');
			}
			else if( cc>='a' && cc<='f' )
			{
				nKeyValue = 10 + (cc - 'a');
			}
			else if( cc>='A' && cc<='F' )
			{
				nKeyValue = 10 + (cc - 'A');
			}
			else
			{
				// ���ڷǷ��ַ�
				k ++;
				continue;
			}

			cc =  psKeyValue[3];

			if( cc != '\0' )
			{
				if( cc>='0' && cc<='9' )
				{
					nKeyValue = nKeyValue * 16 + (cc - '0');
				}
				else if( cc>='a' && cc<='f' )
				{
					nKeyValue = nKeyValue * 16 + 10 + (cc - 'a');
				}
				else if( cc>='A' && cc<='F' )
				{
					nKeyValue = nKeyValue * 16 + 10 + (cc - 'A');
				}
				else
				{
					// ���ڷǷ��ַ�
					k ++;
					continue;
				}
			}
		}
		else
		{
			// ʮ������
			nKeyValue = atoi( psKeyValue );
			if( nKeyValue == 0 )
			{
				k ++;
				continue;
			}
		}

		k ++;

		// ����ֵ��ӵ�KeyMap������
		memset( m_pstKeyMap[ m_nCount ].psKeyName, 0x0, 32 );
		strncpy( m_pstKeyMap[ m_nCount ].psKeyName, psKeyName, 32 );
		m_pstKeyMap[ m_nCount ].nKeyValue = nKeyValue;
		m_nCount ++;

		if( m_nCount == KEY_MAX )
			break;
	}

	delete [] pcaFile;

	return TRUE;
}

int  CTKeyMapMgt::Find( char* psKeyName  )
{

	for( int i=0; i<m_nCount; i++ )
	{
		if( strncmp( psKeyName, m_pstKeyMap[ i ].psKeyName, 32 ) == 0 )
		{
		
			return i;
		}
	}

	return -1;
}

int  CTKeyMapMgt::GetCount()
{
	return m_nCount;
}

BOOL CTKeyMapMgt::GetName( int nIndex, char* psKeyName )
{
	if( (nIndex < 0) || (nIndex >= m_nCount) )
		return FALSE;

	int nLen = strlen( m_pstKeyMap[ nIndex ].psKeyName );
	strncpy( psKeyName, m_pstKeyMap[ nIndex ].psKeyName, 32 );
	psKeyName[nLen] = '\0';

	return TRUE;
}

int CTKeyMapMgt::GetValue( int nIndex )
{
	if( (nIndex < 0) || (nIndex >= m_nCount) )
		return -1;

	return m_pstKeyMap[ nIndex ].nKeyValue;
}
/* END */
