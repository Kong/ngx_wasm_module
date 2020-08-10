#[macro_export]
macro_rules! ngx_log {
    ($lvl:ident, $fmt:expr, $($arg:tt)*) => {
        let msg = format!("{}", format_args!($fmt, $($arg)*));
        ngx_log_err(NgxLogErrLvl::$lvl, msg);
    };
    ($lvl:ident, $($arg:tt)*) => {
        let msg = format!("{}", format_args!("{}", $($arg)*));
        ngx_log_err(NgxLogErrLvl::$lvl, msg);
    };
}

pub enum NgxLogErrLvl {
    Stderr,
    Emerg,
    Alert,
    Crit,
    Err,
    Warn,
    Notice,
    Info,
    Debug,
}

extern "C" {
    fn ngx_log(level: u32, msg: *const u8, size: i32);
}

pub fn ngx_log_err(level: NgxLogErrLvl, msg: String) {
    let lvl: u32 = match level {
        NgxLogErrLvl::Stderr => 0,
        NgxLogErrLvl::Emerg => 1,
        NgxLogErrLvl::Alert => 2,
        NgxLogErrLvl::Crit => 3,
        NgxLogErrLvl::Err => 4,
        NgxLogErrLvl::Warn => 5,
        NgxLogErrLvl::Notice => 6,
        NgxLogErrLvl::Info => 7,
        NgxLogErrLvl::Debug => 8,
    };

    unsafe {
        ngx_log(lvl, msg.as_ptr(), msg.len() as i32)
    }
}

