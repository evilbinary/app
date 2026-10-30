/* weston-headless：默认 shell=kiosk、后端=headless 的固定组合变体。
 * 无显示输出，只把画面合成到内存，用来验证合成逻辑或给远端客户端用。
 * 单独一个源文件的原因见 kiosk.c 的注释（obj 共用会串宏）。
 */
#define WESTON_DEFAULT_SHELL "kiosk"
#define WESTON_DEFAULT_BACKEND "headless-backend.so"
#include "../../../eggs/weston/frontend/main.c"
