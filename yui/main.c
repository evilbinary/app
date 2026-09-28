#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#include <sys/syscall.h>

#include "cJSON.h"
#include "event.h"
#include "layer.h"
#include "render.h"
#include "layout.h"
#include "backend.h"
#include "util.h"
#include "popup_manager.h"
#include "../lib/jsmodule/js_module.h"
#include "yaml_cjson.h"
#include "log.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/* 与 syscall(519,...,20000) 对齐：内核 1000Hz 下 20000 tick ≈ 20s */
#define YUI_PERF_DURATION_SEC 60
// #define YUI_PERF_ENABLE 1


static int g_perf_dumped;
static struct timespec g_perf_t0;

static void yui_perf_dump_once(void) {
    struct timespec now;
    long sec;

    if (g_perf_dumped) {
        return;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return;
    }
    sec = (long)(now.tv_sec - g_perf_t0.tv_sec);
    if (now.tv_nsec < g_perf_t0.tv_nsec) {
        sec--;
    }
    if (sec < YUI_PERF_DURATION_SEC) {
        return;
    }
    g_perf_dumped = 1;
    printf("ymain: SYS_PERF_STOP (dump after %ds)\n", YUI_PERF_DURATION_SEC);
    syscall(520);
}

// 检查文件扩展名是否为 YAML
static bool is_yaml_file(const char* filename) {
    if (!filename) return false;
    
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    
    return (strcmp(ext, ".yaml") == 0 || strcmp(ext, ".yml") == 0);
}


// 解析文件（支持 JSON 和 YAML）
cJSON* parse_yaml_json_file(const char* file_path) {
    if (!file_path) return NULL;
    
    // 检查是否为 YAML 文件
    if (is_yaml_file(file_path)) {
        printf("DEBUG: Detected YAML file: %s\n", file_path);
        
        char* error = NULL;
        cJSON* json = yaml_file2cjson(file_path, &error);
        
        if (!json) {
            fprintf(stderr, "Failed to parse YAML file: %s\n", file_path);
            if (error) {
                fprintf(stderr, "Error: %s\n", error);
                free(error);
            }
            return NULL;
        }
        
        printf("DEBUG: Successfully converted YAML to JSON\n");
        return json;
    } else {
        // 默认按 JSON 解析
        printf("DEBUG: Parsing as JSON file: %s\n", file_path);
        return parse_json((char*)file_path);
    }
}

// ====================== 主入口 ======================
/* ------------------------------------------------------------------
 * YiYiYa: 启动过程可视化
 *
 * main() 里 backend_init() 之后的初始化全是阻塞的（解析 JSON、建树、加载
 * 字体/贴图、解析执行 JS），期间一帧都不画 → 看起来"启动慢且毫无反应"✓。
 * 这里直接用 backend_render_fill_rect + backend_render_present 画
 * "进度条 + 来回滑动的小方块"（backend_init() 之后渲染目标就是窗口，
 * 可以这样直接画 ✓），并在每个阶段打一条耗时，便于继续定位真凶 ✓。
 * ------------------------------------------------------------------ */
static unsigned g_load_t0 = 0;

static void loading_frame(int permille, int phase) {
    int w = 0, h = 0;
    backend_get_windowsize(&w, &h);
    if (w <= 0) w = 640;
    if (h <= 0) h = 480;
    {
        Rect bg = {0, 0, w, h};
        backend_render_fill_rect(&bg, (Color){18, 18, 22, 255});
    }
    {
        Rect track = {w / 8, h / 2, w * 3 / 4, 16};
        backend_render_fill_rect(&track, (Color){58, 58, 68, 255});
        if (permille < 0) permille = 0;
        if (permille > 1000) permille = 1000;
        Rect fill = {track.x, track.y, track.w * permille / 1000, track.h};
        if (fill.w < 2) fill.w = 2;
        backend_render_fill_rect(&fill, (Color){90, 170, 255, 255});
    }
    {   /* 来回滑动的小方块，看起来"在动" */
        int span = w / 4, box = 12;
        int p = phase % 20;
        int dx = (p < 10 ? p : 19 - p) * span / 10;
        Rect dot = {w * 3 / 8 + dx, h / 2 + 36, box, box};
        backend_render_fill_rect(&dot, (Color){255, 200, 90, 255});
    }
    backend_render_present();
}

/* 连画几帧产生"动画"（每帧 30ms） */
static void loading_pulse(int permille, int frames) {
    int i;
    for (i = 0; i < frames; i++) {
        loading_frame(permille, i + frames);
        backend_delay(30);
    }
}

/* 打一条阶段耗时并刷新一帧；permille = 进度千分比，frames = 多画几帧做动画 */
static void loading_mark(const char* what, int permille, int frames) {
    unsigned now = backend_get_ticks();
    if (g_load_t0 != 0) {
        printf("YUI: startup %-16s %5ums\n", what, (unsigned)(now - g_load_t0));
    }
    g_load_t0 = now;
    loading_pulse(permille, frames);
}

int main(int argc, char* argv[]) {

    backend_init();
    g_load_t0 = backend_get_ticks();
    popup_manager_init();

    /* 先给一帧，避免"启动后黑屏、毫无反应" */
    loading_mark("init", 40, 3);

    /* 内核采样；到期只停采。满 20s 由 ymain 再调 520 dump（勿在 IRQ 里 dump） */
    clock_gettime(CLOCK_MONOTONIC, &g_perf_t0);
    g_perf_dumped = 0;
#ifdef YUI_PERF_ENABLE
    backend_register_update_callback(yui_perf_dump_once);
    syscall(519, 1000, 60000);
#endif

    // 初始化 JS 引擎
    if (js_module_init() != 0) {
        fprintf(stderr, "Failed to initialize JavaScript engine\n");
        return -1;
    }


    char* json_path="app/console-os/app.json"; //"app/watch-os/app.json";
    // 加载UI描述文件
    if(argc>1){
        json_path=argv[1];
    }

    printf("DEBUG: Loading file from path: %s\n", json_path);


    cJSON* root_json = parse_yaml_json_file(json_path);
    if (!root_json) {
        fprintf(stderr, "Failed to parse file: %s\n", json_path);
        return -1;
    }    
    Layer* ui_root = layer_create_from_json(root_json,NULL);
    if(ui_root==NULL){
        fprintf(stderr, "Failed to create UI from JSON: %s\n", json_path);
        return -1;
    }

    // 设置 UI 根图层（须在加载 JS 之前，以便事件绑定能找到图层）
    js_module_init_layer(ui_root);

    load_all_fonts(ui_root);
    loading_mark("fonts", 300, 3);

    // 先加载 source 中的 JS，再加载主 JSON，确保 onLoad 触发时脚本已就绪
    printf("加载并执行 JS 文件...\n");
    int count = 0;
    cJSON* source = cJSON_GetObjectItem(root_json, "source");
    if (source && cJSON_IsString(source)) {
        const char* source_path = source->valuestring;
        printf("DEBUG: Loading JS from source file: %s\n", source_path);
        cJSON* source_json = parse_yaml_json_file((char*)source_path);
        if (source_json) {
            count = js_module_load_from_json(source_json, source_path, 0);
            printf("DEBUG: Loaded %d JS file(s) from source JSON\n", count);
            cJSON_Delete(source_json);
        } else {
            printf("ERROR: Failed to load source JSON file: %s\n", source_path);
        }
    }
    if (count <= 0) {
        count = js_module_load_from_json(root_json, json_path, 0);
    } else {
        js_module_load_from_json(root_json, json_path, 1);
    }
    loading_mark("js", 600, 3);
    print_registered_events();

    // 如果根图层没有设置宽度和高度，则根据窗口大小设置
    if (ui_root->rect.w <= 0 || ui_root->rect.h <= 0) {
        int window_width, window_height;
        backend_get_windowsize(&window_width, &window_height);
        if (ui_root->rect.w <= 0) {
            ui_root->rect.w = window_width;
        }
        if (ui_root->rect.h <= 0) {
            ui_root->rect.h = window_height;
        }
    }else{
        backend_set_windowsize(ui_root->rect.w,ui_root->rect.h);
    }
    if(ui_root->text!=NULL){
        backend_set_window_title(ui_root->text);
    }

    // 设置窗口标题栏颜色（从 style 中读取）
    cJSON* root_style = cJSON_GetObjectItem(root_json, "style");
    if (root_style) {
        cJSON* title_bg = cJSON_GetObjectItem(root_style, "titleBarBgColor");
        cJSON* title_text = cJSON_GetObjectItem(root_style, "titleBarTextColor");
        if (title_bg && cJSON_IsString(title_bg)) {
            Color bg, text;
            parse_color(title_bg->valuestring, &bg);
            if (title_text && cJSON_IsString(title_text)) {
                parse_color(title_text->valuestring, &text);
            } else {
                text = (Color){255, 255, 255, 255};
            }
            backend_set_titlebar_color(bg, text);
        }
    }

    // 设置窗口图标
    cJSON* icon_path = cJSON_GetObjectItem(root_json, "icon");
    if (icon_path && cJSON_IsString(icon_path)) {
        backend_set_window_icon(icon_path->valuestring);
    }

    // 如果根图层没有设置宽度和高度，则根据窗口大小设置
    LOGD("playground", "ui_root %d,%d", ui_root->rect.w, ui_root->rect.h);


    cJSON_Delete(root_json);

    if(ui_root->font==NULL){
        ui_root->font=malloc(sizeof(Font));
        sprintf(ui_root->font->path,"%s","Roboto-Regular.ttf");
    }
    if(ui_root->assets==NULL){
        ui_root->assets=malloc(sizeof(Assets));
        sprintf(ui_root->assets->path,"%s","assets");
    }

    // 加载纹理资源
    // load_all_fonts 已经在 layer_create_from_json 之后调用过了

    load_textures(ui_root);
    loading_mark("textures", 850, 3);
    layout_layer(ui_root);

    loading_mark("ready", 1000, 2);
    backend_run(ui_root);

    // 清理资源
    js_module_cleanup();  // 清理 JS 引擎
    // destroy_layer(ui_root);  // 暂时注释掉以避免内存问题
    popup_manager_cleanup();

    #ifdef YUI_PERF_ENABLE
    if (!g_perf_dumped) {
        syscall(520);  // 提前退出时补一次 dump
    }
    #endif
    backend_quit();
    return 0;
}