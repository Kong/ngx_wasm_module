extern "C" {
    fn ngx_http_resp_get_status() -> i32;
}

pub fn ngx_resp_get_status() -> i32 {
    unsafe {
        ngx_http_resp_get_status()
    }
}

extern "C" {
    fn ngx_http_resp_say(body: *const u8, size: i32);
}

pub fn ngx_resp_say(body: String) {
    unsafe {
        ngx_http_resp_say(body.as_ptr(), body.len() as i32)
    }
}
