pub mod hostcalls;

use crate::hostcalls::*;

#[no_mangle]
pub fn _start() {
    ngx_log(6, "with love");
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
