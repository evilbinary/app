// TDialog.cpp: implementation of the CTDialog class.
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CTDlgTemplet::CTDlgTemplet()
{
	m_pT = NULL;
}

CTDlgTemplet::~CTDlgTemplet()
{
	if( m_pT != NULL )
		DeleteDlgTemplet();
}

// 从模板文件初始化对话框模板
struct _DLGTEMPLET* CTDlgTemplet::OpenDlgTemplet( const char* filename )
{
	int k = 0;
	char cOneLine[512];
	DWORD dwActualSize = 0;
	DWORD dwFileLength = 0;
	unsigned char* pFile;

	m_pT = new struct _DLGTEMPLET;

	// 创建模板
	m_pT->wStyle		= TWND_STYLE_NORMAL;
	m_pT->x				= 0;
	m_pT->y				= 0;
	m_pT->w				= 0;
	m_pT->h				= 0;
	memset( m_pT->caption, 0x0, 256 );
	m_pT->id			= 0;
	m_pT->controlnr		= 0;
	m_pT->controls		= NULL;
	m_pT->pAccellTable	= NULL;

	// 打开对话框模板文件（在目标系统上需要修改）
#if defined(RUN_ENVIRONMENT_LINUX)
	FILE* fp = fopen( filename, "r" );
	if( fp == NULL )
	{
		DebugPrintf("Dialog Templet file %s can not open !", filename);
		goto failure;
	}
	
	// 得到文件长度
    if( fseek( fp, 0, SEEK_END ) )
	{
		fclose( fp );
		goto failure;
	}
	dwFileLength = ftell( fp );

	// 创建一个与文件等长的数组
	pFile = new unsigned char[dwFileLength+1];
	memset (pFile, 0x0, dwFileLength+1);

	// 将文件内容读入数组
    if( fseek( fp, 0, SEEK_SET ) )
	{
		fclose( fp );
		delete [] pFile;
		goto failure;
	}
	fread( pFile, sizeof(char), dwFileLength, fp );

	fclose( fp );
#endif // defined(RUN_ENVIRONMENT_LINUX)

#if defined(RUN_ENVIRONMENT_WIN32)
	HANDLE hFile;
	hFile = CreateFile(filename,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,NULL);
	if(hFile==INVALID_HANDLE_VALUE)
		goto failure;

	// 将文件打开到一个char数组中
	dwFileLength = GetFileSize (hFile, 0);

	pFile = new unsigned char[dwFileLength+1];
	memset (pFile, 0x0, dwFileLength+1);

	BOOL bReadSt;
	bReadSt = ReadFile( hFile, pFile, dwFileLength, &dwActualSize, NULL );
	CloseHandle( hFile );
	
	if( (!bReadSt) || (dwActualSize != dwFileLength) )
	{
		delete [] pFile;
		goto failure;
	}
#endif // defined(RUN_ENVIRONMENT_WIN32)

	// 逐行解读模板文件
	memset( cOneLine, 0x0, 512 );
	while( GetLine((char *)pFile, cOneLine, k) )
	{
		k ++;
		if ((cOneLine[0] != '#') && (cOneLine[0] != '\0'))	// 去掉注释行和空行
		{
			int i = 0;
			char cSlice[256];
			memset (cSlice, 0x0, 256);
			if (GetSlice (cOneLine, cSlice, i))
			{
				if (strcmp (cSlice, "DIALOG") == 0)
				{
					// 解读对话框设置
					if( !GetDialogInfoFromString(cOneLine) )
					{
						DebugPrintf("Dialog Templet Error ! -- Dialog Configure Error.");
						goto failure;
					}
				}
				else if (strcmp (cSlice, "CONTROL") == 0)
				{
					// 解读控件设置
					if( !GetControlInfoFromString(cOneLine) )
					{
						DebugPrintf("Dialog Templet Error ! -- Control Configure Error.");
						goto failure;
					}
				}
				else if (strcmp (cSlice, "ACCELL") == 0)
				{
					// 将快捷键列表复制到指定位置
					int iLength = strlen (cOneLine);
					if (iLength > 6)
					{
						iLength -= 6;
					}
					else
					{
						DebugPrintf("Dialog Templet Error ! -- Accelerater Table Error.");
						goto failure;
					}

					m_pT->pAccellTable = new char[iLength];
					memset( m_pT->pAccellTable, 0x0, iLength );
					strncpy( m_pT->pAccellTable, (cOneLine+7), iLength-1 );
				}
				else
				{
					DebugPrintf("Dialog Templet Error ! -- Unexpected Line.");
					goto failure;
				}
			}
			else
			{
				DebugPrintf("Dialog Templet Error ! -- File Format Illegal");
				goto failure;
			}
		}
		// 清空暂存行
		memset( cOneLine, 0x0, 512 );
	}

	delete [] pFile;
	return m_pT;

failure:
	DeleteDlgTemplet();
	return NULL;
}

// 删除对话框模板
// 应当依照链表从最后一个开始delete，直到删光所有控件模板
BOOL CTDlgTemplet::DeleteDlgTemplet()
{
	if (m_pT == NULL)
		return TRUE;

	// 删除控件链表
	if ((m_pT->controlnr == 0) || (m_pT->controls == NULL))
		return TRUE;

	int i;
	for (i=0; i<m_pT->controlnr; i++)
	{
		CTRLDATA* pNext = m_pT->controls->next;
		if (m_pT->controls == NULL)
			return FALSE;

		delete m_pT->controls;
		m_pT->controls = pNext;
	}

	// 删除快捷键列表的文本
	if (m_pT->pAccellTable != NULL)
		delete m_pT->pAccellTable;

	// 删除对话框模板
	delete m_pT;
	m_pT = NULL;

	return TRUE;
}

// 获得模板结构体的指针
struct _DLGTEMPLET* CTDlgTemplet::GetTemplet()
{
	return m_pT;
}

// 从字符串中获取对话框设置
// 格式：DIALOG;对话框风格(数字);X(数字);Y(数字);W(数字);H(数字);ID(数字);对话框题头文字(字符串，不超过255);
BOOL CTDlgTemplet::GetDialogInfoFromString(char* pString)
{
	int k = 1;
	char cTemp[256];

	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
		return FALSE;
	m_pT->wStyle = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
		return FALSE;
	m_pT->x = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
		return FALSE;
	m_pT->y = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
		return FALSE;
	m_pT->w = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
		return FALSE;
	m_pT->h = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
		return FALSE;
	m_pT->id = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
		return FALSE;
	strncpy (m_pT->caption, cTemp, 255);

	return TRUE;
}

// 从字符串中获取控件设置
// 格式：CONTROL;控件类型(数字);控件风格(数字);X(数字);Y(数字);W(数字);H(数字);ID(数字);对话框题头文字(字符串，不超过255);
BOOL CTDlgTemplet::GetControlInfoFromString(char* pString)
{
	_CTRLDATA* pCtrl = new _CTRLDATA;
	memset (pCtrl->caption, 0x0, 256);

	int k = 1;
	char cTemp[256];

	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
	{
		delete pCtrl;
		return FALSE;
	}
	pCtrl->wType = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
	{
		delete pCtrl;
		return FALSE;
	}
	pCtrl->wStyle = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
	{
		delete pCtrl;
		return FALSE;
	}
	pCtrl->x = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
	{
		delete pCtrl;
		return FALSE;
	}
	pCtrl->y = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
	{
		delete pCtrl;
		return FALSE;
	}
	pCtrl->w = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
	{
		delete pCtrl;
		return FALSE;
	}
	pCtrl->h = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
	{
		delete pCtrl;
		return FALSE;
	}
	pCtrl->id = atoi (cTemp);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
	{
		delete pCtrl;
		return FALSE;
	}
	strncpy (pCtrl->caption, cTemp, 255);

	k ++;
	memset (cTemp, 0x0, 256);
	if (!GetSlice (pString, cTemp, k))
	{
		delete pCtrl;
		return FALSE;
	}
	pCtrl->iAddData = atoi (cTemp);

	int iCount = m_pT->controlnr;
	if (iCount == 0)
	{
		m_pT->controls = pCtrl;
		m_pT->controlnr ++;
		return TRUE;
	}
	else
	{
		_CTRLDATA* pNext = m_pT->controls;
		int i;
		for (i=0; i<iCount; i++)
		{
			if (pNext == NULL)
			{
				delete pCtrl;
				return FALSE;
			}
			if (i == iCount-1)
			{
				pNext->next = pCtrl;
				m_pT->controlnr ++;
				return TRUE;
			}
			pNext = pNext->next;
		}
	}

	delete pCtrl;
	return FALSE;
}

/* END */
