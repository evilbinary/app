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

// ��ʼ�����뷨�Ⲣ����������
#if defined(RUN_ENVIRONMENT_WIN32)
BOOL CIMEDataBase::Init ()
{
	if (	// �������ָ���зǿյģ�˵���Ѿ���ʼ���˻��߳�ʼ�����ɹ�
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

	// ��Ҫ��6������<1>~<6>
	// <1>��py.db�ļ�����Ŀ��ϵͳ����Ҫ�޸ģ�
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

	// ��py.db�ļ��򿪵�m_pPyLib������
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

	// <2>��ex.db�ļ�
	hFile = CreateFile("ex.db",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,NULL);
	if(hFile==INVALID_HANDLE_VALUE)
	{
		DebugPrintf("Extra PinYin IME file ex.db can not open !");
		return FALSE;
	}

	// ��ex.db�ļ��򿪵�m_pExLib������
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

	// <3>���������뷨�ʿ�lx.db�ļ�
	hFile = CreateFile("lx.db",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,NULL);
	if(hFile==INVALID_HANDLE_VALUE)
	{
		DebugPrintf("Lengend Chinese Vocabulary file lx.db can not open !");
		return FALSE;
	}

	// ��lx.db�ļ��򿪵�m_pLxLib������
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

	// ����������
	int i = 0;
	char cTemp [1024];	// ÿ�����1K����

	// <4>������׼ƴ�����������
	// ����ƴ�����������ҪΪÿһ�н���һ������
	while (GetLine ((char*)m_pPyLib, cTemp, i))
	{
		i ++;
	}

	if (i != 0)
	{
		m_iPyCount = i + 1;
		m_pPyTable = new _SY_INDEX_ITEM [m_iPyCount];	// ��������������Ҫ���ڴ�ռ�
		memset (m_pPyTable, 0x0, (sizeof(_SY_INDEX_ITEM)*m_iPyCount));
	}
	else
	{
		DebugPrintf("py.db invalid !");
		return FALSE;
	}

	DWORD dwFirstPos = 0;	// ÿһ�еĿ�ʼλ��
	for (i=0; i< m_iPyCount; i++)
	{
		// ȡ��һ��
		memset (cTemp, 0x0, 1024);
		if (GetLine ((char*)m_pPyLib, cTemp, i))
		{
			int iLength = strlen (cTemp);
			// ����������
			memcpy (&(m_pPyTable[i].cSymbol), cTemp, 6);	// ����ÿһ��ǰ6���ֽڵ�ƴ�����
			m_pPyTable[i].dwStart = dwFirstPos + 6;
			m_pPyTable[i].wLength = iLength - 6;
			dwFirstPos += (iLength + 1);				// ���ϻس����з���ռ��һ���ֽڳ���
		}
	}
	// Win32�£��������õ�������д��py.idx�ļ�
	DWORD dwWritten;
	hFile = CreateFile("py.idx", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, NULL);
	WriteFile(hFile, m_pPyTable,(m_iPyCount*sizeof(_SY_INDEX_ITEM)), &dwWritten, NULL);
	CloseHandle(hFile);

	// <5>����������չ���������
	// ���㷽����չ���������ҪΪÿһ�н���һ������
	while (GetLine ((char*)m_pExLib, cTemp, i))
	{
		i ++;
	}

	if (i != 0)
	{
		m_iExCount = i + 1;
		m_pExTable = new _SY_INDEX_ITEM [m_iExCount];	// ��������������Ҫ���ڴ�ռ�
		memset (m_pExTable, 0x0, (sizeof(_SY_INDEX_ITEM)*m_iExCount));
	}
	else
	{
		DebugPrintf("ex.db invalid !");
		return FALSE;
	}

	dwFirstPos = 0;		// ÿһ�еĿ�ʼλ��
	for (i=0; i< m_iExCount; i++)
	{
		// ȡ��һ��
		memset (cTemp, 0x0, 1024);
		if (GetLine ((char*)m_pExLib, cTemp, i))
		{
			int iLength = strlen (cTemp);
			// ����������
			memcpy (&(m_pExTable[i].cSymbol), cTemp, 6);	// ����ÿһ��ǰ6���ֽڵ�ƴ�����
			m_pExTable[i].dwStart = dwFirstPos + 6;
			m_pExTable[i].wLength = iLength - 6;
			dwFirstPos += (iLength + 1);				// ���ϻس����з���ռ��һ���ֽڳ���
		}
	}
	// Win32�£��������õ�������д��ex.idx�ļ�
	dwWritten;
	hFile = CreateFile("ex.idx", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, NULL);
	WriteFile(hFile, m_pExTable,(m_iExCount*sizeof(_SY_INDEX_ITEM)), &dwWritten, NULL);
	CloseHandle(hFile);

	// <6>��������ʿ��������
	while (GetLine ((char*)m_pLxLib, cTemp, i))
	{
		i ++;
	}

	if (i != 0)
	{
		m_iLxCount = i + 1;
		m_pLxTable = new _HZ_INDEX_ITEM [m_iLxCount];	// ��������������Ҫ���ڴ�ռ�
		memset (m_pLxTable, 0x0, (sizeof(_HZ_INDEX_ITEM)*m_iLxCount));
	}
	else
	{
		DebugPrintf("lx.db invalid !");
		return FALSE;
	}

	dwFirstPos = 0;		// ÿһ�еĿ�ʼλ��
	for (i=0; i< m_iLxCount; i++)
	{
		// ȡ��һ��
		memset (cTemp, 0x0, 1024);
		if (GetLine ((char*)m_pLxLib, cTemp, i))
		{
			int iLength = strlen (cTemp);
			// ����������
			memcpy (&(m_pLxTable[i].cCharacter), cTemp, 2);	// ����ÿһ��ǰ2���ֽڵ�ƴ�����
			m_pLxTable[i].dwStart = dwFirstPos + 2;
			m_pLxTable[i].wLength = iLength - 2;
			dwFirstPos += (iLength + 1);				// ���ϻس����з���ռ��һ���ֽڳ���
		}
	}
	// Win32�£��������õ�������д��lx.idx�ļ�
	hFile = CreateFile("lx.idx", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, NULL);
	WriteFile(hFile, m_pLxTable,(m_iLxCount*sizeof(_HZ_INDEX_ITEM)), &dwWritten, NULL);
	CloseHandle(hFile);

	// ���ȫ������û�����⣬�򷵻�TRUE
	return TRUE;
}
#endif // defined(RUN_ENVIRONMENT_WIN32)

#if defined(RUN_ENVIRONMENT_LINUX)
BOOL CIMEDataBase::Init ()
{
	int fd;

	// ��ƴ�����뷨��
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

	// ����չ���뷨��
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

	// ������ʿ�
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

	// ��ƴ�����뷨����
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

	// ����չ���뷨����
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

	// ������ʿ�����
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

// �趨��ǰ���뷨��1��׼ƴ����2������չ
int CIMEDataBase::SetCurIME (int iIME)
{
	int iOldIME = m_iCurIME;
	if ((iIME == 1) || (iIME == 2))
	{
		m_iCurIME = iIME;
	}
	return iOldIME;
}

// ����������һ���ַ�
BOOL CIMEDataBase::SyAdd (char ch)
{
	// ֻ������������
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

// ɾ�����������һ���ַ�
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

// ��������
BOOL CIMEDataBase::SyRemoveAll ()
{
	memset (m_cSymbol, 0x0, 6);
	m_cSyLength = 0;
	return TRUE;
}

// �õ������ĳ���
int CIMEDataBase::GetSyLength ()
{
	return (int)m_cSyLength;
}

// ����ƴ����������ƥ���ƴ��
int CIMEDataBase::GetSy (unsigned char** pSy)
{
	// ���δ�趨���뷨���˳���ѯ
	if (m_iCurIME == 0)
		return 0;
	if (m_cSyLength == 0)
		return 0;
	if ((m_iCurIME == 1) && (m_pPyTable == NULL))
		return 0;
	if ((m_iCurIME == 2) && (m_pExTable == NULL))
		return 0;

	// ��շ��ؼ�
	if (m_pSy != NULL)
	{
		delete [] m_pSy;
		m_pSy = NULL;
	}

	_SY_INDEX_ITEM* pTable = NULL;	// Ҫ��ѯ�����뷨��
	int iTableLength = 0;			// ���뷨��ĳ���

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

	// ���ݵ�һ����ĸ�����ɵ�һ�β�ѯ�������
	m_pSy = new unsigned char [iTableLength*8];
	memset (m_pSy, 0x0, (iTableLength*8));
	m_iSyCount = 0;
	// ע���������ռ�϶��кܶ���࣬�������´β�ѯ��ʱ��ᱻɾ��
	// ֮���Բ�����һ��һ����ӵķ�ʽ����Ϊ�˻�ȡ���ߵ��ٶ�

	char cKey [4];
	GetKeySymbol (cKey, m_cSymbol[0]);	// �õ����ּ�����Ӧ��4��Ӣ����ĸ

	int i;
	for (i=0; i<iTableLength; i++)
	{
		char cc = pTable[i].cSymbol[0];	// ȡ������������Ӧλ�õ��ַ�
		if (
			((cc == cKey[0]) ||			// ����������е���ĸ�밴����Ӧ����ĸ���Ǻ�
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

	// ����Ĳ�ѯ����ǰһ�β�ѯ���������Ϊ���뼯
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
			char cc = m_pSy[j*8+i];	// ȡ������������Ӧλ�õ��ַ�
			if (
				((cc == cKey[0]) ||	// ����������е���ĸ�밴����Ӧ����ĸ���Ǻ�
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

	// ����ƴ����ϵ�ƥ��ֵ('0'����ȫƥ�䣻'1'��ȫƥ��)
	char cFirstCh = 0x0;
	for (i=0; i<m_iSyCount; i++)
	{
		if (m_cSyLength == 6)
		{
			// ���뼯�ﵽ��󳤶�6����ĸ������Ϊ��ȫƥ��
			m_pSy[i*8+6] = '1';
		}
		else if (m_cSyLength == 1)
		{
			// ������뼯ֻ��1����ĸ���򽫵�һ����ĸ��ͬ������Ϊ��ȫƥ��
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
			if (m_pSy[i*8+m_cSyLength] == 0x20)	// �������Ƿ�ո��������ǣ�˵������û��������ĸ��
				m_pSy[i*8+6] = '1';
			else
				m_pSy[i*8+6] = '0';
		}
	}

	*pSy = m_pSy;
	return m_iSyCount;
}

// ����ƴ�����Һ���
int CIMEDataBase::GetHz (unsigned char* pInput, unsigned char** pResult)
{
	// ���δ�趨���뷨���˳���ѯ
	if (m_iCurIME == 0)
		return 0;
	if ((m_iCurIME == 1) && ((m_pPyLib == NULL) || (m_pPyTable == NULL)))
		return 0;
	if ((m_iCurIME == 2) && ((m_pExLib == NULL) || (m_pExTable == NULL)))
		return 0;

	unsigned char* pLib = NULL;
	_SY_INDEX_ITEM* pTable = NULL;	// Ҫ��ѯ�����뷨��
	int iTableLength = 0;			// ���뷨��ĳ���

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

	// ��������ƴ����ֻ��һ����ĸ����ʹ��ģ����ѯ
	int iResult = -1;
	if (pInput[1] == 0x20)
	{
		int i;
		for (i=0; i<iTableLength; i++)
		{
			if (pTable[i].cSymbol[0] == pInput[0])
			{
				iResult = i;	// �ҵ���
				break;
			}
		}
	}
	else
	{
		// ʹ�ö���������
		iResult = SearchHz (0, iTableLength, pTable, pInput);
	}

	if (m_pHzTable != NULL)
	{
		delete [] m_pHzTable;
		m_pHzTable = NULL;
	}

	if (iResult == -1)
		return 0;	// ��ʾû���ҵ�

	m_iHzCount = (int)(pTable[iResult].wLength);
	DWORD dwOffset = pTable[iResult].dwStart;

	m_pHzTable = new unsigned char [m_iHzCount+1];
	memset (m_pHzTable, 0x0, m_iHzCount+1);
	memcpy (m_pHzTable, &pLib[dwOffset], m_iHzCount);

	*pResult = m_pHzTable;
	return (m_iHzCount / 2);	// ���غ��ֵ���Ŀ
}

// ���ݺ��ֲ���������
int CIMEDataBase::GetLx (unsigned char* pInput, unsigned char** pResult)
{
	if (m_pLxTable == NULL)
		return 0;
	if ((m_pLxLib == NULL) || (m_pLxTable == NULL))
		return 0;

	// ʹ�ö���������
	int iResult = SearchLx (0, m_iLxCount, m_pLxTable, pInput);

	if (m_pLxHzTable != NULL)
	{
		delete [] m_pLxHzTable;
		m_pLxHzTable = NULL;
	}

	if (iResult == -1)
		return 0;	// ��ʾû���ҵ�

	m_iLxHzCount = (int)(m_pLxTable[iResult].wLength);
	DWORD dwOffset = m_pLxTable[iResult].dwStart;

	m_pLxHzTable = new unsigned char [m_iLxHzCount+1];
	memset (m_pLxHzTable, 0x0, m_iLxHzCount+1);
	memcpy (m_pLxHzTable, &m_pLxLib[dwOffset], m_iLxHzCount);

	*pResult = m_pLxHzTable;
	return (m_iLxHzCount / 2);		// ���غ��ֵ���Ŀ
}

// �õ����ּ�����Ӧ��4��Ӣ����ĸ
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

// �����������������ӳ���
// iStart:��ʼλ�ã�iEnd:����λ�ã�pIndex:�����ַ�����pString:�μӱȽϵ��ַ���
// ��ƴ����������
int CIMEDataBase::SearchHz (int iStart, int iEnd, _SY_INDEX_ITEM* pIndex, unsigned char* pString)
{
	int iMid = 0;
	int iResult = 0;

	while( iStart <= iEnd )
	{
		iMid = ( iStart + iEnd ) / 2;
		iResult = memcmp( pIndex[iMid].cSymbol, pString, 6 );

		if( iResult == 0 )
			return iMid;    // �ҵ�����

		if( iResult > 0 )
			iEnd   = iMid - 1;
		else
			iStart = iMid + 1;
	}

	return( -1 );           // û�ҵ�
}

// �ú����������뺺��
int CIMEDataBase::SearchLx (int iStart, int iEnd, _HZ_INDEX_ITEM* pIndex, unsigned char* pString)
{
	int iMid = 0;
	int iResult = 0;

	while( iStart <= iEnd )
	{
		iMid = ( iStart + iEnd ) / 2;
		iResult = memcmp( pIndex[iMid].cCharacter, pString, 2 );

		if( iResult == 0 )
			return iMid;    // �ҵ�����

		if( iResult > 0 )
			iEnd   = iMid - 1;
		else
			iStart = iMid + 1;
	}

	return( -1 );           // û�ҵ�
}
/* END */
