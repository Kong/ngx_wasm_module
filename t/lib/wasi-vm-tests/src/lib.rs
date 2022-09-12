use ngx::*;

#[no_mangle]
pub fn test_wasi_non_host() {
    // test a WASI function provided by the VM, not the host
    if unsafe { wasi::fd_filestat_get(1).is_ok() } {
        resp::ngx_resp_local_reason(204, "test passed");
    } else {
        resp::ngx_resp_local_reason(500, "test failed");
    }
}
