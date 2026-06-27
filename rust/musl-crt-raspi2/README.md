# musl-crt-raspi2

这个目录用来验证：

- `raspi2`
- Rust `no_std`
- 目标仍然保持 `musl` 风格
- 入口由 YiYiYa 自己的 musl CRT 提供
- Rust 代码分别测试 `no_std` 和 `std`

## 目标

这条路线的核心是：

- 保留 YiYiYa 已经适配过的 `crt1.o`
- 禁用 Rust toolchain 自带的 self-contained musl CRT
- Rust 不自己提供 `_start`
- Rust 只导出一个 C ABI 的 `main`

## 关键约束

1. 必须是 `#![no_std]` + `#![no_main]`
2. 不要定义 `_start`
3. 由 musl 的 `crt1.o` 调用 Rust 导出的 `main`
4. target 仍然使用 `arm-unknown-linux-musleabi`
5. 必须通过 `-C link-self-contained=no` 禁掉 Rust 自带 CRT
6. 允许链接 `-lc`，但只能保留 YiYiYa/musl 这一套 CRT

## 当前例子内容

- `src/no_std_main.rs`: 当前已跑通的 `no_std + no_main` 版本
- `src/std_main.rs`: 下一步要测试的标准 Rust `std` 版本
- `build.rs`: 链接到 `build/raspi2/armv7-a/debug/musl/lib`
- `.cargo/config.toml`: 保留 `arm-unknown-linux-musleabi`，但通过 `link-self-contained=no` 禁掉 Rust 自带 CRT
- `user-arm-yiyiya-crt.ld`: 使用 YiYiYa 的用户态装载地址

## 构建方式

构建 `no_std` 版本：

```bash
cargo build --bin yiyiya_musl_no_std
```

构建 `std` 版本：

```bash
cargo build --bin yiyiya_musl_std
```

## 预期

如果这条路线成功，说明：

- Rust 可以继续使用 musl target 的 `core/compiler_builtins`
- Rust 可以复用 YiYiYa 当前已经跑通的 musl 启动链
- 这条线更适合作为早期 bring-up 路线

如果失败，常见原因是：

- Rust 代码里仍然自己定义了 `_start`
- 没有正确关闭 `link-self-contained`
- 链接器同时带入了 Rust CRT 和 YiYiYa CRT
- YiYiYa 的 musl sysroot 还缺 `crt1.o/libc.a` 等关键对象
