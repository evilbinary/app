/* weston-kiosk：默认 shell=kiosk、后端=fbdev 的固定组合变体。
 *
 * 前端实现只有 eggs/weston/frontend/main.c 一份，这里靠宏注入默认值（见 main.c
 * 顶部的 WESTON_DEFAULT_SHELL / WESTON_DEFAULT_BACKEND）。之所以给每个变体单独
 * 一个源文件，是因为 ymake 的 obj 路径只按源文件路径生成、不区分 target ——
 * 多个 target 编同一个 main.c 会共用同一份 main.o，宏定义互相覆盖，最后只剩
 * 最后一个编的变体是对的。
 */
#define WESTON_DEFAULT_SHELL "kiosk"
#define WESTON_DEFAULT_BACKEND "fbdev-backend.so"
#include "../../../eggs/weston/frontend/main.c"
