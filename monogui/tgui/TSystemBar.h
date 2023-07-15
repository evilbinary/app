// TSystemBar.h

#if !defined(__TSYSTEMBAR_H__)
#define __TSYSTEMBAR_H__

#if !defined(BOOL)
#define BOOL int
#endif // !defined(BOOL)

#define SYSTEM_BAR_W      64
#define SYSTEM_BAR_H      16

class CTApp;

class CTSystemBar
{
private:
	int  m_nStatus;       // ��ʾ״̬��0:����ʾ��1:��ʾ��磻2:��ʾ��أ�
	int  m_nBattery;      // ��ص�����0 ~ 100��
	BOOL m_bCaps;         // Caps״̬��

public:
	CTSystemBar();
	virtual ~CTSystemBar();

public:
	// ��ʾϵͳ״̬����
	void Show( CLCD* pLCD );

#if defined(MOUSE_SUPPORT)
	// ������л���Сд״̬����
	BOOL PtProc( int x, int y );
#endif // defined(MOUSE_SUPPORT)

	// ����״̬��0:����ʾ��1:��ʾ��磻2:��ʾ��أ�
	BOOL SetStatus( int nStatus );

	// ���õ�ص���ֵ��0 ~ 100��
	BOOL SetBattery( int nValue );

	// �õ���ǰ��ص���ֵ��
	int GetBattery();

	// ���ô�Сд״ָ̬ʾ��
	BOOL SetCaps( BOOL bValue );

	// �õ���ǰ�Ĵ�Сд״̬��
	BOOL GetCaps();
};

#endif // !defined(__TSYSTEMBAR_H__)
