/*!
Exposed C functions and associated operations to get data in and out
the C environment. All `extern "C"` and `unsafe` operations from
this crate happen only in this module.
*/

use crate::backtrace::*;
use ngx_wasm_c_api::*;
use std::mem;

#[repr(C)]
pub struct ngx_wasm_backtrace_name_t {
    pub idx: u32,
    pub name: Box<wasm_byte_vec_t>,
}

#[repr(C)]
pub struct ngx_wasm_backtrace_name_table_t {
    pub size: usize,
    pub table: *mut ngx_wasm_backtrace_name_t,
}

fn vec_into_wasm_byte_vec_t(bv: *mut wasm_byte_vec_t, v: Vec<u8>) -> () {
    unsafe {
        let (ptr, len, _cap) = v.into_raw_parts();
        (*bv).size = len;
        (*bv).data = ptr;
    }
}

#[no_mangle]
pub extern "C" fn ngx_wasm_backtrace_get_function_name_table(
    wasm: *mut wasm_byte_vec_t,
) -> Option<Box<ngx_wasm_backtrace_name_table_t>> {
    let wasm_slice = unsafe {
        eprintln!("INPUT DATA SIZE: {}", (*wasm).size);
        std::slice::from_raw_parts((*wasm).data, (*wasm).size)
    };

    if let Ok(mut table) = get_function_name_table(wasm_slice) {
        table.sort();

        let mut names = Vec::<ngx_wasm_backtrace_name_t>::with_capacity(table.len());
        for (idx, name) in table.drain(..) {
            let (bytes, len, _cap) = name.into_raw_parts();
            mem::forget(bytes);
            let name_t = ngx_wasm_backtrace_name_t {
                idx: idx,
                name: Box::new(wasm_byte_vec_t {
                    size: len,
                    data: bytes,
                }),
            };

            names.push(name_t);
        }

        let (bytes, len, _cap) = names.into_raw_parts();
        //mem::forget(bytes);
        Some(Box::new(ngx_wasm_backtrace_name_table_t {
            size: len,
            table: bytes,
        }))
    } else {
        None
    }
}

#[no_mangle]
pub extern "C" fn ngx_wasm_backtrace_free_function_name_table(v: *mut u8, size: usize) -> () {
    let s = unsafe { Vec::<u8>::from_raw_parts(v, size, size) };
    mem::drop(s);
}

#[no_mangle]
pub extern "C" fn ngx_wasm_backtrace_demangle(
    mangled: *const wasm_byte_vec_t,
    demangled: *mut wasm_byte_vec_t,
) -> () {
    let s: &str = unsafe {
        let slice = std::slice::from_raw_parts((*mangled).data, (*mangled).size);
        std::str::from_utf8_unchecked(slice)
    };

    let bytes = demangle(s).into_bytes();

    vec_into_wasm_byte_vec_t(demangled, bytes);
}

#[no_mangle]
pub extern "C" fn ngx_wasm_backtrace_drop_byte_vec(v: *const wasm_byte_vec_t) -> () {
    let s = unsafe { String::from_raw_parts((*v).data, (*v).size, (*v).size) };
    mem::drop(s);
}
