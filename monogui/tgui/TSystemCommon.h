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
// ������Ϣ���
//#define DebugPrintf    printf
//#else
//#define DebugPrintf(x) do { } while(0);
//#endif // defined(MODE_DEBUG)

class CTWindow;
// ����ȫ�ֺ���

// ������Ϣ���
void DebugPrintf( const char* format, ... );

// ��ú������ֿ��е�ƫ����
int GetHzIndex( unsigned char cH, unsigned char cL, int nLen );

// �ж�һ�����Ƿ�����һ������������
BOOL PtInRect( int x, int y, int left, int top, int right, int bottom );

// ���ַ�����ȡ��һ��
BOOL GetLine(char* cString, char* cReturn, int k);

// ���ַ�����ȡ��������;��֮��Ĳ���
BOOL GetSlice(char* cString, char* cReturn, int k);

// TGUIϵͳ�Զ����ʱ��
ULONGLONG sys_clock();

// �ȴ���ʱ
void Delay(int iMilliSecond);

// ����һ����Ϣ��
void TMessageBox(CTWindow* pWnd, char* cTitle, char* cContent, WORD wStyle, int ID);

// ����һ��ģ̬����Ϣ��
int TModalMessageBox(CTWindow* pWnd, char* cTitle, char* cContent, WORD wStyle, int ID);

// �жϵ�ǰλ��ǰ��(�����)�������Ƿ���
// �Ǻ��ַ���TRUE�����Ǻ��ַ���FALSE
// cString:�ַ���
// iPos:��ǰλ�õ�����(�ڼ����ֽ�)
// bMode:TRUE������������ֽ�;FALSE���ǰ��������ֽ�
BOOL IsChinese(char* cString, int iPos, BOOL bMode);

// �жϵ�ǰλ���Ƿ������˺��ֵ������ֽ��м�
// �����м䷵��FALSE�����򷵻�TRUE
BOOL IsPosValid(char* cString, int iPos);

// �õ�һ���ַ�������ʾ����
int GetDisplayLength(char* cString, int iLength);

// ����һ����Ⱥ�һ���ַ������õ�������ʾ���ֽ���
int GetDisplayLimit(char* cString, int iWidth);

// ����һ����Ⱥ�һ���ַ������õ�������ʾ���ֽ���
int GetDisplayLimit_1(char* cString, int iWidth);

// ���涨��TGUI���õ��ĸ�����Դ
// ���ϼ�ͷ(7*7)
extern unsigned char pBitmap_Arror_Up[49];
// ���¼�ͷ(7*7)
extern unsigned char pBitmap_Arror_Down[49];
// �����ͷ(7*7)
extern unsigned char pBitmap_Arror_Left[49];
// ���Ҽ�ͷ(7*7)
extern unsigned char pBitmap_Arror_Right[49];
// ���Ͽ��ļ�ͷ(7*7)
extern unsigned char pBitmap_Hollow_Arror_Up[49];
// ���¿��ļ�ͷ(7*7)
extern unsigned char pBitmap_Hollow_Arror_Down[49];
// ������ļ�ͷ(7*7)
extern unsigned char pBitmap_Hollow_Arror_Left[49];
// ���ҿ��ļ�ͷ(7*7)
extern unsigned char pBitmap_Hollow_Arror_Right[49];
// С���ϼ�ͷ(5*5)
extern unsigned char pBitmap_Little_Arror_Up[49];
// С���¼�ͷ(5*5)
extern unsigned char pBitmap_Little_Arror_Down[49];
// С�����ͷ(5*5)
extern unsigned char pBitmap_Little_Arror_Left[49];
// С���Ҽ�ͷ(5*5)
extern unsigned char pBitmap_Little_Arror_Right[49];
// �������м�Ļ�����ť(11*11)
extern unsigned char pBitmap_Scroll_Button[121];
// Բ�ǰ�ť������(13*13)
extern unsigned char pBitmap_Button_Normal[169];
// Բ�ǰ�ť��Ĭ��(13*13)
extern unsigned char pBitmap_Button_Default[169];
// ����ͼ��(23*23)
extern unsigned char pBitmap_Icon_Error[529];
// ��̾��ͼ��(23*23)
extern unsigned char pBitmap_Icon_Exclamation[529];
// �ʺ�ͼ��(23*23)
extern unsigned char pBitmap_Icon_Question[529];
// ��Ϣͼ��(23*23)
extern unsigned char pBitmap_Icon_Information[529];
// æͼ��(23*23)
extern unsigned char pBitmap_Icon_Busy[529];
// ��ӡ��ͼ��(23*23)
extern unsigned char pBitmap_Icon_Printer[529];
// ѡ��״̬�ĸ�ѡ��ͼ��(13*13)
extern unsigned char pBitmap_Check_Box_Selected[169];
// δѡ��״̬�ĸ�ѡ��ͼ��(13*13)
extern unsigned char pBitmap_Check_Box_Unselected[169];
// ���ǹرհ�ť(15*15)
extern unsigned char pBitmap_Close_Button1[225];
// Բ�ǹرհ�ť(15*15)
extern unsigned char pBitmap_Close_Button2[225];
// ʱ�Ӱ�ť(15*15)
extern unsigned char pBitmap_Clock_Button[225];
// ��Դ
extern unsigned char pBitmap_Power[546];
// ���
extern unsigned char pBitmap_Battery[546];
// ��д��ĸ
extern unsigned char pBitmap_Captial[156];
// Сд��ĸ
extern unsigned char pBitmap_Lowercase[156];
//
// ����
#define CLOCK_W 182
#define CLOCK_H 82
extern unsigned char pcaClockFace[14924];
// ������������
#define CENTER_X 33
#define CENTER_Y 45
// ����ʱ��ͷ�����յ�
extern unsigned char HOUR_HAND[12][2];
extern unsigned char MINUTE_HAND[60][2];
//
#endif // !defined(__TSYSTEMCOMMON_H__)
