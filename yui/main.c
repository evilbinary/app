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
 * 这里直接用 backend_render_fill_rect + backend_render_texture +
 * backend_render_present 画"标题 + 进度条 + 百分比"（backend_init() 之后
 * 渲染目标就是窗口，可以这样直接画 ✓），并在每个阶段打一条耗时，便于
 * 继续定位启动慢的真凶 ✓。
 * ------------------------------------------------------------------ */
static unsigned g_load_t0 = 0;
static DFont* g_load_font = NULL;       /* 标题用（28px） */
static DFont* g_load_font_small = NULL; /* 进度文字用（16px，小一点 ✓） */
/* YiYiYa: 启动画面标题 = 应用名（从入口 json 的 title 读；缺省显示"正在启动…"）。
 * 原来固定写死 "YiYiYa Console OS"，进任何应用都显示它，没有信息量 ✗。 */
static char g_app_title[128] = "";
/* 带参数启动（ymain <app.json>）= 应用模式：圆形 loading；
 * 不带参数（默认 console-os）= 原进度条样式。 */
static int g_use_spinner = 0;

/* 启动阶段拿字体（和 console 的 assets 约定一致；失败就只有进度条，不崩 ✓） */
static void loading_font_ensure(void) {
    /* 运行时字体在 /font（console 的 app.json 是 "assets":"/font" ✓，
       源文件在 app/resource/font/ ✓）；后两个给本机/开发用 ✓ */
    static const char* cands[] = {
        "/font/Roboto-Regular.ttf",
        "app/resource/font/Roboto-Regular.ttf",
        "font/Roboto-Regular.ttf",
        NULL
    };
    int i;
    if (g_load_font == NULL) {
        for (i = 0; cands[i] != NULL && g_load_font == NULL; i++) {
            g_load_font = backend_load_font_with_weight((char*)cands[i], 28, "normal");
        }
    }
    if (g_load_font_small == NULL) {
        for (i = 0; cands[i] != NULL && g_load_font_small == NULL; i++) {
            g_load_font_small = backend_load_font_with_weight((char*)cands[i], 16, "normal");
        }
    }
}

/* 居中画一行文字；cy = 文字底边所在的 y（都用【逻辑坐标】✓） */
static void loading_text(DFont* font, const char* s, Color color, int cy) {
    Texture* t;
    int tw = 0, th = 0, w = 0, h = 0;
    float d;
    Rect dst;

    if (s == NULL || font == NULL) return;
    t = backend_render_texture(font, s, color);
    if (t == NULL) return;
    backend_query_texture(t, NULL, NULL, &tw, &th);
    /* 字体是按 size*yui_density（物理像素）栅格的，画的时候要换算回逻辑像素 ✓，
       否则会被当成逻辑尺寸使用 ⇒ 再放大 density 倍 + 线性滤波 ⇒ 发虚 ✗ */
    d = backend_get_density();
    if (d < 0.5f) d = 1.0f;
    tw = (int)(tw / d + 0.5f);
    th = (int)(th / d + 0.5f);
    backend_get_windowsize(&w, &h);
    if (w <= 0) w = 640;
    if (h <= 0) h = 480;
    dst.x = (w - tw) / 2;
    dst.y = cy - th;
    dst.w = tw;
    dst.h = th;
    backend_render_text_copy(t, NULL, &dst);
    backend_render_text_destroy(t);
}

static void loading_frame(int permille, int phase) {
    char pct[16];
    int w = 0, h = 0;
    Rect bg, track, fill;

    (void)phase;

    {   /* 只打一次：确认密度/窗口尺寸，用来判断"高清"有没有生效 ✓ */
        static int s_dbg = 0;
        if (!s_dbg) {
            int dw = 0, dh = 0;
            s_dbg = 1;
            backend_get_windowsize(&dw, &dh);
            printf("YUI: loading density=%.2f win=%dx%d\n",
                   backend_get_density(), dw, dh);
        }
    }

    backend_get_windowsize(&w, &h);
    if (w <= 0) w = 640;
    if (h <= 0) h = 480;

    bg.x = 0; bg.y = 0; bg.w = w; bg.h = h;
    backend_render_fill_rect(&bg, (Color){18, 18, 22, 255});

    if (g_use_spinner) {
    /* YiYiYa: 标题 = "正在启动 <应用名>"（不写死 "YiYiYa Console OS"） */
    {
        char title[192];
        if (g_app_title[0]) {
            snprintf(title, sizeof(title), "正在启动 %s", g_app_title);
        } else {
            snprintf(title, sizeof(title), "正在启动…");
        }
        loading_text(g_load_font, title, (Color){235, 235, 245, 255}, h / 2 - 60);
    }

    /* YiYiYa: 圆形 loading —— 底环 + 旋转弧（backend_render_arc：
     * 角度单位为度、0° 在顶部、顺时针；用 phase 驱动旋转做 spinner ✓）。
     * 原来是横向进度条 + 扫光，用户要"圆形 loading"，这里整体替换。 */
    {
        int cx = w / 2;
        int cy = h / 2;
        int radius = 22;
        float a0 = (float)((phase * 12) % 360);      /* 每帧转 12°（≈30ms/帧） */
        float a1 = a0 + 100.0f;                      /* 弧长 100° */
        backend_render_arc(cx, cy, radius, 0.0f, 360.0f,
                           (Color){40, 42, 52, 255}, 5);   /* 底环 */
        backend_render_arc(cx, cy, radius, a0, a1,
                           (Color){56, 150, 245, 255}, 5); /* 旋转弧 */
    }

    } else {
    /* 标题：进度条上方 */
    loading_text(g_load_font, "YiYiYa Console OS", (Color){235, 235, 245, 255}, h / 2 - 28);

    /* 进度条：细一点（8px）、**直角**（圆角在"帧外直绘"里会变形 ✗），
       渐变换成"分段填色"（观感接近，但只用矩形、更稳 ✓） */
    track.x = w / 8;
    track.y = h / 2;
    track.w = w * 3 / 4;
    track.h = 8;
    backend_render_fill_rect(&track, (Color){40, 42, 52, 255});

    if (permille < 0) permille = 0;
    if (permille > 1000) permille = 1000;
    fill = track;
    fill.w = track.w * permille / 1000;
    if (fill.w < 2) fill.w = 2;
    {
        /* 已加载部分：分段填色做横向渐变 + 呼吸（亮度随 phase 轻微起伏 ✓） */
        int k = phase % 12;
        int lum = (k < 6 ? k : 11 - k);
        int steps = 8, i;
        for (i = 0; i < steps; i++) {
            Rect seg;
            Color c;
            int x0 = fill.w * i / steps;
            int x1 = fill.w * (i + 1) / steps;
            c.r = (unsigned char)(55 + 12 * i);
            c.g = (unsigned char)(125 + lum * 6 + 10 * i);
            c.b = 245;
            c.a = 255;
            seg.x = fill.x + x0;
            seg.y = fill.y;
            seg.w = x1 - x0;
            seg.h = fill.h;
            if (seg.w <= 0) continue;
            backend_render_fill_rect(&seg, c);
        }
    }
    {   /* 扫光：一条半透明白带沿已加载部分来回滑 ✓ */
        int sw = track.w / 7;
        int span, p, pos;
        Rect sh;
        if (sw < 8) sw = 8;
        span = fill.w - sw;
        if (span < 1) span = 1;
        p = phase % 24;
        pos = (p < 12 ? p : 23 - p) * span / 12;
        sh.x = fill.x + pos;
        sh.y = fill.y;
        sh.w = sw;
        sh.h = fill.h;
        backend_render_fill_rect(&sh, (Color){255, 255, 255, 70});
    }

    }

    /* 进度百分比：进度条下方（小字号 16px ✓） */
    snprintf(pct, sizeof(pct), "%d%%", permille / 10);
    loading_text(g_load_font_small, pct, (Color){170, 190, 220, 255},
                 g_use_spinner ? (h / 2 + 44) : (h / 2 + 30));

    backend_render_present();
}

/* 连画几帧产生"动画"（每帧 30ms） */
static void loading_pulse(int permille, int frames) {
    int i;
    for (i = 0; i < frames; i++) {
        /* 第一帧先出画面（进度条立刻可见 ✓），第二帧再去花时间取字体 ✓
           —— 首次打开 TTF（大文件）要好几秒 ✗，别让它挡住"有反应"的感觉 ✓ */
        if (i == 1) {
            loading_font_ensure();
        }
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
    if (argc > 1) {
        g_use_spinner = 1;   /* 带参数 = 启动指定应用（圆形 loading） */
    }
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

    /* YiYiYa: 启动画面显示应用名 —— 优先 json 的 title，其次目录名 */
    {
        cJSON* title_item = cJSON_GetObjectItem(root_json, "title");
        if (title_item && title_item->valuestring && title_item->valuestring[0]) {
            strncpy(g_app_title, title_item->valuestring, sizeof(g_app_title) - 1);
            g_app_title[sizeof(g_app_title) - 1] = '\0';
        } else {
            const char* a = json_path;
            const char* b = NULL;
            const char* c;
            for (c = json_path; *c; c++) {
                if (*c == '/') { b = a; a = c + 1; }
            }
            (void)b;
            if (a && a[0] && strcmp(a, "app.json") != 0) {
                strncpy(g_app_title, a, sizeof(g_app_title) - 1);
                g_app_title[sizeof(g_app_title) - 1] = '\0';
            } else {
                /* app/xxx/app.json → 取上一级目录名 */
                const char* dir_end = NULL;
                const char* dir_start = json_path;
                const char* q;
                for (q = json_path; *q; q++) {
                    if (*q == '/') { dir_end = q; }
                }
                if (dir_end) {
                    const char* p2 = dir_end;
                    while (p2 > json_path && *(p2 - 1) != '/') p2--;
                    {
                        int len = (int)(dir_end - p2);
                        if (len > 0 && len < (int)sizeof(g_app_title)) {
                            memcpy(g_app_title, p2, (size_t)len);
                            g_app_title[len] = '\0';
                        }
                    }
                }
            }
        }
    }    
    /* 这一段之前是 JS 引擎初始化 + 解析 app.json（拆开看 ✓） */
    loading_mark("json", 150, 1);
    Layer* ui_root = layer_create_from_json(root_json,NULL);
    if(ui_root==NULL){
        fprintf(stderr, "Failed to create UI from JSON: %s\n", json_path);
        return -1;
    }

    // 设置 UI 根图层（须在加载 JS 之前，以便事件绑定能找到图层）
    js_module_init_layer(ui_root);

    /* 建树完成（这一步只做图层树 ✓） */
    loading_mark("ui_tree", 250, 1);

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