use ngx::*;

#[no_mangle]
pub fn nop() {}

#[no_mangle]
pub fn log_notice_hello() {
    ngx_log!(Notice, "hello world");
}

#[no_mangle]
pub fn log_resp_status() {
    ngx_log!(Notice, "resp status: {}", ngx_resp_get_status());
}

#[no_mangle]
pub fn set_resp_status() {
    ngx_resp_set_status(201);
}

#[no_mangle]
pub fn say_hello() {
    ngx_resp_say(Some("hello say"));
}

#[no_mangle]
pub fn say_nothing() {
    ngx_resp_say(Some(""));
}

#[no_mangle]
pub fn local_reason() {
    ngx_resp_local(201, Some("REASON"), None);
}

#[no_mangle]
pub fn local_response() {
    ngx_resp_local(200, Some("OK"), Some("hello world"));
}

#[no_mangle]
pub fn discard_local_response() {
    ngx_resp_discard_local();
}
