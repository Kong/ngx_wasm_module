use ngx::*;

#[no_mangle]
pub fn log_err() {
    ngx_log!(Info, "hello world");
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
