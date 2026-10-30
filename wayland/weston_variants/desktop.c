/* weston-desktop：默认 shell=desktop、后端=fbdev 的固定组合变体（多窗口桌面）。
 * 需要 desktop-shell.so 与 weston-desktop-shell 客户端，都在图形套里
 * （--wayland-desktop）。单独一个源文件的原因见 kiosk.c 的注释。
 */
#define WESTON_DEFAULT_SHELL "desktop"
#define WESTON_DEFAULT_BACKEND "fbdev-backend.so"
#include "../../../eggs/weston/frontend/main.c"
