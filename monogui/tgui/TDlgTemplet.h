// TDlgTemplet.h: interface for the CTDlgTemplet class.
//
//////////////////////////////////////////////////////////////////////
#if !defined(__TDLGTEMPLET_H__)
#define __TDLGTEMPLET_H__

typedef struct _CTRLDATA
{
	WORD  wType;            // �ؼ�����
	WORD  wStyle;           // �ؼ����
	int   x, y, w, h;       // �ؼ���Ը����ڵ�λ��
	int   id;               // �ؼ�ID��
	char  caption[256];     // �ؼ�����ͷ����
	int   iAddData;         // ��������(CTEdit��������ַ�������)
	_CTRLDATA*	next;       // ָ�������е���һ��
}CTRLDATA;

typedef struct _DLGTEMPLET
{
     WORD  wStyle;          // �Ի�����
     int   x, y, w, h;      // �Ի�������������ʾλ��
     char  caption[256];    // �Ի�����ͷ����
     int   id;              // �Ի����id��
     int   controlnr;       // �Ի��򸽴��Ŀؼ���Ŀ
     CTRLDATA*  controls;   // �ؼ�ģ�������ָ��
     char*  pAccellTable;   // ��ݼ��б���ı�
}DLGTEMPLET;

class CTDlgTemplet
{
private:
	struct _DLGTEMPLET* m_pT;

public:
	CTDlgTemplet();
	virtual ~CTDlgTemplet();

public:
	// ��ģ���ļ���ʼ���Ի���ģ��
	struct _DLGTEMPLET* OpenDlgTemplet( const char* filename );

	// ɾ���Ի���ģ��
	// Ӧ��������������һ����ʼdelete��ֱ��ɾ�����пؼ�ģ��
	BOOL DeleteDlgTemplet();

	// ���ģ��ṹ���ָ��
	struct _DLGTEMPLET* GetTemplet();

private:
	// ���ַ����л�ȡ�Ի�������
	BOOL GetDialogInfoFromString(char* pString);

	// ���ַ����л�ȡ�ؼ�����
	BOOL GetControlInfoFromString(char* pString);
};

#endif // !defined(__TDIALOG_H__)
