// TSystemCommon.h: global functions.
//
//////////////////////////////////////////////////////////////////////
#if defined(RUN_ENVIRONMENT_LINUX)
#include <termios.h>
#endif // defined(RUN_ENVIRONMENT_LINUX)

#if !defined(__TSYSTEMCOMMON_H__)
#define __TSYSTEMCOMMON_H__

#if !defined(BYTE)
#define BYTE    unsigned char
#endif // !defined(BYTE)
#if !defined(WORD)
#define WORD    unsigned short
#endif // !defined(WORD)
#if !defined(DWORD)
#define DWORD   unsigned long
#endif // !defined(DWORD)
#if !defined(UINT)
#define UINT    unsigned int
#endif // !defined(UINT)
#if !defined(LONG)
#define LONG    long
#endif // !defined(LONG)
#if !defined(BOOL)
#define BOOL    int
#endif // !defined(BOOL)

#if !defined(ULONGLONG)
#if defined(RUN_ENVIRONMENT_LINUX)
#define ULONGLONG unsigned long long
#else
#define ULONGLONG unsigned __int64
#endif // defined(RUN_ENVIRONMENT_LINUX)
#endif // !defined(ULONGLONG)

#if !defined(FALSE)
#define	FALSE	0
#endif // !defined(FALSE)
#if !defined(TRUE)
#define	TRUE	1
#endif // !defined(TRUE)
#if !defined(NULL)
#define	NULL	0x0
#endif // !defined(NULL)

#if defined(RUN_ENVIRONMENT_WIN32)
typedef struct _HKDATETIME
{
	int nYear;
	int nMonth;
	int nDay;
	int nHour;
	int nMinute;
	int nSecond;
}HKDATETIME;
#endif // defined(RUN_ENVIRONMENT_WIN32)

//#if defined(MODE_DEBUG)
// 调试信息输出
//#define DebugPrintf    printf
//#else
//#define DebugPrintf(x) do { } while(0);
//#endif // defined(MODE_DEBUG)

class CTWindow;
// 定义全局函数

// 调试信息输出
void DebugPrintf( const char* format, ... );

// 获得汉字在字库中的偏移量
int GetHzIndex( unsigned char cH, unsigned char cL, int nLen );

// 判断一个点是否落在一个矩形区域内
BOOL PtInRect( int x, int y, int left, int top, int right, int bottom );

// 从字符串中取得一行
BOOL GetLine(char* cString, char* cReturn, int k);

// 从字符串中取得两个“;”之间的部分
BOOL GetSlice(char* cString, char* cReturn, int k);

// TGUI系统自定义的时钟
ULONGLONG sys_clock();

// 等待延时
void Delay(int iMilliSecond);

// 弹出一个消息框
void TMessageBox(CTWindow* pWnd, char* cTitle, char* cContent, WORD wStyle, int ID);

// 弹出一个模态的消息框
int TModalMessageBox(CTWindow* pWnd, char* cTitle, char* cContent, WORD wStyle, int ID);

// 判断当前位置前面(或后面)的文字是否汉字
// 是汉字返回TRUE，不是汉字返回FALSE
// cString:字符串
// iPos:当前位置的索引(第几个字节)
// bMode:TRUE检查后面的两个字节;FALSE检查前面的两个字节
BOOL IsChinese(char* cString, int iPos, BOOL bMode);

// 判断当前位置是否塞在了汉字的两个字节中间
// 塞在中间返回FALSE，否则返回TRUE
BOOL IsPosValid(char* cString, int iPos);

// 得到一个字符串的显示长度
int GetDisplayLength(char* cString, int iLength);

// 给出一个宽度和一个字符串，得到可以显示的字节数
int GetDisplayLimit(char* cString, int iWidth);

// 给出一个宽度和一个字符串，得到可以显示的字节数
int GetDisplayLimit_1(char* cString, int iWidth);

// 下面定义TGUI中用到的各种资源
// 向上箭头(7*7)
extern unsigned char pBitmap_Arror_Up[49];
// 向下箭头(7*7)
extern unsigned char pBitmap_Arror_Down[49];
// 向左箭头(7*7)
extern unsigned char pBitmap_Arror_Left[49];
// 向右箭头(7*7)
extern unsigned char pBitmap_Arror_Right[49];
// 向上空心箭头(7*7)
extern unsigned char pBitmap_Hollow_Arror_Up[49];
// 向下空心箭头(7*7)
extern unsigned char pBitmap_Hollow_Arror_Down[49];
// 向左空心箭头(7*7)
extern unsigned char pBitmap_Hollow_Arror_Left[49];
// 向右空心箭头(7*7)
extern unsigned char pBitmap_Hollow_Arror_Right[49];
// 小向上箭头(5*5)
extern unsigned char pBitmap_Little_Arror_Up[49];
// 小向下箭头(5*5)
extern unsigned char pBitmap_Little_Arror_Down[49];
// 小向左箭头(5*5)
extern unsigned char pBitmap_Little_Arror_Left[49];
// 小向右箭头(5*5)
extern unsigned char pBitmap_Little_Arror_Right[49];
// 滚动条中间的滑动按钮(11*11)
extern unsigned char pBitmap_Scroll_Button[121];
// 圆角按钮—正常(13*13)
extern unsigned char pBitmap_Button_Normal[169];
// 圆角按钮—默认(13*13)
extern unsigned char pBitmap_Button_Default[169];
// 错误图标(23*23)
extern unsigned char pBitmap_Icon_Error[529];
// 感叹号图标(23*23)
extern unsigned char pBitmap_Icon_Exclamation[529];
// 问号图标(23*23)
extern unsigned char pBitmap_Icon_Question[529];
// 信息图标(23*23)
extern unsigned char pBitmap_Icon_Information[529];
// 忙图标(23*23)
extern unsigned char pBitmap_Icon_Busy[529];
// 打印机图标(23*23)
extern unsigned char pBitmap_Icon_Printer[529];
// 选中状态的复选框图标(13*13)
extern unsigned char pBitmap_Check_Box_Selected[169];
// 未选中状态的复选框图标(13*13)
extern unsigned char pBitmap_Check_Box_Unselected[169];
// 方角关闭按钮(15*15)
extern unsigned char pBitmap_Close_Button1[225];
// 圆角关闭按钮(15*15)
extern unsigned char pBitmap_Close_Button2[225];
// 时钟按钮(15*15)
extern unsigned char pBitmap_Clock_Button[225];
// 电源
extern unsigned char pBitmap_Power[546];
// 电池
extern unsigned char pBitmap_Battery[546];
// 大写字母
extern unsigned char pBitmap_Captial[156];
// 小写字母
extern unsigned char pBitmap_Lowercase[156];
//
// 表盘
#define CLOCK_W 182
#define CLOCK_H 82
extern unsigned char pcaClockFace[14924];
// 定义表针的中心
#define CENTER_X 33
#define CENTER_Y 45
// 定义时针和分针的终点
extern unsigned char HOUR_HAND[12][2];
extern unsigned char MINUTE_HAND[60][2];
//
#endif // !defined(__TSYSTEMCOMMON_H__)
