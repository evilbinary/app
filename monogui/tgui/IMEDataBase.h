// IMEDataBase.h: interface for the CIMEDataBase class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__IMEDATABASE_H__)
#define __IMEDATABASE_H__

// �����������뷨���ļ��ĳ���(�ֽ���)
// #if defined(RUN_ENVIRONMENT_LINUX)

#define IME_PY_LIB_LENGTH		17407
#define IME_EX_LIB_LENGTH		58296
#define IME_LX_LIB_LENGTH		41206
#define IME_PY_INDEX_LENGTH		6528
#define IME_EX_INDEX_LENGTH		6576
#define IME_LX_INDEX_LENGTH		35604

// #endif // defined(RUN_ENVIRONMENT_LINUX)
// ��Linuxϵͳ�£������ɳ��򴴽�ƴ������������ֱ�Ӵ����е��������ļ�

// ��������ּ���Ӧ��Ӣ����ĸ
const unsigned char Key0[4] = {'\0','\0','\0','\0'};
const unsigned char Key1[4] = {'\0','\0','\0','\0'};
const unsigned char Key2[4] = { 'a', 'b', 'c','\0'};
const unsigned char Key3[4] = { 'd', 'e', 'f','\0'};
const unsigned char Key4[4] = { 'g', 'h', 'i','\0'};
const unsigned char Key5[4] = { 'j', 'k', 'l','\0'};
const unsigned char Key6[4] = { 'm', 'n', 'o','\0'};
const unsigned char Key7[4] = { 'p', 'q', 'r', 's'};
const unsigned char Key8[4] = { 't', 'u', 'v','\0'};
const unsigned char Key9[4] = { 'w', 'x', 'y', 'z'};

// ���뷨�������һ����Ԫ
typedef struct _SY_INDEX_ITEM
{
	unsigned char cSymbol[7];
	DWORD dwStart;
	WORD  wLength;
}SY_INDEX_ITEM;

// ����ʻ��������һ����Ԫ
typedef struct _HZ_INDEX_ITEM
{
	unsigned char cCharacter[3];
	DWORD dwStart;
	WORD  wLength;
}HZ_INDEX_ITEM;

class CIMEDataBase
{
private:
	int m_iCurIME;				// ��ǰ���뷨��1��׼ƴ����2������չ

	char m_cSyLength;			// ƴ��������Ѿ�������ַ���Ŀ
	char m_cSymbol [7];			// ƴ������������������ԭʼ�ַ�(����)

	unsigned char* m_pPyLib;	// ��׼ƴ�����뷨��
	int m_iPyCount;				// ��׼ƴ�����뷨����Ŀ��
	_SY_INDEX_ITEM* m_pPyTable;	// ��׼ƴ�����뷨������

	unsigned char* m_pExLib;	// ������չ���뷨��
	int m_iExCount;				// ������չ���뷨����Ŀ��
	_SY_INDEX_ITEM* m_pExTable;	// ������չ���뷨������

	unsigned char* m_pLxLib;	// ����ʿ�
	int m_iLxCount;				// ����ʿ����Ŀ��
	_HZ_INDEX_ITEM* m_pLxTable;	// ����ʿ�������

	int m_iSyCount;				// ƴ�����ؼ��е�Ԫ����Ŀ
	unsigned char* m_pSy;		// ƴ����ѯ�ķ��ؼ�(7���ֽ�һ�飬ÿ���һ���ֽ���״̬��)
	
	int m_iHzCount;				// ���ַ��ؼ��ĳ���
	unsigned char* m_pHzTable;	// ���뷨�ĺ����ַ���

	int m_iLxHzCount;			// ����ʻ㷵�ؼ��ĳ���
	unsigned char* m_pLxHzTable;// ����ĺ����ַ���

public:
	CIMEDataBase ();
	virtual ~CIMEDataBase ();

	// ��ʼ�����뷨��
	BOOL Init ();

	// �趨��ǰ���뷨��1��׼ƴ����2������չ
	int SetCurIME (int iIME);

	// ����������һ���ַ�
	BOOL SyAdd (char ch);

	// ɾ�����������һ���ַ�
	BOOL SyRemove ();

	// ��������
	BOOL SyRemoveAll ();

	// �õ������ĳ���
	int GetSyLength ();

	// ����ƴ����������ƥ���ƴ��
	int GetSy (unsigned char** pSy);

	// ����ƴ�����Һ���
	int GetHz (unsigned char* pInput, unsigned char** pResult);

	// ���ݺ��ֲ���������
	int GetLx (unsigned char* pInput, unsigned char** pResult);

private:
	// �õ����ּ�����Ӧ��4��Ӣ����ĸ
	void GetKeySymbol (char* pKey, char cNum);

	// �����������������ӳ���
	// iStart:��ʼλ�ã�iEnd:����λ�ã�pIndex:�����ַ�����pString:�μӱȽϵ��ַ���
	// ��ƴ����������
	int SearchHz (int iStart, int iEnd, _SY_INDEX_ITEM* pIndex, unsigned char* pString);

	// �ú����������뺺��
	int SearchLx (int iStart, int iEnd, _HZ_INDEX_ITEM* pIndex, unsigned char* pString);
};

#endif // !defined(__IMEDATABASE_H__)

