// TKeyMapMgt.cpp

#include "tgui.h"

// 定义成员函数
CTKeyMapMgt::CTKeyMapMgt()
{
}

CTKeyMapMgt::~CTKeyMapMgt()
{
}

BOOL CTKeyMapMgt::Load( char* psFileName )
{
#if defined(RUN_ENVIRONMENT_LINUX)
  // 打开文件
  FILE* fp = fopen( psFileName, "r" );
  if( fp == NULL )
    {
      return FALSE;
    }
  
  // 得到文件长度
  if( fseek( fp, 0, SEEK_END ) )
    {
      fclose( fp );
      return FALSE;
    }
  
  unsigned long lFileLen = ftell( fp );
  
  // 创建一个与文件等长的数组
  char* pcaFile = new char[lFileLen];
  
  // 将文件内容读入数组
  if( fseek( fp, 0, SEEK_SET ) )
    {
      fclose( fp );
      return FALSE;
    }
  
  fread( pcaFile, sizeof(char), lFileLen, fp );
  
  // 关闭文件
  fclose( fp );
#endif // defined(RUN_ENVIRONMENT_LINUX)

#if defined(RUN_ENVIRONMENT_WIN32)
	// 打开文件
	HANDLE hFile;
	hFile = CreateFile( psFileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, NULL );
	if( hFile == INVALID_HANDLE_VALUE )
		return FALSE;

	DWORD dwFileSize = GetFileSize( hFile, 0 );

	// 创建一个数组存放文件内容
	char* pcaFile = new char[ dwFileSize ];

	// 将文件内容读入数组
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

	// 用于存放一行内容的缓冲区
	char psTemp[256];
	char psKeyName[32];
	char psKeyValue[8];

	m_nCount = 0;
	// 逐行解析文件
	int k = 0;
	while( GetLine( pcaFile, psTemp, k ) )
	{
		// 如果开头不是“#define ”则跳过
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

		// 找出宏名称
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

		// 找出宏值
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

		// 如果宏名或者宏指长度为零则进入下一个循环
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
		// 将宏值转换为数值
		// 区分十六进制数和十进制数
		if( (strncmp( psKeyValue, "0X", 2 ) == 0) ||
			(strncmp( psKeyValue, "0x", 2 ) == 0) )
		{
			// 十六进制数
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
				// 存在非法字符
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
					// 存在非法字符
					k ++;
					continue;
				}
			}
		}
		else
		{
			// 十进制数
			nKeyValue = atoi( psKeyValue );
			if( nKeyValue == 0 )
			{
				k ++;
				continue;
			}
		}

		k ++;

		// 将数值添加到KeyMap数组中
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
