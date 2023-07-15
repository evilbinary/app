// IMEDataBase.h: interface for the CIMEDataBase class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__IMEDATABASE_H__)
#define __IMEDATABASE_H__

// 定义六个输入法库文件的长度(字节数)
// #if defined(RUN_ENVIRONMENT_LINUX)

#define IME_PY_LIB_LENGTH		17407
#define IME_EX_LIB_LENGTH		58296
#define IME_LX_LIB_LENGTH		41206
#define IME_PY_INDEX_LENGTH		6528
#define IME_EX_INDEX_LENGTH		6576
#define IME_LX_INDEX_LENGTH		35604

// #endif // defined(RUN_ENVIRONMENT_LINUX)
// 在Linux系统下，不再由程序创建拼音索引表，而是直接打开现有的索引表文件

// 定义各数字键对应的英文字母
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

// 输入法索引表的一个单元
typedef struct _SY_INDEX_ITEM
{
	unsigned char cSymbol[7];
	DWORD dwStart;
	WORD  wLength;
}SY_INDEX_ITEM;

// 联想词汇索引表的一个单元
typedef struct _HZ_INDEX_ITEM
{
	unsigned char cCharacter[3];
	DWORD dwStart;
	WORD  wLength;
}HZ_INDEX_ITEM;

class CIMEDataBase
{
private:
	int m_iCurIME;				// 当前输入法：1标准拼音；2方言扩展

	char m_cSyLength;			// 拼音输入表已经输入的字符数目
	char m_cSymbol [7];			// 拼音的输入表，保存输入的原始字符(数字)

	unsigned char* m_pPyLib;	// 标准拼音输入法库
	int m_iPyCount;				// 标准拼音输入法的条目数
	_SY_INDEX_ITEM* m_pPyTable;	// 标准拼音输入法索引表

	unsigned char* m_pExLib;	// 方言扩展输入法库
	int m_iExCount;				// 方言扩展输入法的条目数
	_SY_INDEX_ITEM* m_pExTable;	// 方言扩展输入法索引表

	unsigned char* m_pLxLib;	// 联想词库
	int m_iLxCount;				// 联想词库的条目数
	_HZ_INDEX_ITEM* m_pLxTable;	// 联想词库索引表

	int m_iSyCount;				// 拼音返回集中单元的数目
	unsigned char* m_pSy;		// 拼音查询的返回集(7个字节一组，每组第一个字节是状态字)
	
	int m_iHzCount;				// 汉字返回集的长度
	unsigned char* m_pHzTable;	// 输入法的汉字字符串

	int m_iLxHzCount;			// 联想词汇返回集的长度
	unsigned char* m_pLxHzTable;// 联想的汉字字符串

public:
	CIMEDataBase ();
	virtual ~CIMEDataBase ();

	// 初始化输入法库
	BOOL Init ();

	// 设定当前输入法：1标准拼音；2方言扩展
	int SetCurIME (int iIME);

	// 向输入表添加一个字符
	BOOL SyAdd (char ch);

	// 删除输入表的最后一个字符
	BOOL SyRemove ();

	// 清空输入表
	BOOL SyRemoveAll ();

	// 得到输入表的长度
	int GetSyLength ();

	// 根据拼音输入表查找匹配的拼音
	int GetSy (unsigned char** pSy);

	// 根据拼音查找汉字
	int GetHz (unsigned char* pInput, unsigned char** pResult);

	// 根据汉字查找联想字
	int GetLx (unsigned char* pInput, unsigned char** pResult);

private:
	// 得到数字键所对应的4个英文字母
	void GetKeySymbol (char* pKey, char cNum);

	// 二分搜索法的搜索子程序
	// iStart:开始位置；iEnd:结束位置；pIndex:索引字符串；pString:参加比较的字符串
	// 用拼音搜索汉字
	int SearchHz (int iStart, int iEnd, _SY_INDEX_ITEM* pIndex, unsigned char* pString);

	// 用汉字搜索联想汉字
	int SearchLx (int iStart, int iEnd, _HZ_INDEX_ITEM* pIndex, unsigned char* pString);
};

#endif // !defined(__IMEDATABASE_H__)

