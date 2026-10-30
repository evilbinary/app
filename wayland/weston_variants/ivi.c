/* weston-ivi：默认 shell=ivi、后端=fbdev 的固定组合变体（车机式布局）。
 * 需要 ivi-shell.so 与 ivi-shell-user-interface 客户端，都在图形套里
 * （--wayland-desktop）。单独一个源文件的原因见 kiosk.c 的注释。
 */
#define WESTON_DEFAULT_SHELL "ivi"
#define WESTON_DEFAULT_BACKEND "fbdev-backend.so"
#include "../../../eggs/weston/frontend/main.c"
