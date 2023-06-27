use ngx::*;

#[no_mangle]
pub fn test_wasi_non_host() {
    // test a WASI function provided by the VM, not the host
    if unsafe { wasi::fd_filestat_get(1).is_ok() } {
        resp::ngx_resp_local(200, None, Some("test passed"));
    } else {
        resp::ngx_resp_local(500, None, Some("test failed"));
    }
}
