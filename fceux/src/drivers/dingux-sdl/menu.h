#ifndef _MENU_H_
#define _MENU_H_


typedef enum{
    MENU_TYPE_VOLUME,
    MENU_TYPE_BRIGHTNESS,
    MENU_TYPE_SAVE,
    MENU_TYPE_LOAD,
    MENU_TYPE_ASPECT_RATIO,
    MENU_TYPE_EXIT,
    MENU_TYPE_POWERDOWN,
    NB_MENU_TYPES,
} ENUM_MENU_TYPE;


///------ Definition of the different resume options
#define RESUME_OPTIONS \
    X(RESUME_YES, "RESUME GAME") \
    X(RESUME_NO, "NEW GAME") \
    X(NB_RESUME_OPTIONS, "") 

////------ Enumeration of the different resume options ------
#undef X
#define X(a, b) a,
typedef enum {RESUME_OPTIONS} ENUM_RESUME_OPTIONS;

////------ Defines to be shared -------
#define STEP_CHANGE_VOLUME          10
#define STEP_CHANGE_BRIGHTNESS      10
#define NOTIF_SECONDS_DISP			2

////------ Menu commands -------
#define SHELL_CMD_VOLUME_GET                "volume get"
#define SHELL_CMD_VOLUME_SET                "volume set"
#define SHELL_CMD_BRIGHTNESS_GET            "brightness get"
#define SHELL_CMD_BRIGHTNESS_SET            "brightness set"
#define SHELL_CMD_NOTIF_SET                 "notif set"
#define SHELL_CMD_AUDIO_AMP_ON              "audio_amp on"
#define SHELL_CMD_AUDIO_AMP_OFF             "audio_amp off"
#define SHELL_CMD_POWERDOWN                 "powerdown"
#define SHELL_CMD_POWERDOWN_HANDLE          "powerdown handle"
#define SHELL_CMD_INSTANT_PLAY              "instant_play"
#define SHELL_CMD_KEYMAP_DEFAULT            "keymap default"
#define SHELL_CMD_KEYMAP_RESUME             "keymap resume"

////------ Global variables -------
extern int volume_percentage;
extern int brightness_percentage;
extern int stop_menu_loop;

extern struct SAVE_STATE mSaveState[10];
//extern s8 mSaveStateName[SAL_MAX_PATH];
//extern s8 mRomName[SAL_MAX_PATH];
//extern s8 mSystemDir[SAL_MAX_PATH];

extern char *mRomName;
extern char *mRomPath;
extern char *cfg_file_rom;
extern char *quick_save_file;

//####################################
//# Functions
//####################################
void init_menu_SDL();
void deinit_menu_SDL();
void init_menu_zones();
void init_menu_system_values();
void run_menu_loop();

int launch_resume_menu_loop();


#endif /* _MENU_H_ */
