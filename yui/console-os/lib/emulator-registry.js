/**
 * YUI Game OS · 模拟器注册表
 *
 * 扫描 emulators/ 目录，每个子目录即一个可加载的模拟器：
 *
 *   emulators/<id>/<id>.json    元数据（title / icon / core / exts / roms ...）
 *
 * 约定字段（顶层，全部可选，除 id 外均有默认值）：
 *   id       唯一标识（缺省取目录名）
 *   title    显示名称
 *   short    简称，用于列表标签（如 FC / GBA）
 *   icon     桌面图标（emoji）
 *   subtitle 副标题（英文名 / 厂商）
 *   core     模拟核心名
 *   systems  支持机型数组
 *   exts     支持 ROM 后缀数组
 *   romDir   ROM 目录（相对本应用目录）
 *   accent   强调色
 *   order    排序权重（越小越靠前）
 *   bio      一句话简介
 *   roms     内置演示游戏（数组），与 romDir 下真实文件合并显示
 *
 * 新增模拟器无需改代码：放入目录即可在桌面出现。
 */

var EmulatorRegistry = {
    root: "emulators",
    romRoot: "roms",
    emulators: [],
    byId: {},
    scanned: 0,
    source: "none",
    romCache: {},
    romTotal: 0,

    /* ==================== 初始化 ==================== */

    init: function() {
        this.emulators = [];
        this.byId = {};
        this.romCache = {};
        this.romTotal = 0;

        this.scanned = this._scanRoot();
        if (this.scanned === 0) {
            this.source = "builtin";
            var fallback = this._builtin();
            for (var i = 0; i < fallback.length; i++) {
                this._register(fallback[i]);
            }
        } else {
            this.source = "scan";
        }

        this.emulators.sort(function(a, b) {
            return (a.order || 100) - (b.order || 100);
        });

        for (var j = 0; j < this.emulators.length; j++) {
            this.byId[this.emulators[j].id] = this.emulators[j];
            this.romTotal += this.listRoms(this.emulators[j].id).length;
        }

        YUI.log("[emu] source=" + this.source + " scanned=" + this.scanned
            + " total=" + this.emulators.length + " roms=" + this.romTotal);
        return this.emulators;
    },

    _readFileSafe: function(filePath) {
        if (typeof YUI.readFile !== "function") return null;
        try {
            return YUI.readFile(filePath);
        } catch (e) {
            return null;
        }
    },

    _scanRoot: function() {
        if (typeof YUI.listDir !== "function") {
            YUI.log("[emu] listDir unavailable, fallback to builtin list");
            return 0;
        }
        var entries = YUI.listDir(this.root);
        if (!entries) {
            YUI.log("[emu] cannot list " + this.root);
            return 0;
        }
        var count = 0;
        for (var i = 0; i < entries.length; i++) {
            var entry = entries[i];
            if (!entry || !entry.isDir) continue;
            var name = entry.name;
            if (!name || name.charAt(0) === ".") continue;
            var emu = this._loadDir(name);
            if (emu && this._register(emu)) count++;
        }
        return count;
    },

    _loadDir: function(dirName) {
        var jsonPath = this.root + "/" + dirName + "/" + dirName + ".json";
        var raw = this._readFileSafe(jsonPath);
        if (!raw) {
            YUI.log("[emu] skip " + dirName + " (no " + jsonPath + ")");
            return null;
        }
        var meta;
        try {
            meta = JSON.parse(raw);
        } catch (e) {
            YUI.log("[emu] bad json: " + jsonPath);
            return null;
        }
        return this._normalize(meta, dirName, this.root + "/" + dirName, jsonPath);
    },

    _normalize: function(meta, dirName, base, jsonPath) {
        if (!meta) return null;
        var id = meta.id || dirName;
        if (!id) return null;
        return {
            id: id,
            title: meta.title || dirName,
            short: meta.short || String(id).toUpperCase(),
            icon: meta.icon || "🎮",
            subtitle: meta.subtitle || "",
            core: meta.core || id,
            /* 外部进程可执行文件/命令（external 模式优先用它，缺省回退 core） */
            exec: meta.exec || "",
            systems: meta.systems || [],
            exts: meta.exts || [],
            romDir: meta.romDir || (this.romRoot + "/" + id),
            /* 多个扫描目录（可选）：如播放器 ["roms/video","roms"]；
             * 缺省只用 romDir。 */
            romDirs: meta.romDirs || [],
            accent: meta.accent || "#38BDF8",
            order: (meta.order !== undefined && meta.order !== null) ? meta.order : 100,
            bio: meta.bio || "",
            roms: meta.roms || [],
            dir: base,
            json: jsonPath
        };
    },

    _register: function(emu) {
        if (!emu || !emu.id) return false;
        if (this.byId[emu.id]) return false;
        this.byId[emu.id] = emu;
        this.emulators.push(emu);
        return true;
    },

    /* 扫描失败时的最小兜底集合（保证桌面始终可用） */
    _builtin: function() {
        var defs = [
            { id: "nes", title: "红白机", short: "FC", icon: "🎮", core: "fceumm", exts: ["nes"], order: 10 },
            { id: "gba", title: "GBA", short: "GBA", icon: "🐉", core: "mgba", exts: ["gba"], order: 40 },
            { id: "md", title: "世嘉 MD", short: "MD", icon: "🌀", core: "genesis_plus_gx", exts: ["md", "bin"], order: 50 },
            { id: "arcade", title: "街机", short: "ARC", icon: "🕹", core: "mame2003", exts: ["zip"], order: 70 },
            { id: "ps1", title: "PlayStation", short: "PS1", icon: "💿", core: "pcsx_rearmed", exts: ["bin", "cue"], order: 80 }
        ];
        var list = [];
        for (var i = 0; i < defs.length; i++) {
            list.push(this._normalize(defs[i], defs[i].id, this.root + "/" + defs[i].id,
                this.root + "/" + defs[i].id + "/" + defs[i].id + ".json"));
        }
        return list;
    },

    /* ==================== 查询 ==================== */

    getAll: function() {
        return this.emulators;
    },

    findById: function(id) {
        return this.byId[id] || null;
    },

    count: function() {
        return this.emulators.length;
    },

    totalRoms: function() {
        return this.romTotal;
    },

    extsText: function(emu) {
        if (!emu || !emu.exts || emu.exts.length === 0) return "—";
        return emu.exts.join(" / ");
    },

    systemsText: function(emu) {
        if (!emu || !emu.systems || emu.systems.length === 0) return emu ? emu.short : "—";
        return emu.systems.join(" · ");
    },

    coreText: function(emu) {
        return emu && emu.core ? emu.core : "—";
    },

    /* ==================== ROM 列表 ==================== */

    /* 扫描单个目录（多目录由 listRoms 遍历 romDirs） */
    _scanRomDir: function(dir) {
        var out = [];
        if (!dir || typeof YUI.listDir !== "function") return out;
        var entries = YUI.listDir(dir);
        if (!entries) return out;
        for (var i = 0; i < entries.length; i++) {
            var e = entries[i];
            if (!e || e.isDir) continue;
            var name = e.name;
            if (!name || name.charAt(0) === ".") continue;
            var lower = name.toLowerCase();
            if (lower === "readme.md" || lower === "readme.txt" || lower === ".gitkeep") continue;
            out.push(name);
        }
        out.sort();
        return out;
    },

    _extOf: function(fileName) {
        var idx = fileName.lastIndexOf(".");
        if (idx <= 0) return "";
        return fileName.substring(idx + 1).toLowerCase();
    },

    _baseOf: function(fileName) {
        var idx = fileName.lastIndexOf(".");
        if (idx <= 0) return fileName;
        return fileName.substring(0, idx);
    },

    /* 真实文件 + 内置演示 ROM 合并，返回渲染用条目 */
    listRoms: function(id) {
        if (this.romCache[id]) return this.romCache[id];

        var emu = this.byId[id];
        if (!emu) return [];

        var items = [];
        var seen = {};

        /* YiYiYa: 支持多个扫描目录（romDirs 数组；缺省 = [romDir]）——
         * 例如播放器 romDirs: ["roms/video", "roms"]，默认根目录也能扫到。
         * 每个目录内部再按"同名 .png"配对缩略图（见 button_component.c：
         * icon 含 '.'/'/' 会走 backend_load_texture 当图片渲染）。 */
        var dirs = (emu.romDirs && emu.romDirs.length) ? emu.romDirs : [emu.romDir];
        for (var d = 0; d < dirs.length; d++) {
            var dir = dirs[d];
            var files = this._scanRomDir(dir);
            var covers = {};
            for (var c = 0; c < files.length; c++) {
                if (this._extOf(files[c]) === "png") {
                    covers[this._baseOf(files[c]).toLowerCase()] = files[c];
                }
            }
            for (var i = 0; i < files.length; i++) {
                var base = this._baseOf(files[i]);
                var ext = this._extOf(files[i]);
                if (ext === "png") continue;   /* png 只当图片，不当游戏 */
                if (seen[base]) continue;      /* 多目录去重：先到先得 */
                var cover = covers[base.toLowerCase()];
                items.push({
                    title: base,
                    icon: cover ? (dir + "/" + cover) : emu.icon,
                    file: true,
                    path: dir + "/" + files[i],
                    line: base + "  ·  " + (ext ? ext.toUpperCase() : "ROM") + "  ·  本地文件"
                });
                seen[base] = true;
            }
        }

        var demo = emu.roms || [];
        for (var j = 0; j < demo.length; j++) {
            var r = demo[j];
            if (!r || !r.title) continue;
            if (seen[r.title]) continue;
            var meta = [];
            if (r.region) meta.push(r.region);
            if (r.year) meta.push(String(r.year));
            if (r.size) meta.push(r.size);
            items.push({
                title: r.title,
                icon: emu.icon,
                file: false,
                line: r.title + "  ·  " + (meta.length > 0 ? meta.join(" · ") : emu.short)
            });
        }

        this.romCache[id] = items;
        return items;
    },

    romCount: function(id) {
        return this.listRoms(id).length;
    },

    /* 全部模拟器的游戏汇总（游戏库页使用） */
    allRoms: function() {
        var out = [];
        for (var i = 0; i < this.emulators.length; i++) {
            var emu = this.emulators[i];
            var roms = this.listRoms(emu.id);
            for (var j = 0; j < roms.length; j++) {
                out.push({
                    emu: emu.id,
                    emuTitle: emu.title,
                    icon: roms[j].icon,
                    title: roms[j].title,
                    line: roms[j].line + "  ·  " + emu.title
                });
            }
        }
        return out;
    },

    invalidate: function() {
        this.romCache = {};
        this.romTotal = 0;
        for (var i = 0; i < this.emulators.length; i++) {
            this.romTotal += this.listRoms(this.emulators[i].id).length;
        }
    }
};
