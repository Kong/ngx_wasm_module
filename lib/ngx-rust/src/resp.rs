extern "C" {
    fn ngx_http_resp_get_status() -> i32;
    fn ngx_http_resp_set_status(status: i32);
    fn ngx_http_resp_say(body: *const u8, size: i32);
    fn ngx_http_local_response(
        status: i32,
        reason: *const u8,
        reason_len: i32,
        body: *const u8,
        body_len: i32,
    );
}

pub fn ngx_resp_get_status() -> i32 {
    unsafe { ngx_http_resp_get_status() }
}

pub fn ngx_resp_set_status(status: i32) {
    unsafe { ngx_http_resp_set_status(status) }
}

pub fn ngx_resp_say(body: &str) {
    unsafe { ngx_http_resp_say(body.as_ptr(), body.len() as i32) }
}

pub fn ngx_resp_local_reason(status: i32, reason: &str) {
    unsafe { ngx_http_local_response(status, reason.as_ptr(), reason.len() as i32, "".as_ptr(), 0) }
}
