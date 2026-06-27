use std::env;
use std::process::Command;

fn main() {
    println!("cargo:rustc-link-search=native=../../../build/raspi2/armv7-a/debug/musl/lib/");
    println!("cargo:rustc-link-lib=static=c");

    let rustc = env::var("RUSTC").unwrap_or_else(|_| "rustc".to_string());
    let output = Command::new(rustc)
        .args(["--print", "sysroot"])
        .output()
        .expect("failed to run rustc --print sysroot");

    let sysroot = String::from_utf8(output.stdout).expect("sysroot is not utf8");
    let sysroot = sysroot.trim();
    let target = env::var("TARGET").expect("TARGET not set");

    let self_contained = format!(
        "{}/lib/rustlib/{}/lib/self-contained",
        sysroot, target
    );
    println!("cargo:rustc-link-search=native={}", self_contained);
}
