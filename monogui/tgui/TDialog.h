// TDialog.h: interface for the CTDialog class.
//
//////////////////////////////////////////////////////////////////////
#if !defined(__TDIALOG_H__)
#define __TDIALOG_H__

class CTWindow;
class CLCD;
class CTAccell;
class CTDlgTemplet;
struct _CARET;

class CTDialog : public CTWindow
{
public:
	int m_iDoModalReturn;   // ָ��DoModal�����ķ���ֵ
	CTAccell* m_pAccell;   // ��ݼ��б�

public:
	CTDialog();
	virtual ~CTDialog();

	// �麯�������ƶԻ���
	virtual void Paint(CLCD* pLCD);

	// �麯������Ϣ����
	// ��Ϣ������ˣ�����1��δ������0
	virtual int Proc(CTWindow* pWnd, int iMsg, int wParam, int lParam);

#if defined(MOUSE_SUPPORT)
	// �����豸��Ϣ����
	virtual int PtProc (CTWindow* pWnd, int iMsg, int wParam, int lParam);
#endif // defined(MOUSE_SUPPORT)

	// ����ģʽ״̬
	virtual int DoModal();

	// ����ID�ţ�ʹ����Ӧ��ģ���ļ������Ի���
	virtual BOOL CreateFromID( CTWindow* pWnd, int iID );

	// �ӶԻ���ģ���ļ������Ի���
	BOOL CreateFromFile( CTWindow* pParent, const char* filename );

	// �ӶԻ���ģ��ṹ�崴���Ի���
	BOOL CreateFromTemplet( CTWindow* pParent, _DLGTEMPLET* pT );

	// �������л�
	virtual BOOL OnChangeFocus();

	// �رնԻ���
	void CloseDialog();

private:
	// ���ƶԻ���
	// 0,�ǽ��㴰�ڣ�1,������ʾ(���㴰��)
	void DrawDialog(CLCD* pLCD, int iMode);
};

#endif // !defined(__TDIALOG_H__)
