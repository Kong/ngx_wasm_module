use ngx_wasm_c_api::*;
use regex::Regex;
use std::mem;
use unescape::unescape;

#[macro_use]
extern crate lazy_static;

fn extract_message(err: &str) -> Option<String> {
    static PAT: &str = r#"Error\(Parse\("[^:]+:[^:]+:[^:]+: error: (.*).*"\)\)$"#;
    lazy_static! {
        static ref RE: Regex = Regex::new(PAT).unwrap();
    }

    let caps = RE.captures(err)?;
    let cap = caps.get(1)?;
    let u = unescape(cap.as_str())?;
    if let Some((first, _)) = u.split_once('\n') {
        Some(first.to_string())
    } else {
        Some(u)
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
/// This function should be called with a valid wat wasm_byte_vec_t pointer.
#[no_mangle]
pub unsafe extern "C" fn ngx_wasm_wat_to_wasm(
    wat: *const wasm_byte_vec_t,
    wasm: *mut wasm_byte_vec_t,
) -> Option<Box<wasm_byte_vec_t>> {
    let empty = Vec::new();
    let mut wat_slice = empty.as_ref();

    unsafe {
        if (*wat).size > 0 {
            wat_slice = std::slice::from_raw_parts((*wat).data, (*wat).size);
        }
    }

    match wabt::wat2wasm(wat_slice) {
        Ok(mut wasm_vec) => {
            wasm_vec.shrink_to_fit();
            vec_into_wasm_byte_vec_t(wasm, wasm_vec);
            None
        }
        Err(e) => {
            let err = e.to_string();
            let msg = extract_message(&err).unwrap_or(err);
            let mut bytes = msg.into_bytes();

            let res = Box::new(wasm_byte_vec_t {
                size: bytes.len(),
                data: bytes.as_mut_ptr(),
            });
            mem::forget(bytes);
            Some(res)
        }
    }
}
