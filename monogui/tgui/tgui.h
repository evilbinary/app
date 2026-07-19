// tgui.h: interface of the TGUI system.
//
//////////////////////////////////////////////////////////////////////
#if !defined(__TGUI_H__)
#define __TGUI_H__

#if defined(RUN_ENVIRONMENT_WIN32)
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#endif // defined(RUN_ENVIRONMENT_WIN32)

#if defined(RUN_ENVIRONMENT_LINUX)
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/stat.h>
#include "screen.h"

#endif // defined(RUN_ENVIRONMENT_LINUX)

#include "KeyMap.h"             // ������Ϣӳ���
#include "TKeyMapMgt.h"         // �����궨���밴��ֵ���ձ�
#include "LCD.h"                // Һ������������(GAL��)
#include "TAccell.h"            // ��ݼ�
#include "TCaret.h"             // ���ַ�
#include "TScroll.h"            // ������
#include "TMessageQueue.h"      // ��Ϣ����
#include "TTimerQueue.h"        // ��ʱ������
#include "TDlgTemplet.h"        // �Ի���ģ��
#include "TWindow.h"            // ����
#include "TStatic.h"            // ��̬�ı��ؼ�
#include "TButton.h"            // ��ť�ؼ�
#include "TEdit.h"              // �༭��
#include "TList.h"              // �б���
#include "TCombo.h"             // ��Ͽ�
#include "TProgress.h"          // ������
#include "TCheckBox.h"          // ��ѡ��
#include "IMEWindow.h"          // ���뷨����
#include "IMEDataBase.h"        // ���뷨���ݿ�
#include "TDialog.h"            // �Ի���
#include "TMessageBoxDialog.h"  // MessageBox�Ի���
#include "TApp.h"               // ������
#include "TClock.h"             // �ӱ�����
#include "TSystemBar.h"         // ϵͳ״̬��
#include "TSystemCommon.h"      // ����ȫ�ֺ���������
#include "TDtmMgt.h"            // �Ի���ģ����Դ������
#include "TImgMgt.h"            // �ڰ�λͼ��Դ������

// ���尴�����밴��ֵ���ձ����ļ���
#if defined(RUN_ENVIRONMENT_LINUX)
#define KEY_MAP_FILE "KeyMap.txt"
#else
#define KEY_MAP_FILE "KEYMAP.txt"
#endif // defined(RUN_ENVIRONMENT_LINUX)

// ������dtm�ļ���Ŀ¼
#define DIRECTORY_DTM     "/dtm" //dtm
#define DIRECTORY_IMG     "/img" //img

// ������Ļ�ߴ�
#define SCREEN_W	240
#define SCREEN_H	128

// ���洰�ڵĳߴ磺1:��ͨ��2:˫��
#define SCREEN_MODE	1

// ���崰������
#define TWND_VALID_BEGIN          99
#define	TWND_TYPE_DESKTOP         100
#define	TWND_TYPE_DIALOG          101
#define	TWND_TYPE_STATIC          102
#define	TWND_TYPE_BUTTON          103
#define	TWND_TYPE_EDIT            104
#define	TWND_TYPE_LIST            105
#define	TWND_TYPE_COMBO           106
#define	TWND_TYPE_PROGRESS        107
#define TWND_TYPE_CHECK_BOX       108
#define	TWND_TYPE_IME             109
#define TWND_VALID_END            110

// ������ַ��
#define TWND_STYLE_NORMAL         0x0
#define TWND_STYLE_NO_BORDER      0x0001      // �ޱ߿�(���ڰ�ť�ͶԻ���֮��Ŀؼ�)
#define TWND_STYLE_ROUND_EDGE     0x0002      // Բ��(ֻ���ڶԻ���������ؼ�)
#define TWND_STYLE_PASSWORD       0x0004      // ����(ֻ���ڱ༭��)
#define TWND_STYLE_GROUP          0x0008      // ���(ֻ���ھ�̬�ı�)
#define TWND_STYLE_SOLID          0x0010      // ����Ч��(�������κοؼ�������ֻ���ڶԻ���Ͱ�ť)
#define TWND_STYLE_ORIDEFAULT     0x0020      // ԭʼĬ�ϰ�ť(ֻ���ڰ�ť)
#define TWND_STYLE_NO_TITLE       0x0040      // ����ͷ(ֻ���ڶԻ���)
#define TWND_STYLE_DISABLE_IME    0x0080      // �������뷨(ֻ�Ա༭�����Ͽ���Ч)
#define TWND_STYLE_NO_SCROLL      0x0100      // �޹�����(ֻ���б������Ͽ���Ч)
#define TWND_STYLE_AUTO_DROPDOWN  0x0200      // �Զ����������˵�(ֻ����Ͽ���Ч)
#define TWND_STYLE_AUTO_OPEN_IME  0x0400      // �Զ���ƴ�����뷨
#define TWND_STYLE_IGNORE_ENTER   0x0800      // �༭����Ӧ�س�
#define TWND_STYLE_CLOSE_BUTTON   0x1000      // ���Ͻ���ʾ�رհ�ť(ֻ���ڶԻ���)

// �������״̬
#define	TWND_STATUS_NORMAL        0x0
#define TWND_STATUS_INVISIBLE     0x0001      // ���ɼ�(���ɼ�ͬʱ��Ч)
#define TWND_STATUS_INVALID       0x0002      // ��Ч(��ֻ��ʾ����������Ϣ�����ܱ���Ϊ����)
#define TWND_STATUS_FOCUSED       0x0004      // ���ڽ����
#define TWND_STATUS_DEFAULT       0x0008      // Ĭ�ϵ�(ֻ���ڰ�ť)
#define TWND_STATUS_CHECKED       0x0010      // ѡ�е�(ֻ����ѡ���)
#define TWND_STATUS_PUSHED        0x0020      // �����µ�(ֻ���ڰ�ť)

// ���������Ϣ
// ϵͳ��Ϣʹ��TMSG_��ͷ���Զ�����Ϣ��MSG_��ͷ
#define TMSG_QUIT                 999         // �����˳���Ϣ
#define TMSG_CLOSE                1000        // ���ڹر���Ϣ
#define TMSG_TIMER                1001        // ��ʱ����Ϣ
#define TMSG_PAINT                1002        // ������Ϣ
#define TMSG_SETCHILDACTIVE       1003        // �����Ӵ��ڳ�Ϊ����
#define TMSG_SETFOCUS             1004        // ���ý���
#define TMSG_KILLFOCUS            1005        // ʧȥ����
#define TMSG_DELETECHILD          1006        // ɾ���Ӵ���
#define TMSG_DATACHANGE           1007        // ���ݸı�
#define TMSG_KEYDOWN              1008        // ������Ϣ
#define TMSG_PUSHDOWN             1009        // ��ť������
#define TMSG_CHAR                 1010        // �����ַ�
#define TMSG_NOTIFY_PARENT        1011        // ֪ͨ������
#define TMSG_MESSAGEBOX_RETURN    1012        // MessageBox����
#define TMSG_INITWINDOW           1013        // ���ڴ�����ĳ�ʼ��
#if defined(MOUSE_SUPPORT)
#define TMSG_LBUTTONDOWN          1014        // ����������
#define TMSG_LBUTTONUP            1015        // ����������
#define TMSG_MOUSEMOVE            1016        // ����ƶ�
#endif // defined(MOUSE_SUPPORT)
#define TMSG_USER                 1200

// �������Ĭ��ID
#define TSID_MESSAGE_BOX          10009
#define TSID_OK                   10010
#define TSID_CANCEL               10011
#define TSID_CLOSE                10012
#define TSID_YES                  10013
#define TSID_NO                   10014

#define TSID_USER                 10200

//����Ĭ����Ϣ���ID
#define ID_DLG_MSG_DEFAULT        20000


//����linuxר�õ�GPIO
#if defined(RUN_ENVIRONMENT_LINUX)

#define CLOCK_FOLD     1000
#define GPIO_BASE      0xe0000000
#define KBD_LOCK_BASE  0x0a000000
#define PCCR           0x520
#define PCDR           0x524

#endif // defined(RUN_ENVIRONMENT_LINUX)


#endif // !defined(__TGUI_H__)



