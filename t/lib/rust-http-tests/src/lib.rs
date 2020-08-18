use ngx::*;

#[no_mangle]
pub fn nop() {

}

#[no_mangle]
pub fn log_notice_hello() {
    ngx_log!(Notice, "hello world");
}

#[no_mangle]
pub fn log_resp_status() {
    ngx_log!(Notice, "resp status: {}", ngx_resp_get_status());
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
