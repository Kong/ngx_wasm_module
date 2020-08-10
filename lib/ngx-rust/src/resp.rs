extern "C" {
    fn ngx_http_resp_get_status() -> i32;
}

pub fn ngx_resp_get_status() -> i32 {
    unsafe {
        ngx_http_resp_get_status()
    }
}
