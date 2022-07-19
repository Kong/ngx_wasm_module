use ngx::*;
use wasi;

macro_rules! test_wasi_assert {
    ($e:expr) => {
        if !($e) {
            resp::ngx_resp_local_reason(500, "assertion failed");
            return;
        }
    };
}

#[no_mangle]
pub fn test_wasi_random_get() {
    let mut v = [0; 10];

    test_wasi_assert!(v.iter().sum::<u8>() == 0);

    if let Ok(()) = unsafe { wasi::random_get(v.as_mut_ptr(), v.len()) } {
        // random_get modifies the buffer
        let sum: u32 = v.iter().map(|&b| b as u32).sum();
        test_wasi_assert!(sum != 0);

        resp::ngx_resp_local_reason(204, "test passed");
    } else {
        resp::ngx_resp_local_reason(500, "test failed");
    }
}

#[no_mangle]
pub fn test_wasi_environ_sizes_get() {
    if let Ok((size, buf_size)) = unsafe { wasi::environ_sizes_get() } {
        // current implementation of environ_sizes_get always returns 0
        test_wasi_assert!(size == 0);
        test_wasi_assert!(buf_size == 0);

        resp::ngx_resp_local_reason(204, "test passed");
    } else {
        resp::ngx_resp_local_reason(500, "test failed");
    }
}

#[no_mangle]
pub fn test_wasi_environ_get() {
    let mut u: u8 = 0;
    let mut environ: [*mut u8; 10] = [&mut u; 10];
    let mut environ_buf: [u8; 10] = [0; 10];

    if let Ok(()) = unsafe { wasi::environ_get(environ.as_mut_ptr(), environ_buf.as_mut_ptr()) } {
        // current implementation of environ_get does not touch the buffers
        test_wasi_assert!(environ.iter().filter(|x| std::ptr::eq(**x, &u)).count() == 10);
        test_wasi_assert!(environ_buf.iter().sum::<u8>() == 0);

        resp::ngx_resp_local_reason(204, "test passed");
    } else {
        resp::ngx_resp_local_reason(500, "test failed");
    }
}
