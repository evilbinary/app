## build

rustc --print target-list

rustup target add i686-unknown-linux-musl

cargo build --target=i686-unknown-linux-musl

cp target/i686-unknown-linux-musl/debug/test-rs .


cargo build --target=arm-unknown-linux-musleabi

cargo build --target=armv7a-none-eabi

cargo build --target arm-unknown-linux-musleabi --verbose


rustc -Z unstable-options --print target-spec-json --target arm-unknown-linux-musleabi > target.json

export RUSTFLAGS="-lmusl -lgcc -nostartfiles -nostdinc -nostdlib  -mcpu=cortex-a7 -mtune=cortex-a7 -mfpu=vfpv4 -mfloat-abi=softfp -Tuser-arm.ld"

https://doc.rust-lang.org/cargo/reference/config.html