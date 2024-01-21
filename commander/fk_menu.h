/*
    FK - FunKey retro gaming console library
    Copyright (C) 2020-2021 Vincent Buso
    Copyright (C) 2020-2021 Michel Stempin

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Vincent Buso
    vincent.buso@funkey-project.com
    Michel Stempin
    michel.stempin@funkey-project.com
*/

/**
 *  @file FK_menu.h
 *  This is the menu API for the FunKey retro gaming console library
 */

#ifndef _FK_menu_h
#define _FK_menu_h

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

#include "stdlib.h"
#include "stdio.h"
#include <unistd.h>

#include <SDL/SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#define HAS_MENU_VOLUME
#define HAS_MENU_BRIGHTNESS
//#define HAS_MENU_SAVE
//#define HAS_MENU_LOAD
//#define HAS_MENU_ASPECT_RATIO
//#define HAS_MENU_USB
//#define HAS_MENU_THEME
//#define HAS_MENU_LAUNCHER
#define HAS_MENU_EXIT
//#define HAS_MENU_POWERDOWN
#define HAS_MENU_RO_RW

typedef enum{
    MENU_TYPE_VOLUME,
    MENU_TYPE_BRIGHTNESS,
    MENU_TYPE_SAVE,
    MENU_TYPE_LOAD,
    MENU_TYPE_ASPECT_RATIO,
    MENU_TYPE_USB,
    MENU_TYPE_THEME,
    MENU_TYPE_LAUNCHER,
    MENU_TYPE_EXIT,
    MENU_TYPE_POWERDOWN,
    MENU_TYPE_RO_RW,
    NB_MENU_TYPES,
} ENUM_MENU_TYPE;

typedef enum{
    MENU_RETURN_OK,
    MENU_RETURN_EXIT,
    MENU_RETURN_ERROR,
    NB_MENU_RETURN_CODES,
} ENUM_MENU_RETURN_CODES;

#ifdef HAS_MENU_ASPECT_RATIO
///------ Definition of the different aspect ratios
#define ASPECT_RATIOS \
    X(ASPECT_RATIOS_TYPE_MANUAL, "MANUAL ZOOM") \
    X(ASPECT_RATIOS_TYPE_STRECHED, "STRECHED") \
    X(ASPECT_RATIOS_TYPE_CROPPED, "CROPPED") \
    X(ASPECT_RATIOS_TYPE_SCALED, "SCALED") \
    X(NB_ASPECT_RATIOS_TYPES, "")

////------ Enumeration of the different aspect ratios ------
#undef X
#define X(a, b) a,
typedef enum {ASPECT_RATIOS} ENUM_ASPECT_RATIOS_TYPES;
#endif

////------ Defines to be shared -------
#ifdef HAS_MENU_VOLUME
#define STEP_CHANGE_VOLUME          10
#endif
#ifdef HAS_MENU_BRIGHTNESS
#define STEP_CHANGE_BRIGHTNESS      10
#endif

////------ Menu commands -------
#ifdef HAS_MENU_VOLUME
#define SHELL_CMD_VOLUME_GET                "volume get"
#define SHELL_CMD_VOLUME_SET                "volume set"
#endif
#ifdef HAS_MENU_BRIGHTNESS
#define SHELL_CMD_BRIGHTNESS_GET            "brightness get"
#define SHELL_CMD_BRIGHTNESS_SET            "brightness set"
#endif
#ifdef HAS_MENU_USB
#define SHELL_CMD_USB_DATA_CONNECTED        "is_usb_data_connected"
#define SHELL_CMD_USB_MOUNT                 "share start"
#define SHELL_CMD_USB_UNMOUNT               "share stop"
#define SHELL_CMD_USB_CHECK_IS_SHARING      "share is_sharing"
#endif
#ifdef HAS_MENU_POWERDOWN
#define SHELL_CMD_POWERDOWN                 "shutdown_funkey"
#define SHELL_CMD_SCHEDULE_POWERDOWN        "sched_shutdown"
#define SHELL_CMD_CANCEL_SCHED_POWERDOWN    "cancel_sched_powerdown"
#endif
#ifdef HAS_MENU_LAUNCHER
#define SHELL_CMD_SET_LAUNCHER_GMENU2X      "set_launcher gmenu2x"
#define SHELL_CMD_SET_LAUNCHER_RETROFE      "set_launcher retrofe"
#endif
#ifdef HAS_MENU_RO_RW
#define SHELL_CMD_RO                        "ro"
#define SHELL_CMD_RW                        "rw"
#endif

#define SHELL_CMD_KEYMAP_DEFAULT            "keymap default"
#define SHELL_CMD_KEYMAP_RESUME             "keymap resume"

#ifdef HAS_MENU_THEME
extern void FK_InitMenu(Configuration &c);
#else
extern void FK_InitMenu(void);
#endif
extern void FK_EndMenu(void);
extern int FK_RunMenu(SDL_Surface *screen);
extern void FK_StopMenu(void);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif /* _FK_menu_h */
