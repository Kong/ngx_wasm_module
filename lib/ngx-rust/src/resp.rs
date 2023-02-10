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
    fn ngx_http_discard_local_response();
}

pub fn ngx_resp_get_status() -> i32 {
    unsafe { ngx_http_resp_get_status() }
}

pub fn ngx_resp_set_status(status: i32) {
    unsafe { ngx_http_resp_set_status(status) }
}

pub fn ngx_resp_say(body: Option<&str>) {
    unsafe {
        ngx_http_resp_say(
            body.unwrap_or_default().as_ptr(),
            body.unwrap_or_default().len() as i32,
        )
    }
}

pub fn ngx_resp_local(status: i32, reason: Option<&str>, body: Option<&str>) {
    unsafe {
        ngx_http_local_response(
            status,
            reason.unwrap_or_default().as_ptr(),
            reason.unwrap_or_default().len() as i32,
            body.unwrap_or_default().as_ptr(),
            body.unwrap_or_default().len() as i32,
        )
    }
}

pub fn ngx_resp_discard_local() {
    unsafe { ngx_http_discard_local_response() }
}
