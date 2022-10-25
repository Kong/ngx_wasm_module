use ngx::*;
use std::time::{Instant, SystemTime};
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

#[no_mangle]
pub fn test_wasi_clock_time_get_via_systemtime() {
    // Rust standard library call which uses wasi
    let now = SystemTime::now();
    match now.duration_since(SystemTime::UNIX_EPOCH) {
        Ok(n) => {
            let msg = format!("seconds since Unix epoch: {}", n.as_secs());
            resp::ngx_resp_local_reason(204, &msg);
        }
        Err(_) => {
            resp::ngx_resp_local_reason(500, "test failed, bad system time");
        }
    }
}

#[no_mangle]
pub fn test_wasi_clock_time_get_via_instant() {
    // Rust standard library call which uses wasi
    let mono1 = Instant::now();
    let mono2 = Instant::now();
    if mono2 >= mono1 {
        resp::ngx_resp_local_reason(204, "test passed");
    } else {
        resp::ngx_resp_local_reason(500, "test failed");
    }
}

#[no_mangle]
pub fn test_wasi_clock_time_get() {
    // direct WASI access
    if let Ok(timestamp) = unsafe { wasi::clock_time_get(wasi::CLOCKID_REALTIME, 0) } {
        let msg = format!("test passed with timestamp: {timestamp}");
        resp::ngx_resp_local_reason(204, &msg);
    } else {
        resp::ngx_resp_local_reason(500, "test failed");
    }
}

#[no_mangle]
pub fn test_wasi_clock_time_get_unsupported() {
    // test an unsupported clock
    if unsafe { wasi::clock_time_get(wasi::CLOCKID_PROCESS_CPUTIME_ID, 0).is_ok() } {
        resp::ngx_resp_local_reason(500, "test failed");
    } else {
        resp::ngx_resp_local_reason(204, "test passed, clock is unsupported");
    }
}

#[no_mangle]
pub fn test_wasi_args_sizes_get() {
    if let Ok((size, buf_size)) = unsafe { wasi::args_sizes_get() } {
        // current implementation of args_sizes_get always returns 0
        test_wasi_assert!(size == 0);
        test_wasi_assert!(buf_size == 0);

        resp::ngx_resp_local_reason(204, "test passed");
    } else {
        resp::ngx_resp_local_reason(500, "test failed");
    }
}

#[no_mangle]
pub fn test_wasi_args_get() {
    let mut u: u8 = 0;
    let mut args: [*mut u8; 10] = [&mut u; 10];
    let mut args_buf: [u8; 10] = [0; 10];

    if let Ok(()) = unsafe { wasi::args_get(args.as_mut_ptr(), args_buf.as_mut_ptr()) } {
        // current implementation of args_get does not touch the buffers
        test_wasi_assert!(args.iter().filter(|x| std::ptr::eq(**x, &u)).count() == 10);
        test_wasi_assert!(args_buf.iter().sum::<u8>() == 0);

        resp::ngx_resp_local_reason(204, "test passed");
    } else {
        resp::ngx_resp_local_reason(500, "test failed");
    }
}

#[no_mangle]
pub fn test_wasi_fd_write_via_println() {
    // Rust standard library call which uses wasi
    println!("Hello, println");
    resp::ngx_resp_local_reason(204, "test passed");
}

fn test_fd_write(fd: wasi::Fd) {
    let iovs = [
        wasi::Ciovec {
            buf: "hello".as_ptr(),
            buf_len: 5,
        },
        wasi::Ciovec {
            buf: ", ".as_ptr(),
            buf_len: 2,
        },
        wasi::Ciovec {
            buf: "fd_write".as_ptr(),
            buf_len: 8,
        },
    ];

    // direct WASI access
    if let Ok(n) = unsafe { wasi::fd_write(fd, &iovs) } {
        let msg = format!("test passed, wrote {n} bytes");
        resp::ngx_resp_local_reason(204, &msg);
    } else {
        resp::ngx_resp_local_reason(500, "test failed");
    }
}

#[no_mangle]
pub fn test_wasi_fd_write_stdout() {
    test_fd_write(1);
}

#[no_mangle]
pub fn test_wasi_fd_write_stderr() {
    test_fd_write(2);
}

#[no_mangle]
pub fn test_wasi_fd_write_unsupported_fd() {
    let iovs = [
        wasi::Ciovec {
            buf: "hello".as_ptr(),
            buf_len: 5,
        },
        wasi::Ciovec {
            buf: ", ".as_ptr(),
            buf_len: 2,
        },
        wasi::Ciovec {
            buf: "fd_write".as_ptr(),
            buf_len: 8,
        },
    ];

    if unsafe { wasi::fd_write(999, &iovs).is_ok() } {
        resp::ngx_resp_local_reason(500, "test failed");
    } else {
        resp::ngx_resp_local_reason(204, "test passed, unsupported fd");
    }
}

#[no_mangle]
pub fn test_wasi_fd_write_empty_string() {
    let iovs: [wasi::Ciovec; 0] = [];

    // direct WASI access
    if unsafe { wasi::fd_write(1, &iovs).is_ok() } {
        resp::ngx_resp_local_reason(204, "test passed");
    } else {
        resp::ngx_resp_local_reason(500, "test failed");
    }
}
