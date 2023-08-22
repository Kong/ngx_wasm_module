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

impl Drop for ngx_wasm_backtrace_name_table_t {
    fn drop(&mut self) {
        unsafe {
            let mut v =
                Vec::<ngx_wasm_backtrace_name_t>::from_raw_parts(self.table, self.size, self.size);

            for n in v.drain(..) {
                drop(String::from_raw_parts(
                    n.name.data,
                    n.name.size,
                    n.name.size,
                ));
            }
        }
    }
}

fn vec_into_wasm_byte_vec_t(bv: *mut wasm_byte_vec_t, v: Vec<u8>) {
    unsafe {
        // FIXME Update this when vec_into_raw_parts is stabilized
        let mut v = mem::ManuallyDrop::new(v);
        (*bv).size = v.len();
        (*bv).data = v.as_mut_ptr();
    }
}

/// # Safety
///
/// The `wasm` byte vector pointer must not be NULL, and it should contain
/// valid binary WebAssembly.
#[no_mangle]
pub unsafe extern "C" fn ngx_wasm_backtrace_get_name_table(
    wasm: *const wasm_byte_vec_t,
) -> Option<Box<ngx_wasm_backtrace_name_table_t>> {
    let wasm_slice = unsafe {
        eprintln!("INPUT DATA SIZE: {}", (*wasm).size);
        std::slice::from_raw_parts((*wasm).data, (*wasm).size)
    };

    if let Ok(mut table) = get_function_name_table(wasm_slice) {
        table.sort();

        let mut names = Vec::<ngx_wasm_backtrace_name_t>::with_capacity(table.len());
        for (idx, name) in table.drain(..) {
            // FIXME Update this when vec_into_raw_parts is stabilized
            let mut name = mem::ManuallyDrop::new(name);
            let name_t = ngx_wasm_backtrace_name_t {
                idx,
                name: Box::new(wasm_byte_vec_t {
                    size: name.len(),
                    data: name.as_mut_ptr(),
                }),
            };

            names.push(name_t);
        }

        // FIXME Update this when vec_into_raw_parts is stabilized
        let mut names = mem::ManuallyDrop::new(names);
        Some(Box::new(ngx_wasm_backtrace_name_table_t {
            size: names.len(),
            table: names.as_mut_ptr(),
        }))
    } else {
        None
    }
}

/// # Safety
///
/// Argument must be a name table returned by [ngx_wasm_backtrace_get_name_table].
#[no_mangle]
pub unsafe extern "C" fn ngx_wasm_backtrace_free_name_table(
    ptr: *mut ngx_wasm_backtrace_name_table_t,
) {
    drop(Box::from_raw(ptr));
}

/// # Safety
///
/// First argument must point to a byte vector containing a mangled name.
/// Second argument must point to a byte vector with NULL data; both fields
/// are overwritten by this function. The data must be disposed later with
/// [ngx_wasm_backtrace_drop_byte_vec].
#[no_mangle]
pub unsafe extern "C" fn ngx_wasm_backtrace_demangle(
    mangled: *const wasm_byte_vec_t,
    demangled: *mut wasm_byte_vec_t,
) {
    let s: &str = unsafe {
        let slice = std::slice::from_raw_parts((*mangled).data, (*mangled).size);
        std::str::from_utf8_unchecked(slice)
    };

    let bytes = demangle(s).into_bytes();

    vec_into_wasm_byte_vec_t(demangled, bytes);
}

/// # Safety
///
/// The byte vector given must be the one produced by [ngx_wasm_backtrace_demangle].
#[no_mangle]
pub unsafe extern "C" fn ngx_wasm_backtrace_drop_byte_vec(v: *const wasm_byte_vec_t) {
    let s = unsafe { String::from_raw_parts((*v).data, (*v).size, (*v).size) };
    mem::drop(s);
}
