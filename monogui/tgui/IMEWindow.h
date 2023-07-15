// IMEWindow.h: interface for the CIMEWindow class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__IMEWINDOW_H__)
#define __IMEWINDOW_H__

// 定义各种输入法的名称
const unsigned char cIMEName[25] = {"拼音扩展小写大写符号手写"};
//const unsigned char cIMEName[49] = {"标准拼音方言扩展英文小写英文大写标点符号手写输入"};

// 定义各种符号
#define IME_PUNCTUATION_NUM 33		// 有33个字符
const unsigned char strPunctuation[] =
{
' ', 0x20,  '!', 0x20, '\"', 0x20,  '#', 0x20,  '$', 0x20,  '%', 0x20,  '&', 0x20, '\'', 0x20,  '(', 0x20,
')', 0x20,  '*', 0x20,  '+', 0x20,  ',', 0x20,  '-', 0x20,  '.', 0x20,  '/', 0x20,  ':', 0x20,  ';', 0x20,
'<', 0x20,  '=', 0x20,  '>', 0x20,  '?', 0x20,  '@', 0x20,  '[', 0x20, '\\', 0x20,  ']', 0x20,  '^', 0x20,
'_', 0x20,  '`', 0x20,  '{', 0x20,  '|', 0x20,  '}', 0x20,  '~', 0x20
};

#define IME_TIMER_ID			100	// 输入法窗口使用的定时器ID值
#define IME_KEY_PRESS_GAP		2000	// 英文输入法中按键间隔的最大秒数
// 注：如果两次按下同一个键不超过这个时间，则切换字符，超过则输入新的字符
#define IME_CURRENT_UPPER		0	// 当前输入为上栏
#define IME_CURRENT_LOWER		1	// 当前输入为下栏
#define IME_OUT_STRING_LIMIT	32	// 英文输入法中输入字符串的最大长度

class CIMEDataBase;

class CIMEWindow : public CTWindow	// (从CTWindow派生为了能够接收消息)
{
public:
	int m_iCurIME;                  // 当前输入法：0:不打开输入法；1:标准拼音；2:方言扩展；
                                        // 3:英文小写；4:英文大写；5:标点符号；6:手写板输入；
	int m_iCurInputLine;		// 当前输入行：0:上边的拼音行；1:下边的汉字行。

	int m_iGraffitiPort;            // 设置手写板所使用的端口
                                        // 0:未使用手写板；1:使用串口1；2:使用串口2；

	BOOL m_bGraffitiPortStatus;     // 手写板端口的打开状态
	BOOL m_bGraffitiProcRun;        // 控制线程退出的变量
	BOOL m_bCleanReadCom;           // 清除串口缓冲区残留内容
	pthread_mutex_t m_mutexReadCom; // 用于阻塞线程运行的户斥体
	pthread_t m_threadReadCom;

private:
	CTWindow* m_pTargetWnd;         // 输出汉字的目标窗口
	CIMEDataBase* m_pIMEDB;         // 输入法数据库

	int m_iOutStrLength;		// 有效输出字符串的长度(只用于英文输入法)

	// 供选择、翻页用(只用于拼音输入法、扩展输入法和符号)
	BOOL m_bAllSyInvalid;           // 所有拼音都无效的标志(供显示用)
	int m_iSyLength;		// 单个拼音的字母数(总数)
	int m_iSyValidCount;            // 可以显示的拼音的数目(上行)
	int m_iSyCount;                 // 查询出来的拼音的数目
	int m_iCurSy;			// 当前选中的拼音(上行，0-based)

	int m_iHzCount;			// 汉字的数目(下行，每行显示9个)
	int m_iCurPage;			// 当前页号(0-based)

	// 上下行的文字内容
	unsigned char m_cUpperLine [33];	// 上行文字，32个字符
	unsigned char m_cLowerLine [37];	// 下行文字，36个字符

	// 输入法返回集
	unsigned char* m_cSyReturn;
	unsigned char* m_cHzReturn;

	// 英文输入的按键等待超时标志
	int m_iTimeOut;		// 0未超时; 1超时

public:
	CIMEWindow();
	virtual ~CIMEWindow();

	// 创建输入法窗口
	virtual BOOL Create( CTApp* pApp );

	// 绘制输入法窗口
	virtual void Paint( CLCD* pLCD );

	// 消息处理过了，返回1，未处理返回0
	virtual int Proc( CTWindow* pWnd, int iMsg, int wParam, int lParam );

	// 是否可以处理控制按键(如果输入窗是空的，则不处理控制按键，而让CTEdit窗口去处理)
	BOOL CanHandleControlKey();

	// 打开输入法窗口(打开显示，创建联系)
	BOOL OpenIME( CTWindow* pWnd );

	// 关闭输入法窗口(关闭显示，断开联系)
	BOOL CloseIME( CTWindow* pWnd );

	// 看输入法窗口是否处于打开状态
	BOOL IsIMEOpen();

	// 设置手写输入的相关参数
	BOOL EnableGraffiti( int nSerialPort );

private:
	// 处理定时器消息(只用于英文输入法)
	virtual void OnTimer( int iTimerID, int iInterval );

	// 向目标窗口发送字符(如果是英文，则只有c1，c2为0)
	void SendChar( unsigned char c1, unsigned char c2 );

	// 根据数字件1~9选择字符并发送
	unsigned char* LowerLineGetChar( int iKeyValue, int iLength );

	// 三种不同输入法的处理函数
	// 拼音输入法
	void PinYinProc( CTWindow* pWnd, int iMsg, int wParam, int lParam );

	// 英文输入法
	void YingWenProc( CTWindow* pWnd, int iMsg, int wParam, int lParam );

	// 符号输入法
	void PunctuationProc( CTWindow* pWnd, int iMsg, int wParam, int lParam );

	// 手写输入法(外部设备通过端口输入备选文字)
	void GraffitiProc( CTWindow* pWnd, int iMsg, int wParam, int lParam );

	// 根据m_cSyReturn、m_iSyLength、m_iSyCount更新上栏的字符串m_cUpperLine并设置m_iSyValidCount
	void RenewUpperLine();

	// 修改拼音以后的后续处理
	BOOL RenewSy();

	// 根据m_cHzReturn、m_iHzCount、m_iCurPage更新下栏的字符串cLowerLine
	void RenewLowerLine();

	// 下行分页的向前翻页处理
	void LowerLinePageUp();

	// 下行分页的向后翻页处理
	void LowerLinePageDown();

	// 将按键消息转换成按键字符
	char MessageToKey( int iMessage );

	// 根据字母查找对应的数字键
	char CharToKey( char cc );

	// 根据数字键查找对应的字母(返回第一个)
	char KeyToChar( char cKey );

	// 查找该字母所在数字键组的下一个字母
	char KeyNextChar( char cc );
};

#endif // !defined(__IMEWINDOW_H__)
