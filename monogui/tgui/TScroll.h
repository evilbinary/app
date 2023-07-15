// TScroll.h: interface for the CTScroll class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TSCROLL_H__)
#define __TSCROLL_H__

#define SCROLL_WIDTH 13

#if defined(MOUSE_SUPPORT)
// �������������Ĳ��Խ��
#define SCROLL_PT_UP           1
#define SCROLL_PT_PAGEUP       2
#define SCROLL_PT_DOWN         3
#define SCROLL_PT_PAGEDOWN     4
#define SCROLL_PT_BUTTON       5
#endif // defined(MOUSE_SUPPORT)

class CTScroll
{
private:
	int m_iRange;		// �ƶ���Χ
	int m_iSize;		// �м䰴ť�Ĵ�С
	int m_iPos;			// ��ǰλ��
/* ���磺
 * һ���б����40���б��ÿҳ��ʾ6������ǰ������һ���ǵ�25������
 * m_iRange = 40;    m_iSize = 6;    m_iPos = 24;
 */

#if defined(MOUSE_SUPPORT)
	int m_iOldPos;      // ��¼�����а�ťʱ��λ��
	int m_iOldPt;       // ��¼�����а�ťʱ������
						// (��ֱ��������¼y���꣬ˮƽ��������¼x����)
#endif // defined(MOUSE_SUPPORT)

public:
	int m_iStatus;		// 0������ʾ��1����ʾ
	int m_iMode;		// 1����ֱ��������2��ˮƽ������
	int m_x;
	int m_y;
	int m_w;
	int m_h;			// ��Сλ��(�������Ŀ��ǿ����Ϊ13)

public:
	CTScroll ();
	virtual ~CTScroll ();

	// ������������Ӧָ��ˮƽ���Ǵ�ֱ��
	BOOL Create (int iMode, int x, int y, int w, int h, int iRange, int iSize, int iPos);

	// ���ƹ�����
	void Paint (CLCD* pLCD);

	// ���ù�����Χ
	BOOL SetRange (int iRange);

	// �����м䰴ť�Ĵ�С
	BOOL SetSize (int iSize);

	// ���õ�ǰλ��
	BOOL SetPos (int iPos);

#if defined(MOUSE_SUPPORT)
	// ��¼��ǰλ�ú͵�ǰ����
	BOOL RecordPos (int iPt);

	// �ж�������
	int TestPt (int x, int y);

	// ���������������Ӧ����λ��
	int TestNewPos (int x, int y);
#endif // defined(MOUSE_SUPPORT)
};

#endif // !defined(__TSCROLL_H__)
