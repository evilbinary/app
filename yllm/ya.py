# coding:utf-8
# *******************************************************************
# * yllm：轻量级大语言模型推理工具（移植自上游 yllm 仓库，见 README.md）
# *   目标与上游 Makefile 的标量版一致：
# *     SRC(inference/**) + main.c + serve/{rank,server,router,router_http,
# *     supervisor,hub,status,ctl,sync}.c
# *   本平台没有 OpenMP / CUDA / Vulkan：
# *     - 不定义 _OPENMP（代码里 #ifdef 守卫，自动单线程）；
# *     - 不编 device_cuda/vulkan（device_cpu.c 是默认后端）。
# *******************************************************************
target("yllm")
set_type("cli")

add_deps(get_config('default_libc'))

add_includedirs(
    '.',
    'inference/include',
    'inference/cuda',
    'inference/vulkan',
    'inference/vision',
)

# 与上游 Makefile 的 CFLAGS_BASE 对齐（去掉 -fopenmp）：
#   -O3 -std=c99 -D_DEFAULT_SOURCE -DYLLM_BATCH_PREFILL=1 -DYLLM_SESS_DEBUG=0
# _GNU_SOURCE 由 platform.c 自己定义，其它文件也一并给（musl 下无害）。
add_cflags(
    '-O3 -std=c99 -Wall -Wextra -D_GNU_SOURCE -D_DEFAULT_SOURCE '
    '-DYLLM_BATCH_PREFILL=1 -DYLLM_SESS_DEBUG=0'
)

add_files(
    'main.c',
    # ---- inference 核心（上游 Makefile 的 SRC）----
    'inference/core/platform.c',
    'inference/core/log.c',
    'inference/core/llf.c',
    'inference/core/tokenizer.c',
    'inference/core/matvec.c',
    'inference/core/engine.c',
    'inference/core/cache.c',
    'inference/core/dist.c',
    'inference/arch/arch.c',
    'inference/arch/llama.c',
    'inference/arch/qwen.c',
    'inference/arch/gemma4.c',
    'inference/arch/qwen35.c',
    'inference/convert/convert.c',
    'inference/convert/convert_safetensors.c',
    'inference/convert/convert_gguf.c',
    'inference/convert/dump.c',
    'inference/vision/vision.c',
    'inference/vision/minicpmv.c',
    'inference/vision/qwen3vl.c',
    'inference/vision/gemma4v.c',
    'inference/device/device_cpu.c',
    # ---- serve（main.c 引用了这些子命令，必须一起编）----
    'serve/rank.c',
    'serve/server.c',
    'serve/router.c',
    'serve/router_http.c',
    'serve/supervisor.c',
    'serve/hub.c',
    'serve/status.c',
    'serve/ctl.c',
    'serve/sync.c',
)
