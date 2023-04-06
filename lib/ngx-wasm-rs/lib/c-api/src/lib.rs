#[repr(C)]
pub struct wasm_byte_vec_t {
    pub size: usize,
    pub data: *mut u8,
}
