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
pub fn say_hello() {
    ngx_resp_say("hello say");
}

#[no_mangle]
pub fn local_reason() {
    ngx_resp_local_reason(201, "REASON");
}

#[no_mangle]
pub fn say_nothing() {
    ngx_resp_say("");
}

#[no_mangle]
pub fn set_resp_status() {
    ngx_resp_set_status(201);
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
