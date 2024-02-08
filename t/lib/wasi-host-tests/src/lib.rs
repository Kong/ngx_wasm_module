use ngx::*;
use std::time::{Instant, SystemTime};
use std::ffi::CStr;

macro_rules! test_wasi_assert {
    ($e:expr) => {
        if !($e) {
            resp::ngx_resp_local(500, None, Some("assertion failed"));
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

        resp::ngx_resp_local(200, None, Some("test passed"));
    } else {
        resp::ngx_resp_local(500, None, Some("test failed"));
    }
}

#[no_mangle]
pub fn test_wasi_environ_sizes_get() {
    if let Ok((size, buf_size)) = unsafe { wasi::environ_sizes_get() } {
        resp::ngx_resp_say(Some(&format!("envs: {size}, size: {buf_size}")));
    } else {
        resp::ngx_resp_say(Some("test failed"));
    }
}

#[no_mangle]
pub fn test_wasi_environ_get() {
    let mut u: u8 = 0;

    if let Ok((size, buf_size)) = unsafe { wasi::environ_sizes_get() } {
        let mut environ: Vec<*mut u8> = vec![&mut u; size];
        let mut environ_buf = vec![0; buf_size];
        let mut environ_out: Vec<String> = vec![String::new(); size];

        if let Ok(()) = unsafe { wasi::environ_get(environ.as_mut_ptr(), environ_buf.as_mut_ptr()) }
        {
            for (i, &env) in environ.iter().enumerate() {
                let c_str = unsafe { CStr::from_ptr(env as *const i8) };
                match std::str::from_utf8(c_str.to_bytes()) {
                    Ok(v) => {
                        environ_out[i] = v.to_string();
                    }
                    Err(e) => panic!("Invalid UTF-8 sequence: {}", e),
                };
            };
            let msg: String = environ_out.join("\n");
            resp::ngx_resp_say(Some(&msg));
            return;
        }
    }

    resp::ngx_resp_say(Some("test failed"))
}

#[no_mangle]
pub fn test_wasi_clock_time_get_via_systemtime() {
    // Rust standard library call which uses wasi
    let now = SystemTime::now();
    match now.duration_since(SystemTime::UNIX_EPOCH) {
        Ok(n) => {
            let msg = format!("seconds since Unix epoch: {}", n.as_secs());
            resp::ngx_resp_say(Some(&msg));
        }
        Err(_) => {
            resp::ngx_resp_local(500, None, Some("test failed, bad system time"));
        }
    }
}

#[no_mangle]
pub fn test_wasi_clock_time_get_via_instant() {
    // Rust standard library call which uses wasi
    let mono1 = Instant::now();
    let mono2 = Instant::now();
    if mono2 >= mono1 {
        resp::ngx_resp_local(200, None, Some("test passed"));
    } else {
        resp::ngx_resp_local(500, None, Some("test failed"));
    }
}

#[no_mangle]
pub fn test_wasi_clock_time_get() {
    // direct WASI access
    if let Ok(timestamp) = unsafe { wasi::clock_time_get(wasi::CLOCKID_REALTIME, 0) } {
        let msg = format!("test passed with timestamp: {timestamp}");
        resp::ngx_resp_say(Some(&msg));
    } else {
        resp::ngx_resp_local(500, None, Some("test failed"));
    }
}

#[no_mangle]
pub fn test_wasi_clock_time_get_unsupported() {
    // test an unsupported clock
    if unsafe { wasi::clock_time_get(wasi::CLOCKID_PROCESS_CPUTIME_ID, 0).is_ok() } {
        resp::ngx_resp_local(500, None, Some("test failed"));
    } else {
        resp::ngx_resp_local(200, None, Some("test passed"));
    }
}

#[no_mangle]
pub fn test_wasi_args_sizes_get() {
    if let Ok((size, buf_size)) = unsafe { wasi::args_sizes_get() } {
        // current implementation of args_sizes_get always returns 0
        test_wasi_assert!(size == 0);
        test_wasi_assert!(buf_size == 0);

        resp::ngx_resp_local(200, None, Some("test passed"));
    } else {
        resp::ngx_resp_local(500, None, Some("test failed"));
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

        resp::ngx_resp_local(200, None, Some("test passed"));
    } else {
        resp::ngx_resp_local(500, None, Some("test failed"));
    }
}

#[no_mangle]
pub fn test_wasi_fd_write_via_println() {
    // Rust standard library call which uses wasi
    println!("Hello, println");
    resp::ngx_resp_local(200, None, Some("test passed"));
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
        resp::ngx_resp_local(200, None, Some(&msg));
    } else {
        resp::ngx_resp_local(500, None, Some("test failed"));
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
        resp::ngx_resp_local(500, None, Some("test failed"));
    } else {
        resp::ngx_resp_local(200, None, Some("test passed"));
    }
}

#[no_mangle]
pub fn test_wasi_fd_write_empty_string() {
    let iovs: [wasi::Ciovec; 0] = [];

    // direct WASI access
    if unsafe { wasi::fd_write(1, &iovs).is_ok() } {
        resp::ngx_resp_local(200, None, Some("test passed"));
    } else {
        resp::ngx_resp_local(500, None, Some("test failed"));
    }
}

fn expect_errno<T>(errno: wasi::Errno, r: Result<T, wasi::Errno>) {
    if let Err(e) = r {
        if e == errno {
            resp::ngx_resp_local(200, None, Some("test passed"));
            return;
        }
        ngx_log!(Err, "unexpected errno: {}", e);
    }

    resp::ngx_resp_local(500, None, Some("test failed"));
}

#[no_mangle]
pub fn test_wasi_fd_close() {
    // current stub implementation always returns BADF
    expect_errno(wasi::ERRNO_BADF, unsafe { wasi::fd_close(1) });
}

#[no_mangle]
pub fn test_wasi_fd_fdstat_get() {
    // current stub implementation always returns BADF
    expect_errno(wasi::ERRNO_BADF, unsafe { wasi::fd_fdstat_get(1) });
}

#[no_mangle]
pub fn test_wasi_fd_prestat_get() {
    // current stub implementation always returns BADF
    expect_errno(wasi::ERRNO_BADF, unsafe { wasi::fd_prestat_get(1) });
}

#[no_mangle]
pub fn test_wasi_fd_prestat_dir_name() {
    let mut u = 0;

    // current stub implementation always returns NOTSUP
    expect_errno(wasi::ERRNO_NOTSUP, unsafe {
        wasi::fd_prestat_dir_name(1, &mut u, 0)
    });
}

#[no_mangle]
pub fn test_wasi_fd_read() {
    let iovs: &[wasi::Iovec] = &[];

    // current stub implementation always returns BADF
    expect_errno(wasi::ERRNO_BADF, unsafe { wasi::fd_read(1, iovs) });
}

#[no_mangle]
pub fn test_wasi_fd_seek() {
    // current stub implementation always returns BADF
    expect_errno(wasi::ERRNO_BADF, unsafe {
        wasi::fd_seek(1, 0, wasi::WHENCE_SET)
    });
}

#[no_mangle]
pub fn test_wasi_path_open() {
    // current stub implementation always returns NOTDIR
    expect_errno(wasi::ERRNO_NOTDIR, unsafe {
        wasi::path_open(1, 0, "/tmp", 0, 0, 0, 0)
    });
}
