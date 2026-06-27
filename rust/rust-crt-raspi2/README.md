# rust-crt-raspi2

这个例子用来验证：

- `raspi2`
- `arm-unknown-linux-musleabi`
- Rust target 自带 CRT
- Rust `std` 启动链

## 目标

这条路线的核心是：

- 让 Rust target 自己提供 `_start/crt1.o`
- 不再链接 YiYiYa 自己的 `crt1.o`
- 只把 YiYiYa 当成“尽量接近 Linux/musl 的运行环境”

## 关键约束

1. 不能再把你自己的 `crt1.o/crti.o/crtn.o` 拉进来
2. 不要再在链接参数里写 `-nostartfiles`
3. 不要再在 linker script 里写 `STARTUP(crt1.o)`
4. 如果链接器报 `_start` 重定义，说明又混进了 YiYiYa 那套 CRT

## 当前例子内容

- `src/main.rs`: 标准 Rust `fn main()`
- `.cargo/config.toml`: 只保留目标和链接脚本
- `user-arm-rust-crt.ld`: 不主动指定 CRT，只控制装载地址

## 预期

如果这条路线成功，说明：

- YiYiYa 当前 `raspi2` 用户态启动 ABI 已经足够接近 Rust target 默认假设
- 后续可以继续补 `std` 运行所需的 TLS / libc 兼容

如果失败，常见原因是：

- 仍混入了 YiYiYa 自己的 CRT
- target 的 `std`/TLS/runtime 需求高于当前 YiYiYa 支持
