# yllm 移植说明（app/yllm）

上游 yllm 仓库整树移植进 yiyiya OS；构建走 ymake（`app/yllm/ya.py`），
上游 `Makefile` 保留仅作参考（不参与构建）。

## 构建

- 根 `ya.py` 的 apps 列表已加 `'yllm'`，正常全量构建即产出 `yllm`。
- 目标 = 上游标量版：`inference/**`（含 `device_cpu.c`）+ `main.c` +
  `serve/{rank,server,router,router_http,supervisor,hub,status,ctl,sync}.c`
  （与上游 Makefile 的 SRC/OBJ 一致；`serve/dist_worker.c` 是独立小工具，
  不编进主程序）。
- 不开 OpenMP / CUDA / Vulkan（本平台没有）：代码里全是 `#ifdef` 守卫，
  自动走标量、单线程路径。
- 架构相关代码（AVX2 / F16C / NEON / FP16）都有守卫，armv7-a 走标量 C。

## 为适配本 OS 的改动（相对上游）

1. `inference/include/llf.h`：`WMap` 增加 `heapbuf` 字段（退化映射标记）。
2. `inference/core/platform.c`：
   - `wmap_open`：本内核**不支持文件映射 mmap(fd)**（`sys_mmap2` 对 fd>0
     直接返回失败）⇒ mmap 失败时退化为"整个文件读进堆"；
   - `wmap_close`：`heapbuf` 非空则 `free`，否则 `munmap`。
   - 影响：上游的层流式加载/释放（`ws_release` 的 munmap+MAP_FIXED、
     `madvise`、`mincore` 驻留查询）在本平台全是 no-op ⇒
     **模型全量常驻内存**，128MB 机器建议用 ≤49MB 的小模型
     （上游 README：最小 49M 内存）。

## 已知限制

- 无 OpenMP ⇒ 单线程推理，速度明显慢于 PC（GEMV 是主要耗时）。
- `serve`（HTTP/分布式）需要内核 net 模块；miyoo 当前构建把 net 裁掉了，
  所以只有 `convert / check / gen / chat` 这些本地子命令可用。
- 模型放 SD 卡（GGUF 用 `yllm convert` 转成 llf，或直接跑支持 GGUF 的路径）。

## 常用命令

```sh
yllm convert --gguf <model.gguf> --out <model.llf>   # 转换（首次）
yllm check <model.llf>                               # 校验
yllm chat <model.llf>                                # 聊天
yllm gen <model.llf> -p "你好" -n 32                 # 单次生成
```
（具体参数以 `yllm --help` / 上游 `docs/serve-cli.md` 为准。）
