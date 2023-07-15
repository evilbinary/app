// TDlgTemplet.h: interface for the CTDlgTemplet class.
//
//////////////////////////////////////////////////////////////////////
#if !defined(__TDLGTEMPLET_H__)
#define __TDLGTEMPLET_H__

typedef struct _CTRLDATA
{
	WORD  wType;            // 控件类型
	WORD  wStyle;           // 控件风格
	int   x, y, w, h;       // 控件相对父窗口的位置
	int   id;               // 控件ID号
	char  caption[256];     // 控件口题头文字
	int   iAddData;         // 附加数据(CTEdit中是最大字符串长度)
	_CTRLDATA*	next;       // 指向链表中的下一个
}CTRLDATA;

typedef struct _DLGTEMPLET
{
     WORD  wStyle;          // 对话框风格
     int   x, y, w, h;      // 对话框相对桌面的显示位置
     char  caption[256];    // 对话框题头文字
     int   id;              // 对话框的id号
     int   controlnr;       // 对话框附带的控件数目
     CTRLDATA*  controls;   // 控件模板链表的指针
     char*  pAccellTable;   // 快捷键列表的文本
}DLGTEMPLET;

class CTDlgTemplet
{
private:
	struct _DLGTEMPLET* m_pT;

public:
	CTDlgTemplet();
	virtual ~CTDlgTemplet();

public:
	// 从模板文件初始化对话框模板
	struct _DLGTEMPLET* OpenDlgTemplet( const char* filename );

	// 删除对话框模板
	// 应当依照链表从最后一个开始delete，直到删光所有控件模板
	BOOL DeleteDlgTemplet();

	// 获得模板结构体的指针
	struct _DLGTEMPLET* GetTemplet();

private:
	// 从字符串中获取对话框设置
	BOOL GetDialogInfoFromString(char* pString);

	// 从字符串中获取控件设置
	BOOL GetControlInfoFromString(char* pString);
};

#endif // !defined(__TDIALOG_H__)
