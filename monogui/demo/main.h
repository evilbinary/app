// main.h

#if !defined(__MAIN_H__)
#define __MAIN_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// #include <X11/Xlib.h>
// #include <X11/Xutil.h>
// #include <X11/Xos.h>
// #include <X11/Xatom.h>
// #include <pthread.h>

#include <SDL.h>
#include "screen.h"

 #define bool char

#define RUN_ENVIRONMENT_LINUX
#define MOUSE_SUPPORT
#define RESMGT_SUPPORT
#define CHINESE_SUPPORT

#define LCDW   SCREEN_W
#define LCDH   SCREEN_H

#define COLORBLACK	0x00ff
#define COLORWHITE	0xff00

#include "../tgui/tgui.h"
#include "MainWindow.h"
#include "DlgShowButtons.h"
#include "DlgShowEdit.h"
#include "DlgShowCombo.h"
#include "DlgShowList.h"
#include "DlgShowProgress.h"

#endif // !defined(__MAIN_H__)
