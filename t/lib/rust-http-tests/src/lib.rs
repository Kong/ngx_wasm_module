pub mod hostcalls;

use crate::hostcalls::*;

#[no_mangle]
pub fn my_func() {
    ngx_log(6, "with love");
    //here();
}

fn here() {
    let foo = 5;
    let bar = 0;

    //foo / bar;
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
