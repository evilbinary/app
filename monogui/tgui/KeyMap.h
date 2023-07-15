// KeyMap.h: define all TVK_ key value;
//
//////////////////////////////////////////////////////////////////////
// 本文件定义了全部键值
#if !defined(__KEYMAP_H__)
#define __KEYMAP_H__

// 数字键
#define TVK_0               0X30 
#define TVK_1               0X31
#define TVK_2               0X32
#define TVK_3               0X33
#define TVK_4               0X34
#define TVK_5               0X35
#define TVK_6               0X36
#define TVK_7               0X37
#define TVK_8               0X38
#define TVK_9               0X39
#define TVK_00              0X71

// 字母键
#define TVK_A               0X41
#define TVK_B               0X42
#define TVK_C               0X43
#define TVK_D               0X44
#define TVK_E               0X45
#define TVK_F               0X46
#define TVK_G               0X47
#define TVK_H               0X48
#define TVK_I               0X49
#define TVK_J               0X4A
#define TVK_K               0X4B
#define TVK_L               0X4C
#define TVK_M               0X4D
#define TVK_N               0X4E
#define TVK_O               0X4F
#define TVK_P               0X50
#define TVK_Q               0X51
#define TVK_R               0X52
#define TVK_S               0X53
#define TVK_T               0X54
#define TVK_U               0X55
#define TVK_V               0X56
#define TVK_W               0X57
#define TVK_X               0X58
#define TVK_Y               0X59
#define TVK_Z               0X5A

#define TVK_CLOCK           0xBE   //当前时间

// 功能键
// #define TVK_F1              0XFFBE
#define TVK_F2              0XBF
#define TVK_F3              0XC0
#define TVK_F4              0XC1
#define TVK_F5              0XC2
#define TVK_F6              0XC3
#define TVK_F7              0XC4
#define TVK_F8              0XC5
#define TVK_F9              0XC6
#define TVK_F10             0XC7
#define TVK_F11             0XC8	// 用于打开 / 关闭输入法
#define TVK_F12             0XC9	// 用于切换输入法

// 方向键
#define TVK_LEFT            0X51
#define TVK_UP              0x52
#define TVK_RIGHT           0X53
#define TVK_DOWN            0X54

// 符号键
#define TVK_EQUALS          0X6D	// (=)
#define TVK_ADD             0XBB	// (+)
#define TVK_SEMICOLON       0X6B	// (;)
#define TVK_BACK_QUOTE      0X6A	// (`)
#define TVK_BACK_SLASH      0XDC	// (\)
#define TVK_DIVIDE          0X6F	// (/)
#define TVK_COMMA           0XBC	// (,)
#define TVK_PERIOD          0XBE	// (.)
#define TVK_OPEN_BRACKET    0X0D	// ([)
#define TVK_CLOSE_BRACKET   0XDD	// (])

// 编辑控制键
#define TVK_SPACE           0X20
#define TVK_BACK_SPACE      0X08
#define TVK_TAB             0x09
#define TVK_INSERT          0X2D
#define TVK_DELETE          0X2E
#define TVK_HOME            0X24
#define TVK_END             0X23
#define TVK_PAGE_UP         0X21
#define TVK_PAGE_DOWN       0X22

// 其他按键
#define TVK_ENTER           0X0D
#define TVK_ESCAPE          0X1B
#define TVK_CONTROL         0X11
#define TVK_SHIFT           0X10
#define TVK_PAUSE           0X13	// (BREAK)
#define TVK_CAPS_LOCK       0X14
#define TVK_NUM_LOCK        0X90

// 特殊按键掩码
// mask key flag
#define SHIFT_MASK          0x01
#define CTRL_MASK           0x02
#define ALT_MASK            0x04
#define CAPSLOCK_MASK       0x10
#define NUMLOCK_MASK        0x20
#define SCROLLLOCK_MASK     0x40
#define INS_MASK            0x80

#endif // !defined(__KEYMAP_H__)

