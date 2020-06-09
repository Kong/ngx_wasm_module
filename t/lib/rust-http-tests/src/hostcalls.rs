extern "C" {
    fn ngx_wasm_log(level: u32, msg: *const u8, size: i32);
}

pub fn ngx_log(level: u32, msg: &str) {
    unsafe {
        ngx_wasm_log(level, msg.as_ptr(), msg.len() as i32);
    }
}
