/**
 * 应用列表 —— 扫描 app/ 下的 JSON 应用（app.json / <dir>.json / 子目录 *.json），
 * 选中后用 ymain 以外部进程启动（与外部模拟器同一模式，退出回本页）。
 *
 * 机制说明（参考 yui 上游 app/launcher）：
 *   - YUI.listDir("app") 扫目录；
 *   - 每个目录解析 app.json / <dir>.json / 子目录里的 *.json（含 source 间接）；
 *   - 启动 = spawn `ymain <json>`（ymain 的 main.c 支持 argv[1] 指定入口 json，
 *     包内相对路径由 js_module 按 json 所在目录自动解析）。
 */

var APPS_LIST = [];

/* 不当作"应用"的目录（框架/资源/自身） */
var APPS_SKIP_DIRS = {
    "assets": 1, "lib": 1, "js": 1, "tests": 1, "console-os": 1
};

function onAppsLoad() {
    YUI.log("[apps] onLoad");
    refreshAppsList();
}

function onAppsShow() {
    applyConsoleTheme();
    refreshAppsList();
}

function appsReadMeta(path, fallback) {
    var raw = null;
    try { raw = YUI.readFile(path); } catch (e) { return null; }
    if (!raw) return null;
    var json;
    try { json = JSON.parse(raw); } catch (err) { return null; }
    if (!json.type && !json.children && !json.js && !json.source) return null;
    var title = json.title || json.text || json.id || fallback;
    return {
        title: String(title),
        icon: json.icon || "📄",
        path: path,
        source: json.source || null
    };
}

function appsResolve(dir) {
    var base = "app/" + dir;
    var meta = appsReadMeta(base + "/app.json", dir);
    if (meta) {
        if (meta.source) {
            var m2 = appsReadMeta(meta.source, dir);
            if (m2) {
                if (!m2.icon || m2.icon === "📄") m2.icon = meta.icon;
                return m2;
            }
        }
        return meta;
    }
    meta = appsReadMeta(base + "/" + dir + ".json", dir);
    if (meta) return meta;
    var sub = YUI.listDir(base);
    var has_self = false;
    if (sub) {
        for (var i = 0; i < sub.length; i++) {
            if (sub[i].isDir) continue;
            var fn = sub[i].name;
            if (fn === dir) has_self = true;   /* native：与目录同名的可执行文件 */
            if (fn.indexOf(".json") < 0) continue;
            meta = appsReadMeta(base + "/" + fn, dir);
            if (meta) return meta;
        }
    }
    if (has_self) {
        /* YiYiYa: native 应用（doom/doom、doom2/doom2、gpsp/gpsp...）——
         * 没有 app.json，目录名即应用名，直接运行目录里的同名可执行文件 ✓ */
        return {
            title: dir,
            icon: "📦",
            path: base + "/" + dir,
            native: 1,
            source: null
        };
    }
    return null;
}

function refreshAppsList() {
    var list = [];
    var entries = YUI.listDir("app");
    if (entries) {
        for (var i = 0; i < entries.length; i++) {
            var e = entries[i];
            if (!e || !e.isDir) continue;
            var name = e.name;
            if (!name || name.charAt(0) === "." || name === "__pycache__") continue;
            if (APPS_SKIP_DIRS[name]) continue;
            var meta = appsResolve(name);
            if (meta) list.push(meta);
        }
    }
    list.sort(function(a, b) { return a.title < b.title ? -1 : (a.title > b.title ? 1 : 0); });
    APPS_LIST = list;

    var data = [];
    for (var k = 0; k < list.length; k++) {
        data.push({
            icon: list[k].icon,
            line: list[k].title + "  ·  " + list[k].path,
            path: list[k].path,
            native: list[k].native || 0
        });
    }
    YUI.update({ target: "apps_list", change: { data: data } });
    YUI.setText("apps_sub", "共 " + list.length + " 个应用 · 扫描 app/");
    setConsoleHint("Ⓐ 启动应用    Ⓑ 返回桌面    右滑返回");
}

function onAppsRescan() {
    refreshAppsList();
    setConsoleHint("已重新扫描 app/ · " + APPS_LIST.length + " 个应用");
}

function onAppsItemSelect(layerId) {
    var raw = YUI.getText("apps_list");
    if (!raw) return;
    var item;
    try { item = JSON.parse(raw); } catch (e) { return; }

    var path = item ? item.path : null;
    if (!path && item && item.line) {
        var idx = item.line.indexOf("  ·  ");
        if (idx >= 0) path = item.line.substring(idx + 5);
    }
    if (!path) return;

    if (typeof YUI.spawn !== "function") {
        setConsoleHint("当前后端不支持启动外部应用");
        return;
    }
    /* json 应用 → ymain <json>；native 应用 → 直接运行同名可执行文件。
     * native 判定：优先 item.native；再用路径后缀兜底（.json 之外的都按
     * native 处理，防止 data 字段在 getText 往返中丢失 ✗ —— 实测踩过）。 */
    var isNative = item && item.native;
    if (!isNative && path) {
        if (path.toLowerCase().indexOf(".json") < 0) isNative = 1;
    }

    var cmd;
    var cwd = null;
    if (isNative) {
        /* native：在应用自身目录里运行（同目录资源如 doom 的 wad 才找得到），
         * 命令用 ./<名字>（相对新 cwd）✓ */
        var slash = path.lastIndexOf("/");
        cwd = (slash > 0) ? path.substring(0, slash) : null;
        cmd = (slash >= 0) ? ("./" + path.substring(slash + 1)) : path;
    } else {
        cmd = "ymain " + consoleShellQuote(path);
    }

    YUI.log("[apps] spawn " + cmd + (cwd ? (" (cwd=" + cwd + ")") : ""));
    var rc = YUI.spawn(cmd, cwd);
    YUI.log("[apps] spawn rc=" + rc + " (" + path + ")");
    setConsoleHint("应用已退出 · " + path);
    refreshAppsList();
}
