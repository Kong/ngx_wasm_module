use ngx::*;

#[no_mangle]
pub fn log_err() {
    ngx_log!(Info, "hello world");
}

#[no_mangle]
pub fn get_resp_status() {
    ngx_log!(Info, "resp status: {}", ngx_resp_get_status());
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
