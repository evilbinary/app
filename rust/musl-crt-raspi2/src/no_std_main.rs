#![no_std]
#![no_main]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

extern "C" {
    fn printf(fmt: *const i8, ...) -> i32;
}

static HELLO: &[u8] = b"hello from yiyiya musl crt on raspi2\n\0";

#[no_mangle]
pub extern "C" fn abort() -> ! {
    loop {}
}

#[no_mangle]
pub extern "C" fn main(_argc: i32, _argv: *const *const u8) -> i32 {
    unsafe {
        printf(HELLO.as_ptr() as *const i8);
    }
    0
}
