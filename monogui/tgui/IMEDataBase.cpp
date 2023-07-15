// IMEDataBase.cpp: implementation of the CIMEDataBase class.
//
//////////////////////////////////////////////////////////////////////

#include "tgui.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CIMEDataBase::CIMEDataBase ()
{
	m_iCurIME = 0;
	m_cSyLength = 0;
	memset( m_cSymbol, 0x0, 7 );
	m_pPyLib = NULL;
	m_iPyCount = 0;
	m_pPyTable = NULL;
	m_pExLib = NULL;
	m_iExCount = 0;
	m_pExTable = NULL;
	m_pLxLib = NULL;
	m_iLxCount = 0;
	m_pLxTable = NULL;
	m_iSyCount = 0;
	m_pSy = NULL;
	m_iHzCount = 0;
	m_pHzTable = NULL;
	m_iLxHzCount = 0;
	m_pLxHzTable = NULL;
}

CIMEDataBase::~CIMEDataBase ()
{
	if (m_pPyLib != NULL)
		delete [] m_pPyLib;
	if (m_pPyTable != NULL)
		delete [] m_pPyTable;
	if (m_pExLib != NULL)
		delete [] m_pExLib;
	if (m_pExTable != NULL)
		delete [] m_pExTable;
	if (m_pLxLib != NULL)
		delete [] m_pLxLib;
	if (m_pSy != NULL)
		delete [] m_pSy;
	if (m_pHzTable != NULL)
		delete [] m_pHzTable;
	if (m_pLxHzTable != NULL)
		delete [] m_pLxHzTable;
}

// 初始化输入法库并创建索引表
#if defined(RUN_ENVIRONMENT_WIN32)
BOOL CIMEDataBase::Init ()
{
	if (	// 如果下列指针有非空的，说明已经初始化了或者初始化不成功
		(m_pPyLib	!= NULL) ||
		(m_pPyTable	!= NULL) ||
		(m_pExLib	!= NULL) ||
		(m_pExTable	!= NULL) ||
		(m_pLxLib	!= NULL) ||
		(m_pLxTable	!= NULL)
	   )
	{
		DebugPrintf("IMEDB Can Not Be Init Twice !");
		return FALSE;
	}

	// 主要有6个步骤<1>~<6>
	// <1>打开py.db文件（在目标系统上需要修改）
	HANDLE hFile;
	unsigned long ulActualSize;
	BOOL bReadSt;
	DWORD dwFileLength;

	hFile = CreateFile("py.db",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,NULL);
	if(hFile==INVALID_HANDLE_VALUE)
	{
		DebugPrintf("PinYin IME file py.db can not open !");
		return FALSE;
	}

	// 将py.db文件打开到m_pPyLib数组中
	dwFileLength = GetFileSize( hFile, 0 );
	m_pPyLib = new unsigned char[dwFileLength+1];
	memset (m_pPyLib, 0x0, dwFileLength+1);

	bReadSt = ReadFile( hFile, m_pPyLib, dwFileLength, &ulActualSize, NULL);
	CloseHandle (hFile);
	
	if (!bReadSt)
	{
		DebugPrintf("PinYin IME file py.db read error !");
		return FALSE;
	}
	if (ulActualSize != IME_PY_LIB_LENGTH)
	{
		DebugPrintf("PinYin IME file py.db read size error !");
		return FALSE;
	}

	// <2>打开ex.db文件
	hFile = CreateFile("ex.db",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,NULL);
	if(hFile==INVALID_HANDLE_VALUE)
	{
		DebugPrintf("Extra PinYin IME file ex.db can not open !");
		return FALSE;
	}

	// 将ex.db文件打开到m_pExLib数组中
	dwFileLength = GetFileSize( hFile, 0 );
	m_pExLib = new unsigned char[dwFileLength+1];
	memset (m_pExLib, 0x0, dwFileLength+1);

	bReadSt = ReadFile( hFile, m_pExLib, dwFileLength, &ulActualSize, NULL);
	CloseHandle (hFile);
	
	if (!bReadSt)
	{
		DebugPrintf("Extra PinYin IME file ex.db read error !");
		return FALSE;
	}
	if (ulActualSize != IME_EX_LIB_LENGTH)
	{
		DebugPrintf("Extra PinYin IME file ex.db read size error !");
		return FALSE;
	}

	// <3>打开联想输入法词库lx.db文件
	hFile = CreateFile("lx.db",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,NULL);
	if(hFile==INVALID_HANDLE_VALUE)
	{
		DebugPrintf("Lengend Chinese Vocabulary file lx.db can not open !");
		return FALSE;
	}

	// 将lx.db文件打开到m_pLxLib数组中
	dwFileLength = GetFileSize( hFile, 0 );
	m_pLxLib = new unsigned char[dwFileLength+1];
	memset (m_pLxLib, 0x0, dwFileLength+1);

	bReadSt = ReadFile( hFile, m_pLxLib, dwFileLength, &ulActualSize, NULL);
	CloseHandle (hFile);
	
	if (!bReadSt)
	{
		DebugPrintf("Lengend Chinese Vocabulary file lx.db read error !");
		return FALSE;
	}
	if (ulActualSize != IME_LX_LIB_LENGTH)
	{
		DebugPrintf("Lengend Chinese Vocabulary file lx.db read size error !");
		return FALSE;
	}

	// 创建索引表
	int i = 0;
	char cTemp [1024];	// 每行最多1K长度

	// <4>创建标准拼音库的索引表
	// 计算拼音库的行数，要为每一行建立一个索引
	while (GetLine ((char*)m_pPyLib, cTemp, i))
	{
		i ++;
	}

	if (i != 0)
	{
		m_iPyCount = i + 1;
		m_pPyTable = new _SY_INDEX_ITEM [m_iPyCount];	// 创建索引表所需要的内存空间
		memset (m_pPyTable, 0x0, (sizeof(_SY_INDEX_ITEM)*m_iPyCount));
	}
	else
	{
		DebugPrintf("py.db invalid !");
		return FALSE;
	}

	DWORD dwFirstPos = 0;	// 每一行的开始位置
	for (i=0; i< m_iPyCount; i++)
	{
		// 取出一行
		memset (cTemp, 0x0, 1024);
		if (GetLine ((char*)m_pPyLib, cTemp, i))
		{
			int iLength = strlen (cTemp);
			// 设置索引表
			memcpy (&(m_pPyTable[i].cSymbol), cTemp, 6);	// 拷贝每一行前6个字节的拼音组合
			m_pPyTable[i].dwStart = dwFirstPos + 6;
			m_pPyTable[i].wLength = iLength - 6;
			dwFirstPos += (iLength + 1);				// 加上回车换行符所占的一个字节长度
		}
	}
	// Win32下：将创建好的索引表写入py.idx文件
	DWORD dwWritten;
	hFile = CreateFile("py.idx", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, NULL);
	WriteFile(hFile, m_pPyTable,(m_iPyCount*sizeof(_SY_INDEX_ITEM)), &dwWritten, NULL);
	CloseHandle(hFile);

	// <5>创建方言扩展库的索引表
	// 计算方言扩展库的行数，要为每一行建立一个索引
	while (GetLine ((char*)m_pExLib, cTemp, i))
	{
		i ++;
	}

	if (i != 0)
	{
		m_iExCount = i + 1;
		m_pExTable = new _SY_INDEX_ITEM [m_iExCount];	// 创建索引表所需要的内存空间
		memset (m_pExTable, 0x0, (sizeof(_SY_INDEX_ITEM)*m_iExCount));
	}
	else
	{
		DebugPrintf("ex.db invalid !");
		return FALSE;
	}

	dwFirstPos = 0;		// 每一行的开始位置
	for (i=0; i< m_iExCount; i++)
	{
		// 取出一行
		memset (cTemp, 0x0, 1024);
		if (GetLine ((char*)m_pExLib, cTemp, i))
		{
			int iLength = strlen (cTemp);
			// 设置索引表
			memcpy (&(m_pExTable[i].cSymbol), cTemp, 6);	// 拷贝每一行前6个字节的拼音组合
			m_pExTable[i].dwStart = dwFirstPos + 6;
			m_pExTable[i].wLength = iLength - 6;
			dwFirstPos += (iLength + 1);				// 加上回车换行符所占的一个字节长度
		}
	}
	// Win32下：将创建好的索引表写入ex.idx文件
	dwWritten;
	hFile = CreateFile("ex.idx", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, NULL);
	WriteFile(hFile, m_pExTable,(m_iExCount*sizeof(_SY_INDEX_ITEM)), &dwWritten, NULL);
	CloseHandle(hFile);

	// <6>创建联想词库的索引表
	while (GetLine ((char*)m_pLxLib, cTemp, i))
	{
		i ++;
	}

	if (i != 0)
	{
		m_iLxCount = i + 1;
		m_pLxTable = new _HZ_INDEX_ITEM [m_iLxCount];	// 创建索引表所需要的内存空间
		memset (m_pLxTable, 0x0, (sizeof(_HZ_INDEX_ITEM)*m_iLxCount));
	}
	else
	{
		DebugPrintf("lx.db invalid !");
		return FALSE;
	}

	dwFirstPos = 0;		// 每一行的开始位置
	for (i=0; i< m_iLxCount; i++)
	{
		// 取出一行
		memset (cTemp, 0x0, 1024);
		if (GetLine ((char*)m_pLxLib, cTemp, i))
		{
			int iLength = strlen (cTemp);
			// 设置索引表
			memcpy (&(m_pLxTable[i].cCharacter), cTemp, 2);	// 拷贝每一行前2个字节的拼音组合
			m_pLxTable[i].dwStart = dwFirstPos + 2;
			m_pLxTable[i].wLength = iLength - 2;
			dwFirstPos += (iLength + 1);				// 加上回车换行符所占的一个字节长度
		}
	}
	// Win32下：将创建好的索引表写入lx.idx文件
	hFile = CreateFile("lx.idx", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, NULL);
	WriteFile(hFile, m_pLxTable,(m_iLxCount*sizeof(_HZ_INDEX_ITEM)), &dwWritten, NULL);
	CloseHandle(hFile);

	// 如果全部创建没有问题，则返回TRUE
	return TRUE;
}
#endif // defined(RUN_ENVIRONMENT_WIN32)

#if defined(RUN_ENVIRONMENT_LINUX)
BOOL CIMEDataBase::Init ()
{
	int fd;

	// 打开拼音输入法库
    fd = open ("py.db", O_RDONLY);
    if (fd == -1)
    {
		DebugPrintf("ERR: Can't Open py.db");
		return FALSE;
    }
	m_pPyLib = new unsigned char [IME_PY_LIB_LENGTH + 1];
    memset (m_pPyLib, 0x0, IME_PY_LIB_LENGTH+1);
    int iRlt = read (fd, m_pPyLib, IME_PY_LIB_LENGTH);
    if (iRlt != IME_PY_LIB_LENGTH)
    {
		DebugPrintf("ERR: py.db Read Error.");
		return FALSE;
    }
    close (fd);

	// 打开扩展输入法库
    fd = open ("ex.db", O_RDONLY);
    if (fd == -1)
    {
		DebugPrintf("ERR: Can't Open ex.db");
		return FALSE;
    }
	m_pExLib = new unsigned char [IME_EX_LIB_LENGTH + 1];
    memset (m_pExLib, 0x0, IME_EX_LIB_LENGTH+1);
    iRlt = read (fd, m_pExLib, IME_EX_LIB_LENGTH);
    if (iRlt != IME_EX_LIB_LENGTH)
    {
		DebugPrintf("ERR: ex.db Read Error.");
		return FALSE;
    }
    close (fd);

	// 打开联想词库
    fd = open ("lx.db", O_RDONLY);
    if (fd == -1)
    {
		DebugPrintf("ERR: Can't Open lx.db");
		return FALSE;
    }
	m_pLxLib = new unsigned char [IME_LX_LIB_LENGTH + 1];
    memset (m_pLxLib, 0x0, IME_LX_LIB_LENGTH+1);
    iRlt = read (fd, m_pLxLib, IME_LX_LIB_LENGTH);
    if (iRlt != IME_LX_LIB_LENGTH)
    {
		DebugPrintf("ERR: lx.db Read Error.");
		return FALSE;
    }
    close (fd);

	// 打开拼音输入法索引
	m_iPyCount = IME_PY_INDEX_LENGTH / sizeof(_SY_INDEX_ITEM);
    fd = open ("py.idx", O_RDONLY);
    if (fd == -1)
    {
		DebugPrintf("ERR: Can't Open py.idx");
		return FALSE;
    }
	m_pPyTable = new _SY_INDEX_ITEM[ m_iPyCount ];
    memset (m_pPyTable, 0x0, IME_PY_INDEX_LENGTH);
    iRlt = read (fd, m_pPyTable, IME_PY_INDEX_LENGTH);
    if (iRlt != IME_PY_INDEX_LENGTH)
    {
		DebugPrintf("ERR: py.idx Read Error.");
		return FALSE;
    }
    close (fd);

	// 打开扩展输入法索引
	m_iExCount = IME_EX_INDEX_LENGTH / sizeof(_SY_INDEX_ITEM);
    fd = open ("ex.idx", O_RDONLY);
    if (fd == -1)
    {
		DebugPrintf("ERR: Can't Open ex.idx");
		return FALSE;
    }
	m_pExTable = new _SY_INDEX_ITEM[ m_iExCount ];
    memset (m_pExTable, 0x0, IME_EX_INDEX_LENGTH);
    iRlt = read (fd, m_pExTable, IME_EX_INDEX_LENGTH);
    if (iRlt != IME_EX_INDEX_LENGTH)
    {
		DebugPrintf("ERR: ex.idx Read Error.");
		return FALSE;
    }
    close (fd);

	// 打开联想词库索引
	m_iLxCount = IME_LX_INDEX_LENGTH / sizeof(_HZ_INDEX_ITEM);
    fd = open ("lx.idx", O_RDONLY);
    if (fd == -1)
    {
		DebugPrintf("ERR: Can't Open lx.idx");
		return FALSE;
    }
	m_pLxTable = new _HZ_INDEX_ITEM[ m_iLxCount ];
    memset (m_pLxTable, 0x0, IME_LX_INDEX_LENGTH);
    iRlt = read (fd, m_pLxTable, IME_LX_INDEX_LENGTH);
    if (iRlt != IME_LX_INDEX_LENGTH)
    {
		DebugPrintf("ERR: lx.idx Read Error.");
		return FALSE;
    }
    close (fd);

	return TRUE;
}
#endif // defined(RUN_ENVIRONMENT_LINUX)

// 设定当前输入法：1标准拼音；2方言扩展
int CIMEDataBase::SetCurIME (int iIME)
{
	int iOldIME = m_iCurIME;
	if ((iIME == 1) || (iIME == 2))
	{
		m_iCurIME = iIME;
	}
	return iOldIME;
}

// 向输入表添加一个字符
BOOL CIMEDataBase::SyAdd (char ch)
{
	// 只允许输入数字
	if ((ch < '0') || (ch > '9'))
		return FALSE;

	if ((m_cSyLength >= 0) && (m_cSyLength < 6))
	{
		m_cSymbol [m_cSyLength] = ch;
		m_cSyLength ++;
		return TRUE;
	}
	return FALSE;
}

// 删除输入表的最后一个字符
BOOL CIMEDataBase::SyRemove ()
{
	if (m_cSyLength > 0)
	{
		m_cSyLength --;
		m_cSymbol [m_cSyLength] = '\0';
		return TRUE;
	}
	return FALSE;
}

// 清空输入表
BOOL CIMEDataBase::SyRemoveAll ()
{
	memset (m_cSymbol, 0x0, 6);
	m_cSyLength = 0;
	return TRUE;
}

// 得到输入表的长度
int CIMEDataBase::GetSyLength ()
{
	return (int)m_cSyLength;
}

// 根据拼音输入表查找匹配的拼音
int CIMEDataBase::GetSy (unsigned char** pSy)
{
	// 如果未设定输入法则退出查询
	if (m_iCurIME == 0)
		return 0;
	if (m_cSyLength == 0)
		return 0;
	if ((m_iCurIME == 1) && (m_pPyTable == NULL))
		return 0;
	if ((m_iCurIME == 2) && (m_pExTable == NULL))
		return 0;

	// 清空返回集
	if (m_pSy != NULL)
	{
		delete [] m_pSy;
		m_pSy = NULL;
	}

	_SY_INDEX_ITEM* pTable = NULL;	// 要查询的输入法表
	int iTableLength = 0;			// 输入法表的长度

	if (m_iCurIME == 1)
	{
		pTable = m_pPyTable;
		iTableLength = m_iPyCount;
	}
	else if (m_iCurIME == 2)
	{
		pTable = m_pExTable;
		iTableLength = m_iExCount;
	}

	// 根据第一个字母，生成第一次查询的输出表
	m_pSy = new unsigned char [iTableLength*8];
	memset (m_pSy, 0x0, (iTableLength*8));
	m_iSyCount = 0;
	// 注：这样做空间肯定有很多空余，但是在下次查询的时候会被删掉
	// 之所以不采用一条一条添加的方式，是为了换取更高的速度

	char cKey [4];
	GetKeySymbol (cKey, m_cSymbol[0]);	// 得到数字键所对应的4个英文字母

	int i;
	for (i=0; i<iTableLength; i++)
	{
		char cc = pTable[i].cSymbol[0];	// 取得索引表中相应位置的字符
		if (
			((cc == cKey[0]) ||			// 如果索引表中的字母与按键对应的字母相吻合
			 (cc == cKey[1]) ||
			 (cc == cKey[2]) ||
			 (cc == cKey[3]))
			 && (cc != 0x0)
		   )
		{
			memcpy( (m_pSy+m_iSyCount*8), pTable[i].cSymbol, 6 );
			m_iSyCount ++;
		}
	}

	// 后面的查询均以前一次查询的输出集作为输入集
	for (i=1; i<m_cSyLength; i++)
	{
		unsigned char* pTempTable = new unsigned char [m_iSyCount*8];
		memset (pTempTable, 0x0, (m_iSyCount*8));
		int iTempCount = 0;

		char cKey [4];
		GetKeySymbol (cKey, m_cSymbol[i]);

		int j;
		for (j=0; j<(m_iSyCount); j++)
		{
			char cc = m_pSy[j*8+i];	// 取得索引表中相应位置的字符
			if (
				((cc == cKey[0]) ||	// 如果索引表中的字母与按键对应的字母相吻合
				 (cc == cKey[1]) ||
				 (cc == cKey[2]) ||
				 (cc == cKey[3]))
				 && (cc != 0x0)
			   )
			{
				memcpy ((pTempTable+iTempCount*8), &(m_pSy[j*8]), 6);
				iTempCount ++;
			}
		}

		if (m_pSy != NULL)
		{
			delete [] m_pSy;
			m_pSy = NULL;
		}

		m_pSy = pTempTable;
		m_iSyCount = iTempCount;
	}

	// 设置拼音组合的匹配值('0'不完全匹配；'1'完全匹配)
	char cFirstCh = 0x0;
	for (i=0; i<m_iSyCount; i++)
	{
		if (m_cSyLength == 6)
		{
			// 输入集达到最大长度6个字母，设置为完全匹配
			m_pSy[i*8+6] = '1';
		}
		else if (m_cSyLength == 1)
		{
			// 如果输入集只有1个字母，则将第一个字母不同的设置为完全匹配
			if (m_pSy[i*8] != cFirstCh)
			{
				m_pSy[i*8+6] = '1';
				cFirstCh = m_pSy[i*8];
			}
			else
			{
				m_pSy[i*8+6] = '0';
			}
		}
		else
		{
			if (m_pSy[i*8+m_cSyLength] == 0x20)	// 看后面是否空格键，如果是，说明后面没有其他字母了
				m_pSy[i*8+6] = '1';
			else
				m_pSy[i*8+6] = '0';
		}
	}

	*pSy = m_pSy;
	return m_iSyCount;
}

// 根据拼音查找汉字
int CIMEDataBase::GetHz (unsigned char* pInput, unsigned char** pResult)
{
	// 如果未设定输入法则退出查询
	if (m_iCurIME == 0)
		return 0;
	if ((m_iCurIME == 1) && ((m_pPyLib == NULL) || (m_pPyTable == NULL)))
		return 0;
	if ((m_iCurIME == 2) && ((m_pExLib == NULL) || (m_pExTable == NULL)))
		return 0;

	unsigned char* pLib = NULL;
	_SY_INDEX_ITEM* pTable = NULL;	// 要查询的输入法表
	int iTableLength = 0;			// 输入法表的长度

	if (m_iCurIME == 1)
	{
		pLib = m_pPyLib;
		pTable = m_pPyTable;
		iTableLength = m_iPyCount;
	}
	else if (m_iCurIME == 2)
	{
		pLib = m_pExLib;
		pTable = m_pExTable;
		iTableLength = m_iExCount;
	}
	else
	{
		return 0;
	}

	// 如果输入的拼音串只有一个字母，则使用模糊查询
	int iResult = -1;
	if (pInput[1] == 0x20)
	{
		int i;
		for (i=0; i<iTableLength; i++)
		{
			if (pTable[i].cSymbol[0] == pInput[0])
			{
				iResult = i;	// 找到了
				break;
			}
		}
	}
	else
	{
		// 使用二分搜索法
		iResult = SearchHz (0, iTableLength, pTable, pInput);
	}

	if (m_pHzTable != NULL)
	{
		delete [] m_pHzTable;
		m_pHzTable = NULL;
	}

	if (iResult == -1)
		return 0;	// 表示没有找到

	m_iHzCount = (int)(pTable[iResult].wLength);
	DWORD dwOffset = pTable[iResult].dwStart;

	m_pHzTable = new unsigned char [m_iHzCount+1];
	memset (m_pHzTable, 0x0, m_iHzCount+1);
	memcpy (m_pHzTable, &pLib[dwOffset], m_iHzCount);

	*pResult = m_pHzTable;
	return (m_iHzCount / 2);	// 返回汉字的数目
}

// 根据汉字查找联想字
int CIMEDataBase::GetLx (unsigned char* pInput, unsigned char** pResult)
{
	if (m_pLxTable == NULL)
		return 0;
	if ((m_pLxLib == NULL) || (m_pLxTable == NULL))
		return 0;

	// 使用二分搜索法
	int iResult = SearchLx (0, m_iLxCount, m_pLxTable, pInput);

	if (m_pLxHzTable != NULL)
	{
		delete [] m_pLxHzTable;
		m_pLxHzTable = NULL;
	}

	if (iResult == -1)
		return 0;	// 表示没有找到

	m_iLxHzCount = (int)(m_pLxTable[iResult].wLength);
	DWORD dwOffset = m_pLxTable[iResult].dwStart;

	m_pLxHzTable = new unsigned char [m_iLxHzCount+1];
	memset (m_pLxHzTable, 0x0, m_iLxHzCount+1);
	memcpy (m_pLxHzTable, &m_pLxLib[dwOffset], m_iLxHzCount);

	*pResult = m_pLxHzTable;
	return (m_iLxHzCount / 2);		// 返回汉字的数目
}

// 得到数字键所对应的4个英文字母
void CIMEDataBase::GetKeySymbol (char* pKey, char cNum)
{
	switch (cNum)
	{
	case '0':
		memcpy (pKey, Key0, 4);
		break;
	case '1':
		memcpy (pKey, Key1, 4);
		break;
	case '2':
		memcpy (pKey, Key2, 4);
		break;
	case '3':
		memcpy (pKey, Key3, 4);
		break;
	case '4':
		memcpy (pKey, Key4, 4);
		break;
	case '5':
		memcpy (pKey, Key5, 4);
		break;
	case '6':
		memcpy (pKey, Key6, 4);
		break;
	case '7':
		memcpy (pKey, Key7, 4);
		break;
	case '8':
		memcpy (pKey, Key8, 4);
		break;
	case '9':
		memcpy (pKey, Key9, 4);
		break;
	}
}

// 二分搜索法的搜索子程序
// iStart:开始位置；iEnd:结束位置；pIndex:索引字符串；pString:参加比较的字符串
// 用拼音搜索汉字
int CIMEDataBase::SearchHz (int iStart, int iEnd, _SY_INDEX_ITEM* pIndex, unsigned char* pString)
{
	int iMid = 0;
	int iResult = 0;

	while( iStart <= iEnd )
	{
		iMid = ( iStart + iEnd ) / 2;
		iResult = memcmp( pIndex[iMid].cSymbol, pString, 6 );

		if( iResult == 0 )
			return iMid;    // 找到啦！

		if( iResult > 0 )
			iEnd   = iMid - 1;
		else
			iStart = iMid + 1;
	}

	return( -1 );           // 没找到
}

// 用汉字搜索联想汉字
int CIMEDataBase::SearchLx (int iStart, int iEnd, _HZ_INDEX_ITEM* pIndex, unsigned char* pString)
{
	int iMid = 0;
	int iResult = 0;

	while( iStart <= iEnd )
	{
		iMid = ( iStart + iEnd ) / 2;
		iResult = memcmp( pIndex[iMid].cCharacter, pString, 2 );

		if( iResult == 0 )
			return iMid;    // 找到啦！

		if( iResult > 0 )
			iEnd   = iMid - 1;
		else
			iStart = iMid + 1;
	}

	return( -1 );           // 没找到
}
/* END */
