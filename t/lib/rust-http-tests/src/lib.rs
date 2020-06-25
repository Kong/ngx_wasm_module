pub mod hostcalls;

use crate::hostcalls::*;

#[no_mangle]
pub fn my_func() {
    //ngx_log(1, "with love");
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
